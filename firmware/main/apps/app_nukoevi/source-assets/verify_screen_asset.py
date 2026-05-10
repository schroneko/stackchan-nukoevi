import ast
import hashlib
import re
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
SOURCE_DIR = ROOT / "source-assets"
ASSET = ROOT / "assets" / "nukoevi_screen.c"
PREVIEW = SOURCE_DIR / "nukoevi-current-screen-preview.png"
RECONSTRUCTED = SOURCE_DIR / "nukoevi-screen-from-c-asset.png"
RECONSTRUCTED_LATEST = SOURCE_DIR / "nukoevi-screen-from-c-asset-latest.txt"


def read_open_map():
    text = ASSET.read_text(encoding="utf-8")
    match = re.search(r"uint8_t nukoevi_screen_open_map\[\] = \{\n(.*?)\n\};", text, re.S)
    if not match:
        raise RuntimeError("nukoevi_screen_open_map was not found")
    values = ast.literal_eval("[" + match.group(1).replace("\n", "") + "]")
    return bytes(values)


def rgb565_to_image(data):
    pixels = []
    for index in range(0, len(data), 2):
        value = data[index] | (data[index + 1] << 8)
        r = ((value >> 11) & 0x1F) << 3
        g = ((value >> 5) & 0x3F) << 2
        b = (value & 0x1F) << 3
        pixels.append((r | (r >> 5), g | (g >> 6), b | (b >> 5)))
    image = Image.new("RGB", (320, 240))
    image.putdata(pixels)
    return image


def main():
    data = read_open_map()
    image = rgb565_to_image(data)
    image.save(RECONSTRUCTED)
    asset_hash = hashlib.sha256(data).hexdigest()[:12]
    versioned_reconstructed = SOURCE_DIR / f"nukoevi-screen-from-c-asset-{asset_hash}.png"
    image.save(versioned_reconstructed)
    RECONSTRUCTED_LATEST.write_text(str(versioned_reconstructed) + "\n", encoding="utf-8")

    preview = Image.open(PREVIEW).convert("RGB")
    preview_rgb565 = bytearray()
    pixels = preview.get_flattened_data() if hasattr(preview, "get_flattened_data") else preview.getdata()
    for r, g, b in pixels:
        value = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
        preview_rgb565.append(value & 0xFF)
        preview_rgb565.append((value >> 8) & 0xFF)

    print(f"asset_bytes_sha256={hashlib.sha256(data).hexdigest()}")
    print(f"preview_rgb565_sha256={hashlib.sha256(bytes(preview_rgb565)).hexdigest()}")
    print(f"matches={data == bytes(preview_rgb565)}")
    print(f"reconstructed={RECONSTRUCTED}")
    print(f"reconstructed_versioned={versioned_reconstructed}")


if __name__ == "__main__":
    main()
