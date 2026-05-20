#!/usr/bin/env python3
import argparse
import base64
import json
import os
import shlex
import shutil
import socket
import subprocess
import threading
import time
import traceback
import uuid
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path


def now_iso():
    return time.strftime("%Y-%m-%dT%H:%M:%S", time.gmtime()) + f".{int((time.time() % 1) * 1000):03d}Z"


def compact(value, limit=4000):
    if value is None:
        return ""
    text = str(value)
    if len(text) <= limit:
        return text
    return text[:limit] + f"...<truncated {len(text) - limit} chars>"


def safe_int(value, default=0):
    try:
        return int(value)
    except (TypeError, ValueError):
        return default


class BridgeLogger:
    def __init__(self, path):
        self.path = path
        self.lock = threading.Lock()
        self.path.parent.mkdir(parents=True, exist_ok=True)

    def event(self, event, **fields):
        record = {"ts": now_iso(), "event": event, **fields}
        line = json.dumps(record, ensure_ascii=False, separators=(",", ":"))
        with self.lock:
            with self.path.open("a", encoding="utf-8") as file:
                file.write(line + "\n")


class CaptureState:
    def __init__(self, request_id, size, chunks, mime):
        self.request_id = request_id
        self.size = size
        self.chunks = chunks
        self.mime = mime
        self.parts = {}
        self.created_at = time.time()

    def add_part(self, index, data):
        self.parts[index] = data

    def complete(self):
        return len(self.parts) == self.chunks

    def image_bytes(self):
        return b"".join(self.parts[index] for index in range(self.chunks))


