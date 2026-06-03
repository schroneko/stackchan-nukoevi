from pathlib import Path
import struct

import lz4.block
from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
FIRMWARE_MAIN = ROOT.parents[1]
SOURCE_DIR = ROOT / "source-assets"
ASSET_BIN_DIR = FIRMWARE_MAIN / "assets" / "assets_bin"
FRAME_WIDTH = 320
FRAME_HEIGHT = 240
FRAME_ASPECT = FRAME_WIDTH / FRAME_HEIGHT
LV_IMAGE_HEADER_MAGIC = 0x19
LV_COLOR_FORMAT_RGB565 = 0x12
LV_IMAGE_FLAGS_COMPRESSED = 0x08
LV_IMAGE_COMPRESS_LZ4 = 0x02

SEQUENCES = [
    (
        "talk",
        SOURCE_DIR / "nukoevi-talk-imagegen-strip.png",
        [
            "nukoevi-talk-closed.bin",
            "nukoevi-talk-tiny.bin",
            "nukoevi-talk-medium.bin",
            "nukoevi-talk-wide.bin",
            "nukoevi-talk-small.bin",
            "nukoevi-talk-smile.bin",
        ],
    ),
    (
        "sleep",
        SOURCE_DIR / "nukoevi-sleep-imagegen-strip.png",
        [
            "nukoevi-sleep-drowsy.bin",
            "nukoevi-sleep-nearly-closed.bin",
            "nukoevi-sleep-nod.bin",
            "nukoevi-sleep-asleep.bin",
            "nukoevi-sleep-wobble.bin",
            "nukoevi-sleep-return.bin",
        ],
    ),
]


def content_bbox(image):
    rgb = image.convert("RGB")
    pixels = rgb.load()
    width, height = rgb.size
    left, top, right, bottom = width, height, -1, -1

    for y in range(height):
        for x in range(width):
            r, g, b = pixels[x, y]
            if min(r, g, b) < 245:
                left = min(left, x)
                top = min(top, y)
                right = max(right, x)
                bottom = max(bottom, y)

    if right < left or bottom < top:
        return (0, 0, width, height)

    return (left, top, right + 1, bottom + 1)


def fit_frame(frame):
    frame = frame.convert("RGB")
    frame_aspect = frame.width / frame.height
    if frame_aspect > FRAME_ASPECT:
        crop_width = round(frame.height * FRAME_ASPECT)
        left = round((frame.width - crop_width) / 2)
        frame = frame.crop((left, 0, left + crop_width, frame.height))
    elif frame_aspect < FRAME_ASPECT:
        base_crop_height = round(frame.width / FRAME_ASPECT)
        crop_height = min(frame.height, round(base_crop_height * 1.05))
        top = round((frame.height - crop_height) * 0.32)
        frame = frame.crop((0, top, frame.width, top + crop_height))
        background = frame.resize(
            (FRAME_WIDTH, round(frame.height * FRAME_WIDTH / frame.width)),
            Image.Resampling.LANCZOS,
        )
        crop_top = max(0, round((background.height - FRAME_HEIGHT) / 2))
        background = background.crop((0, crop_top, FRAME_WIDTH, crop_top + FRAME_HEIGHT))
        foreground_width = round(frame.width * FRAME_HEIGHT / frame.height)
        foreground = frame.resize((foreground_width, FRAME_HEIGHT), Image.Resampling.LANCZOS)
        background.paste(foreground, (round((FRAME_WIDTH - foreground_width) / 2), 0))
        return background

    return frame.resize((FRAME_WIDTH, FRAME_HEIGHT), Image.Resampling.LANCZOS)


def split_strip(source):
    image = Image.open(source).convert("RGB")
    strip = image.crop(content_bbox(image))
    panel_count = 6
    panel_width = strip.width / panel_count
    frames = []

    for index in range(panel_count):
        left = round(panel_width * index)
        right = round(panel_width * (index + 1))
        frames.append(fit_frame(strip.crop((left, 0, right, strip.height))))

    return frames


def rgb565_bytes(image):
    raw = image.convert("RGB").tobytes()
    data = bytearray()
    for index in range(0, len(raw), 3):
        r, g, b = raw[index], raw[index + 1], raw[index + 2]
        value = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
        data.append(value & 0xFF)
        data.append((value >> 8) & 0xFF)
    return data


def write_compressed_bin_asset(name, frame):
    data = rgb565_bytes(frame)
    compressed = lz4.block.compress(bytes(data), store_size=False)
    header = struct.pack(
        "<BBHHHHH",
        LV_IMAGE_HEADER_MAGIC,
        LV_COLOR_FORMAT_RGB565,
        LV_IMAGE_FLAGS_COMPRESSED,
        FRAME_WIDTH,
        FRAME_HEIGHT,
        FRAME_WIDTH * 2,
        0,
    )
    compression_header = struct.pack(
        "<III",
        LV_IMAGE_COMPRESS_LZ4,
        len(compressed),
        len(data),
    )
    (ASSET_BIN_DIR / name).write_bytes(header + compression_header + compressed)


def write_raw_bin_asset(name, frame):
    data = rgb565_bytes(frame)
    header = struct.pack(
        "<BBHHHHH",
        LV_IMAGE_HEADER_MAGIC,
        LV_COLOR_FORMAT_RGB565,
        0,
        FRAME_WIDTH,
        FRAME_HEIGHT,
        0,
        0,
    )
    (ASSET_BIN_DIR / name).write_bytes(header + data)


def write_preview(sequence_name, frames):
    preview = Image.new("RGB", (FRAME_WIDTH * len(frames), FRAME_HEIGHT))
    for index, frame in enumerate(frames):
        frame.save(SOURCE_DIR / f"nukoevi-{sequence_name}-frame-{index}.png")
        preview.paste(frame, (FRAME_WIDTH * index, 0))
    preview.save(SOURCE_DIR / f"nukoevi-{sequence_name}-preview.png")


def main():
    ASSET_BIN_DIR.mkdir(parents=True, exist_ok=True)

    for sequence_name, strip_source, asset_names in SEQUENCES:
        frames = split_strip(strip_source)
        write_preview(sequence_name, frames)
        for asset_name, frame in zip(asset_names, frames):
            if sequence_name == "sleep":
                write_raw_bin_asset(asset_name, frame)
            else:
                write_compressed_bin_asset(asset_name, frame)


if __name__ == "__main__":
    main()
