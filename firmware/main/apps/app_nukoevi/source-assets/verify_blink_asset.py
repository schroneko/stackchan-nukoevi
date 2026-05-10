import ast
import hashlib
import re
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
SOURCE_DIR = ROOT / "source-assets"
ASSET = ROOT / "assets" / "nukoevi_screen.c"
RECONSTRUCTED_LATEST = SOURCE_DIR / "nukoevi-blink-from-c-asset-latest.txt"

FRAME_WIDTH = 320
FRAME_HEIGHT = 240
FRAMES = [
    "nukoevi_screen_open",
    "nukoevi_screen_half_a",
    "nukoevi_screen_closed",
    "nukoevi_screen_half_b",
]


def read_map(name):
    text = ASSET.read_text(encoding="utf-8")
    match = re.search(rf"uint8_t {name}_map\[\] = \{{\n(.*?)\n\}};", text, re.S)
    if not match:
        raise RuntimeError(f"{name}_map was not found")
    return bytes(ast.literal_eval("[" + match.group(1).replace("\n", "") + "]"))


def rgb565_bytes(image):
    data = bytearray()
    pixels = image.get_flattened_data() if hasattr(image, "get_flattened_data") else image.getdata()
    for r, g, b in pixels:
        value = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
        data.append(value & 0xFF)
        data.append((value >> 8) & 0xFF)
    return bytes(data)


def rgb565_to_image(data):
    pixels = []
    for index in range(0, len(data), 2):
        value = data[index] | (data[index + 1] << 8)
        r = ((value >> 11) & 0x1F) << 3
        g = ((value >> 5) & 0x3F) << 2
        b = (value & 0x1F) << 3
        pixels.append((r | (r >> 5), g | (g >> 6), b | (b >> 5)))
    image = Image.new("RGB", (FRAME_WIDTH, FRAME_HEIGHT))
    image.putdata(pixels)
    return image


def main():
    reconstructed = []
    all_match = True

    for index, name in enumerate(FRAMES):
        asset_data = read_map(name)
        frame = Image.open(SOURCE_DIR / f"nukoevi-blink-frame-{index}.png").convert("RGB")
        frame_data = rgb565_bytes(frame)
        match = asset_data == frame_data
        all_match = all_match and match
        reconstructed.append(rgb565_to_image(asset_data))
        print(f"{name}_matches={match}")

    preview = Image.new("RGB", (FRAME_WIDTH * len(reconstructed), FRAME_HEIGHT))
    for index, frame in enumerate(reconstructed):
        preview.paste(frame, (FRAME_WIDTH * index, 0))

    digest = hashlib.sha256(preview.tobytes()).hexdigest()[:12]
    out = SOURCE_DIR / f"nukoevi-blink-from-c-asset-{digest}.png"
    preview.save(out)
    RECONSTRUCTED_LATEST.write_text(str(out) + "\n", encoding="utf-8")

    print(f"matches={all_match}")
    print(f"reconstructed_versioned={out}")


if __name__ == "__main__":
    main()