class Bridge:
    def __init__(self, target, source, state_dir, evictl_config, queue_only, response_timeout, remote_host, remote_evictl, log_file):
        self.target = target
        self.source = source
        self.state_dir = state_dir
        self.evictl_config = evictl_config
        self.queue_only = queue_only
        self.response_timeout = response_timeout
        self.remote_host = remote_host
        self.remote_evictl = remote_evictl
        self.requests = {}
        self.state_dir.mkdir(parents=True, exist_ok=True)
        self.logger = BridgeLogger(log_file)
        self.logger.event(
            "bridge_start",
            target=self.target,
            source=self.source,
            state_dir=str(self.state_dir),
            log_file=str(log_file),
            remote_host=self.remote_host,
            queue_only=self.queue_only,
            response_timeout=self.response_timeout,
        )

    def health(self):
        command = evictl_command()
        return {
            "ok": True,
            "target": self.target,
            "source": self.source,
            "stateDir": str(self.state_dir),
            "evictlCommand": command,
            "evictlAvailable": command_available(command),
            "localAddresses": local_ipv4_addresses(),
            "remoteHost": self.remote_host,
            "queueOnly": self.queue_only,
            "responseTimeout": self.response_timeout,
            "logFile": str(self.logger.path),
        }

    def handle(self, payload, client=""):
        cmd = payload.get("cmd")
        data = payload.get("data") or {}
        request_id = str(data.get("id") or "")
        self.logger.event("request_received", client=client, cmd=cmd, request_id=request_id)
        if cmd == "cameraStart":
            size = safe_int(data.get("size"))
            chunks = safe_int(data.get("chunks"))
            mime = str(data.get("mime") or "image/jpeg")
            if not request_id or chunks <= 0:
                self.logger.event("camera_start_rejected", request_id=request_id, size=size, chunks=chunks, mime=mime)
                return 400, {"ok": False, "text": "invalid cameraStart"}
            self.requests[request_id] = CaptureState(request_id, size, chunks, mime)
            self.logger.event("camera_start_accepted", request_id=request_id, size=size, chunks=chunks, mime=mime)
            return 200, {"ok": True, "text": "capture started"}
        if cmd == "cameraChunk":
            state = self.requests.get(request_id)
            if state is None:
                self.logger.event("camera_chunk_rejected", request_id=request_id, reason="unknown_capture")
                return 404, {"ok": False, "text": "unknown capture"}
            index = safe_int(data.get("index"))
            total = safe_int(data.get("total"))
            encoded = str(data.get("data") or "")
            try:
                decoded = base64.b64decode(encoded, validate=True)
                state.add_part(index, decoded)
            except Exception as exc:
                self.logger.event("camera_chunk_rejected", request_id=request_id, index=index, total=total, reason=str(exc))
                return 400, {"ok": False, "text": f"invalid chunk: {exc}"}
            self.logger.event("camera_chunk_accepted", request_id=request_id, index=index, total=total, bytes=len(decoded), received=len(state.parts))
            return 200, {"ok": True, "text": "chunk received"}
        if cmd == "chatPrompt":
            text = str(data.get("text") or "")
            state = self.requests.pop(request_id, None)
            image_path = None
            if state is not None:
                if not state.complete():
                    self.logger.event("capture_incomplete", request_id=request_id, expected=state.chunks, received=len(state.parts))
                    return 409, {"ok": False, "text": "capture incomplete"}
                image_path = self.write_image(state)
            return self.send_to_evictl(request_id, text, image_path)
        self.logger.event("request_rejected", client=client, cmd=cmd, request_id=request_id, reason="unknown_command")
        return 400, {"ok": False, "text": "unknown command"}

    def write_image(self, state):
        suffix = ".jpg" if state.mime == "image/jpeg" else ".bin"
        path = self.state_dir / f"stackchan-camera-{state.request_id}-{int(time.time())}{suffix}"
        data = state.image_bytes()
        if state.size and len(data) != state.size:
            self.logger.event("image_size_mismatch", request_id=state.request_id, expected=state.size, actual=len(data), path=str(path))
            raise ValueError(f"image size mismatch: expected {state.size}, got {len(data)}")
        path.write_bytes(data)
        self.logger.event("image_written", request_id=state.request_id, path=str(path), bytes=len(data), mime=state.mime)
        return path

    def send_to_evictl(self, request_id, text, image_path):
        response_path = self.response_path(request_id)
        body = text.strip()
        self.logger.event("send_prepare", request_id=request_id, response_path=str(response_path), has_image=image_path is not None, text_bytes=len(body.encode("utf-8")))
        if image_path is not None:
            remote_image_path = self.copy_image_to_remote(image_path)
            if remote_image_path is None:
                self.logger.event("send_failed", request_id=request_id, reason="remote_image_copy_failed", image_path=str(image_path))
                return 502, {"ok": False, "text": "remote image copy failed"}
            image_path = remote_image_path
            body = f"{body}\n\nImage file: {image_path}".strip()
        if self.response_timeout > 0:
            body = f"{body}\n\nWrite the final short response for Stack-chan to this file as JSON: {response_path}\nSchema: {{\"text\":\"...\"}}".strip()
        if not body:
            body = "Stack-chan camera request"

        args = ["send", self.target, "--text", body, "--subject", f"stackchan-camera-{request_id}", "--source", self.source]
        if self.evictl_config:
            args.extend(["--config", self.evictl_config])
        if self.queue_only:
            args.append("--queue-only")

        try:
            result = self.run_evictl(args)
        except Exception as exc:
            self.logger.event("send_exception", request_id=request_id, error=str(exc), traceback=traceback.format_exc())
            return 502, {"ok": False, "text": f"evictl failed: {exc}"}

        detail = (result.stdout or result.stderr).strip()
        self.logger.event(
            "send_result",
            request_id=request_id,
            returncode=result.returncode,
            stdout=compact(result.stdout),
            stderr=compact(result.stderr),
            detail=compact(detail),
        )
        if result.returncode != 0:
            return 502, {"ok": False, "text": "evictl send failed", "detail": detail}
        response_text = self.wait_response(response_path)
        if response_text:
            self.logger.event("response_received", request_id=request_id, response_path=str(response_path), text=compact(response_text))
            return 200, {"ok": True, "text": response_text, "detail": detail, "imagePath": str(image_path) if image_path else None}
        self.logger.event("response_timeout", request_id=request_id, response_path=str(response_path), timeout=self.response_timeout)
        return 200, {"ok": True, "text": "sent to evictl", "detail": detail, "imagePath": str(image_path) if image_path else None}

    def run_evictl(self, args):
        if self.remote_host and self.remote_evictl:
            remote_args = [*shlex.split(self.remote_evictl), *args]
            remote_cmd = " ".join(shlex.quote(part) for part in remote_args)
            command = ["ssh", self.remote_host, remote_cmd]
            self.logger.event("run_evictl_start", mode="remote", command=command)
            return subprocess.run(command, check=False, capture_output=True, text=True, timeout=30)
        command = [*evictl_command(), *args]
        self.logger.event("run_evictl_start", mode="local", command=command)
        return subprocess.run(command, check=False, capture_output=True, text=True, timeout=30)

    def copy_image_to_remote(self, image_path):
        if not self.remote_host:
            return image_path
        remote_dir = str(image_path.parent)
        mkdir_result = subprocess.run(
            ["ssh", self.remote_host, "mkdir", "-p", remote_dir],
            check=False,
            capture_output=True,
            text=True,
            timeout=15,
        )
        if mkdir_result.returncode != 0:
            self.logger.event("remote_mkdir_failed", path=remote_dir, returncode=mkdir_result.returncode, stdout=compact(mkdir_result.stdout), stderr=compact(mkdir_result.stderr))
            return None
        copy_result = subprocess.run(
            ["scp", str(image_path), f"{self.remote_host}:{image_path}"],
            check=False,
            capture_output=True,
            text=True,
            timeout=30,
        )
        if copy_result.returncode != 0:
            self.logger.event("remote_copy_failed", path=str(image_path), returncode=copy_result.returncode, stdout=compact(copy_result.stdout), stderr=compact(copy_result.stderr))
            return None
        self.logger.event("remote_copy_succeeded", path=str(image_path), remote_host=self.remote_host)
        return image_path

    def response_path(self, request_id):
        response_dir = self.state_dir / "responses"
        response_dir.mkdir(parents=True, exist_ok=True)
        token = f"{int(time.time() * 1000)}-{uuid.uuid4().hex[:8]}"
        return response_dir / f"stackchan-camera-{request_id}-{token}.json"

    def wait_response(self, path):
        deadline = time.time() + self.response_timeout
        while time.time() < deadline:
            raw = self.read_response(path)
            if raw:
                try:
                    data = json.loads(raw)
                    return str(data.get("text") or data.get("message") or "").strip()
                except Exception as exc:
                    self.logger.event("response_parse_failed", response_path=str(path), error=str(exc), raw=compact(raw))
                    return raw
            time.sleep(0.25)
        return ""

    def read_response(self, path):
        if not self.remote_host:
            if path.exists() and path.stat().st_size > 0:
                return path.read_text(encoding="utf-8").strip()
            return ""
        probe = subprocess.run(
            ["ssh", self.remote_host, "test", "-s", str(path)],
            check=False,
            capture_output=True,
            text=True,
            timeout=5,
        )
        if probe.returncode != 0:
            return ""
        result = subprocess.run(
            ["ssh", self.remote_host, "cat", str(path)],
            check=False,
            capture_output=True,
            text=True,
            timeout=5,
        )
        if result.returncode != 0:
            self.logger.event("remote_response_read_failed", response_path=str(path), returncode=result.returncode, stdout=compact(result.stdout), stderr=compact(result.stderr))
            return ""
        return result.stdout.strip()


