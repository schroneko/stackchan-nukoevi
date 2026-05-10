import hashlib
import re
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
SOURCE_DIR = ROOT / "source-assets"
ASSET = ROOT / "assets" / "nukoevi_motion.c"
RECONSTRUCTED_LATEST = SOURCE_DIR / "nukoevi-motion-from-c-asset-latest.txt"
FRAME_WIDTH = 320
FRAME_HEIGHT = 240

FRAMES = [
    ("talk", "nukoevi_talk_open", 2),
    ("sleep", "nukoevi_sleep_drowsy", 0),
    ("sleep", "nukoevi_sleep_asleep", 3),
]


def rgb565_bytes(image):
    raw = image.convert("RGB").tobytes()
    data = bytearray()
    for index in range(0, len(raw), 3):
        r, g, b = raw[index], raw[index + 1], raw[index + 2]
        value = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
        data.append(value & 0xFF)
        data.append((value >> 8) & 0xFF)
    return bytes(data)


def decode_rgb565(data):
    pixels = []
    for index in range(0, len(data), 2):
        value = data[index] | (data[index + 1] << 8)
        r = ((value >> 11) & 0x1F) << 3
        g = ((value >> 5) & 0x3F) << 2
        b = (value & 0x1F) << 3
        pixels.append((r, g, b))

    image = Image.new("RGB", (FRAME_WIDTH, FRAME_HEIGHT))
    image.putdata(pixels)
    return image


def asset_bytes(asset_text, symbol):
    match = re.search(rf"uint8_t {symbol}_map\[\] = \{{\n(.*?)\n\}};", asset_text, re.S)
    if not match:
        raise RuntimeError(f"{symbol}_map was not found")
    values = re.findall(r"0x([0-9a-fA-F]{2})", match.group(1))
    return bytes(int(value, 16) for value in values)


def main():
    text = ASSET.read_text(encoding="utf-8")
    reconstructed = []
    matches = []

    for sequence, symbol, frame_index in FRAMES:
        data = asset_bytes(text, symbol)
        frame = Image.open(SOURCE_DIR / f"nukoevi-{sequence}-frame-{frame_index}.png").convert("RGB")
        matches.append(data == rgb565_bytes(frame))
        reconstructed.append(decode_rgb565(data))

    preview = Image.new("RGB", (FRAME_WIDTH * len(reconstructed), FRAME_HEIGHT))
    for index, frame in enumerate(reconstructed):
        x = index * FRAME_WIDTH
        y = 0
        preview.paste(frame, (x, y))

    digest = hashlib.sha256(preview.tobytes()).hexdigest()[:12]
    out = SOURCE_DIR / f"nukoevi-motion-from-c-asset-{digest}.png"
    preview.save(out)
    RECONSTRUCTED_LATEST.write_text(str(out) + "\n", encoding="utf-8")
    print(f"matches={all(matches)}")
    print(out)


if __name__ == "__main__":
    main()
