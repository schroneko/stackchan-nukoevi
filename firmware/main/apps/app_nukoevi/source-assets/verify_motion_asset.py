import hashlib
from pathlib import Path
import struct

import lz4.block
from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
FIRMWARE_MAIN = ROOT.parents[1]
SOURCE_DIR = ROOT / "source-assets"
ASSET_BIN_DIR = FIRMWARE_MAIN / "assets" / "assets_bin"
RECONSTRUCTED_LATEST = SOURCE_DIR / "nukoevi-motion-from-c-asset-latest.txt"
FRAME_WIDTH = 320
FRAME_HEIGHT = 240
LV_IMAGE_FLAGS_COMPRESSED = 0x08
LV_IMAGE_COMPRESS_LZ4 = 0x02

FRAMES = [
    ("talk", "nukoevi-talk-closed.bin", 0),
    ("talk", "nukoevi-talk-tiny.bin", 1),
    ("talk", "nukoevi-talk-medium.bin", 2),
    ("talk", "nukoevi-talk-wide.bin", 3),
    ("talk", "nukoevi-talk-small.bin", 4),
    ("talk", "nukoevi-talk-smile.bin", 5),
    ("sleep", "nukoevi-sleep-drowsy.bin", 0),
    ("sleep", "nukoevi-sleep-nearly-closed.bin", 1),
    ("sleep", "nukoevi-sleep-nod.bin", 2),
    ("sleep", "nukoevi-sleep-asleep.bin", 3),
    ("sleep", "nukoevi-sleep-wobble.bin", 4),
    ("sleep", "nukoevi-sleep-return.bin", 5),
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


def read_asset_pixels(asset_name):
    data = (ASSET_BIN_DIR / asset_name).read_bytes()
    flags = struct.unpack_from("<H", data, 2)[0]
    payload = data[12:]
    if flags & LV_IMAGE_FLAGS_COMPRESSED:
        method, compressed_size, raw_size = struct.unpack_from("<III", payload, 0)
        if method != LV_IMAGE_COMPRESS_LZ4:
            raise ValueError(f"unsupported compression method: {method}")
        compressed = payload[12:12 + compressed_size]
        return lz4.block.decompress(compressed, uncompressed_size=raw_size)
    return payload


def main():
    reconstructed = []
    matches = []

    for sequence, asset_name, frame_index in FRAMES:
        data = read_asset_pixels(asset_name)
        frame = Image.open(SOURCE_DIR / f"nukoevi-{sequence}-frame-{frame_index}.png").convert("RGB")
        matches.append(data == rgb565_bytes(frame))
        reconstructed.append(decode_rgb565(data))

    preview = Image.new("RGB", (FRAME_WIDTH * 6, FRAME_HEIGHT * 2))
    for index, frame in enumerate(reconstructed):
        x = (index % 6) * FRAME_WIDTH
        y = (index // 6) * FRAME_HEIGHT
        preview.paste(frame, (x, y))

    digest = hashlib.sha256(preview.tobytes()).hexdigest()[:12]
    out = SOURCE_DIR / f"nukoevi-motion-from-c-asset-{digest}.png"
    preview.save(out)
    RECONSTRUCTED_LATEST.write_text(str(out) + "\n", encoding="utf-8")
    print(f"matches={all(matches)}")
    print(out)


if __name__ == "__main__":
    main()