def evictl_command():
    configured = os.environ.get("EVICTL_BIN")
    if configured:
        return configured.split()
    found = shutil.which("evictl")
    if found:
        return [found]
    source_tree = os.environ.get("EVICTL_SOURCE_TREE")
    candidates = []
    if source_tree:
        candidates.append(Path(source_tree) / "src" / "cli.ts")
    ghq_root = Path(os.environ.get("GHQ_ROOT", Path.home() / "ghq"))
    candidates.append(ghq_root / "github.com" / "schroneko" / "evictl" / "src" / "cli.ts")
    for candidate in candidates:
        if candidate.exists() and shutil.which("bun"):
            return ["bun", "run", str(candidate)]
    return ["evictl"]


def command_available(command):
    if not command:
        return False
    executable = command[0]
    if "/" in executable:
        return Path(executable).exists()
    return shutil.which(executable) is not None


def local_ipv4_addresses():
    addresses = []
    try:
        for item in socket.getaddrinfo(socket.gethostname(), None, socket.AF_INET):
            address = item[4][0]
            if address not in addresses and not address.startswith("127."):
                addresses.append(address)
    except OSError:
        pass
    if not addresses:
        probe = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        try:
            probe.connect(("8.8.8.8", 80))
            address = probe.getsockname()[0]
            if not address.startswith("127."):
                addresses.append(address)
        except OSError:
            pass
        finally:
            probe.close()
    return addresses


