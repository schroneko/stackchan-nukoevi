from pathlib import Path
import struct
import subprocess


ROOT = Path(__file__).resolve().parents[1]
FIRMWARE_MAIN = ROOT.parents[1]
SOURCE = ROOT / "source-assets" / "nukoevi-bridge-icon-source.png"
ASSET = FIRMWARE_MAIN / "assets" / "assets_bin" / "nukoevi_icon.bin"
WIDTH = 188
HEIGHT = 150
LV_IMAGE_HEADER_MAGIC = 0x19
LV_COLOR_FORMAT_RGB565A8 = 0x14


def load_rgb_bytes():
    result = subprocess.run(
        [
            "magick",
            str(SOURCE),
            "-resize",
            f"{WIDTH}x{HEIGHT}^",
            "-gravity",
            "center",
            "-extent",
            f"{WIDTH}x{HEIGHT}",
            "-depth",
            "8",
            "RGB:-",
        ],
        check=True,
        stdout=subprocess.PIPE,
    )
    return result.stdout


def rgb565_bytes(raw):
    data = bytearray()
    for index in range(0, len(raw), 3):
        r, g, b = raw[index], raw[index + 1], raw[index + 2]
        value = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
        data.append(value & 0xFF)
        data.append((value >> 8) & 0xFF)
    return bytes(data)


def main():
    icon = load_rgb_bytes()
    alpha = bytes([255]) * (WIDTH * HEIGHT)
    header = struct.pack(
        "<BBHHHHH",
        LV_IMAGE_HEADER_MAGIC,
        LV_COLOR_FORMAT_RGB565A8,
        0,
        WIDTH,
        HEIGHT,
        WIDTH * 2,
        0,
    )
    ASSET.write_bytes(header + rgb565_bytes(icon) + alpha)


if __name__ == "__main__":
    main()
