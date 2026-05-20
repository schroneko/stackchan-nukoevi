#!/usr/bin/env python3
import argparse
import base64
import json
import os
import shlex
import shutil
import subprocess
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path


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
    def __init__(self, target, source, state_dir, evictl_config, queue_only, response_timeout, remote_host, remote_evictl):
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

    def handle(self, payload):
        cmd = payload.get("cmd")
        data = payload.get("data") or {}
        if cmd == "cameraStart":
            request_id = str(data.get("id") or "")
            size = int(data.get("size") or 0)
            chunks = int(data.get("chunks") or 0)
            mime = str(data.get("mime") or "image/jpeg")
            if not request_id or chunks <= 0:
                return 400, {"ok": False, "text": "invalid cameraStart"}
            self.requests[request_id] = CaptureState(request_id, size, chunks, mime)
            return 200, {"ok": True, "text": "capture started"}
        if cmd == "cameraChunk":
            request_id = str(data.get("id") or "")
            state = self.requests.get(request_id)
            if state is None:
                return 404, {"ok": False, "text": "unknown capture"}
            index = int(data.get("index") or 0)
            encoded = str(data.get("data") or "")
            try:
                state.add_part(index, base64.b64decode(encoded, validate=True))
            except Exception as exc:
                return 400, {"ok": False, "text": f"invalid chunk: {exc}"}
            return 200, {"ok": True, "text": "chunk received"}
        if cmd == "chatPrompt":
            request_id = str(data.get("id") or "")
            text = str(data.get("text") or "")
            state = self.requests.pop(request_id, None)
            image_path = None
            if state is not None:
                if not state.complete():
                    return 409, {"ok": False, "text": "capture incomplete"}
                image_path = self.write_image(state)
            return self.send_to_evictl(request_id, text, image_path)
        return 400, {"ok": False, "text": "unknown command"}

    def write_image(self, state):
        suffix = ".jpg" if state.mime == "image/jpeg" else ".bin"
        path = self.state_dir / f"stackchan-camera-{state.request_id}-{int(time.time())}{suffix}"
        data = state.image_bytes()
        if state.size and len(data) != state.size:
            raise ValueError(f"image size mismatch: expected {state.size}, got {len(data)}")
        path.write_bytes(data)
        return path

    def send_to_evictl(self, request_id, text, image_path):
        response_path = self.response_path(request_id)
        self.delete_response(response_path)
        body = text.strip()
        if image_path is not None:
            remote_image_path = self.copy_image_to_remote(image_path)
            if remote_image_path is None:
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
            return 502, {"ok": False, "text": f"evictl failed: {exc}"}

        detail = (result.stdout or result.stderr).strip()
        if result.returncode != 0:
            return 502, {"ok": False, "text": "evictl send failed", "detail": detail}
        response_text = self.wait_response(response_path)
        if response_text:
            return 200, {"ok": True, "text": response_text, "detail": detail, "imagePath": str(image_path) if image_path else None}
        return 200, {"ok": True, "text": "sent to evictl", "detail": detail, "imagePath": str(image_path) if image_path else None}

    def run_evictl(self, args):
        if self.remote_host and self.remote_evictl:
            remote_args = [*shlex.split(self.remote_evictl), *args]
            remote_cmd = " ".join(shlex.quote(part) for part in remote_args)
            return subprocess.run(["ssh", self.remote_host, remote_cmd], check=False, capture_output=True, text=True, timeout=30)
        return subprocess.run([*evictl_command(), *args], check=False, capture_output=True, text=True, timeout=30)

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
            return None
        copy_result = subprocess.run(
            ["scp", str(image_path), f"{self.remote_host}:{image_path}"],
            check=False,
            capture_output=True,
            text=True,
            timeout=30,
        )
        if copy_result.returncode != 0:
            return None
        return image_path

    def delete_response(self, path):
        if path.exists():
            path.unlink()
        if self.remote_host:
            subprocess.run(
                ["ssh", self.remote_host, "rm", "-f", str(path)],
                check=False,
                capture_output=True,
                text=True,
                timeout=10,
            )

    def response_path(self, request_id):
        response_dir = self.state_dir / "responses"
        response_dir.mkdir(parents=True, exist_ok=True)
        return response_dir / f"stackchan-camera-{request_id}.json"

    def wait_response(self, path):
        deadline = time.time() + self.response_timeout
        while time.time() < deadline:
            raw = self.read_response(path)
            if raw:
                try:
                    data = json.loads(raw)
                    return str(data.get("text") or data.get("message") or "").strip()
                except Exception:
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


def make_handler(bridge):
    class Handler(BaseHTTPRequestHandler):
        def do_GET(self):
            if self.path.rstrip("/") == "/health":
                self.send_json(200, {"ok": True})
                return
            self.send_json(404, {"ok": False, "text": "not found"})

        def do_POST(self):
            try:
                length = int(self.headers.get("Content-Length") or "0")
                payload = json.loads(self.rfile.read(length).decode("utf-8"))
                status, response = bridge.handle(payload)
            except Exception as exc:
                status, response = 500, {"ok": False, "text": str(exc)}
            self.send_json(status, response)

        def send_json(self, status, payload):
            body = json.dumps(payload, separators=(",", ":")).encode("utf-8")
            self.send_response(status)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

        def log_message(self, fmt, *args):
            print(f"{self.address_string()} {fmt % args}")

    return Handler


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default=os.environ.get("NUKOEVI_EVI_BRIDGE_HOST", "0.0.0.0"))
    parser.add_argument("--port", type=int, default=int(os.environ.get("NUKOEVI_EVI_BRIDGE_PORT", "8787")))
    parser.add_argument("--target", default=os.environ.get("EVICTL_TARGET", "nukoevi"))
    parser.add_argument("--source", default=os.environ.get("EVICTL_SOURCE", "stackchan-camera"))
    parser.add_argument("--state-dir", default=os.environ.get("NUKOEVI_EVI_BRIDGE_STATE", str(Path.home() / ".local" / "share" / "stackchan-nukoevi" / "evictl-bridge")))
    parser.add_argument("--evictl-config", default=os.environ.get("EVICTL_CONFIG"))
    parser.add_argument("--remote-host", default=os.environ.get("EVICTL_REMOTE_HOST", ""))
    parser.add_argument("--remote-evictl", default=os.environ.get("EVICTL_REMOTE_EVICTL", ""))
    parser.add_argument("--queue-only", action="store_true", default=os.environ.get("EVICTL_QUEUE_ONLY") == "1")
    parser.add_argument("--response-timeout", type=float, default=float(os.environ.get("NUKOEVI_EVI_RESPONSE_TIMEOUT", "20")))
    args = parser.parse_args()

    bridge = Bridge(args.target, args.source, Path(args.state_dir).expanduser(), args.evictl_config, args.queue_only, args.response_timeout, args.remote_host, args.remote_evictl)
    server = ThreadingHTTPServer((args.host, args.port), make_handler(bridge))
    print(f"nukoevi evictl bridge listening on http://{args.host}:{args.port}")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("nukoevi evictl bridge stopped")


if __name__ == "__main__":
    main()