def make_handler(bridge):
    class Handler(BaseHTTPRequestHandler):
        def do_GET(self):
            if self.path.rstrip("/") == "/health":
                bridge.logger.event("health_requested", client=self.address_string())
                self.send_json(200, bridge.health())
                return
            bridge.logger.event("http_not_found", client=self.address_string(), method="GET", path=self.path)
            self.send_json(404, {"ok": False, "text": "not found"})

        def do_POST(self):
            try:
                length = int(self.headers.get("Content-Length") or "0")
                raw = self.rfile.read(length).decode("utf-8")
                payload = json.loads(raw)
                status, response = bridge.handle(payload, client=self.address_string())
            except json.JSONDecodeError as exc:
                status, response = 400, {"ok": False, "text": f"invalid json: {exc}"}
                bridge.logger.event("http_bad_json", client=self.address_string(), error=str(exc))
            except Exception as exc:
                status, response = 500, {"ok": False, "text": str(exc)}
                bridge.logger.event("http_exception", client=self.address_string(), error=str(exc), traceback=traceback.format_exc())
            bridge.logger.event("http_response", client=self.address_string(), status=status, ok=response.get("ok"), text=compact(response.get("text")))
            self.send_json(status, response)

        def send_json(self, status, payload):
            body = json.dumps(payload, separators=(",", ":")).encode("utf-8")
            self.send_response(status)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

        def log_message(self, fmt, *args):
            message = fmt % args
            bridge.logger.event("http_access", client=self.address_string(), message=message)
            print(f"{self.address_string()} {message}", flush=True)

    return Handler


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default=os.environ.get("NUKOEVI_EVI_BRIDGE_HOST", "0.0.0.0"))
    parser.add_argument("--port", type=int, default=int(os.environ.get("NUKOEVI_EVI_BRIDGE_PORT", "18787")))
    parser.add_argument("--target", default=os.environ.get("EVICTL_TARGET", "nukoevi"))
    parser.add_argument("--source", default=os.environ.get("EVICTL_SOURCE", "stackchan-camera"))
    parser.add_argument("--state-dir", default=os.environ.get("NUKOEVI_EVI_BRIDGE_STATE", str(Path.home() / ".local" / "share" / "stackchan-nukoevi" / "evictl-bridge")))
    parser.add_argument("--evictl-config", default=os.environ.get("EVICTL_CONFIG"))
    parser.add_argument("--remote-host", default=os.environ.get("EVICTL_REMOTE_HOST", ""))
    parser.add_argument("--remote-evictl", default=os.environ.get("EVICTL_REMOTE_EVICTL", ""))
    parser.add_argument("--queue-only", action="store_true", default=os.environ.get("EVICTL_QUEUE_ONLY") == "1")
    parser.add_argument("--response-timeout", type=float, default=float(os.environ.get("NUKOEVI_EVI_RESPONSE_TIMEOUT", "20")))
    parser.add_argument("--log-file", default=os.environ.get("NUKOEVI_EVI_BRIDGE_LOG"))
    args = parser.parse_args()

    state_dir = Path(args.state_dir).expanduser()
    log_file = Path(args.log_file).expanduser() if args.log_file else state_dir / "logs" / "bridge.jsonl"
    bridge = Bridge(args.target, args.source, state_dir, args.evictl_config, args.queue_only, args.response_timeout, args.remote_host, args.remote_evictl, log_file)
    server = ThreadingHTTPServer((args.host, args.port), make_handler(bridge))
    print(f"nukoevi evictl bridge listening on http://{args.host}:{args.port}", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        bridge.logger.event("bridge_stop", reason="keyboard_interrupt")
        print("nukoevi evictl bridge stopped", flush=True)


if __name__ == "__main__":
    main()
