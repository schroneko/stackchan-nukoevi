from pathlib import Path
import struct

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
FIRMWARE_MAIN = ROOT.parents[1]
SOURCE_DIR = ROOT / "source-assets"
ASSET_DIR = ROOT / "assets"
ASSET_BIN_DIR = FIRMWARE_MAIN / "assets" / "assets_bin"
SOURCE = SOURCE_DIR / "nukoevi-blink-imagegen-contact.png"
OUTPUT = ASSET_DIR / "nukoevi_screen.c"
PREVIEW = SOURCE_DIR / "nukoevi-blink-preview.png"

FRAME_WIDTH = 320
FRAME_HEIGHT = 240
FRAME_ASPECT = FRAME_WIDTH / FRAME_HEIGHT
LV_IMAGE_HEADER_MAGIC = 0x19
LV_COLOR_FORMAT_RGB565 = 0x12

FRAMES = [
    ("nukoevi_screen_open", "NUKOEVI_SCREEN_OPEN", "nukoevi-screen-open.bin"),
    ("nukoevi_screen_half_a", "NUKOEVI_SCREEN_HALF_A", "nukoevi-screen-half-a.bin"),
    ("nukoevi_screen_closed", "NUKOEVI_SCREEN_CLOSED", "nukoevi-screen-closed.bin"),
    ("nukoevi_screen_half_b", "NUKOEVI_SCREEN_HALF_B", "nukoevi-screen-half-b.bin"),
]


def content_box(image):
    rgb = image.convert("RGB")
    pixels = rgb.load()
    min_x = rgb.width
    min_y = rgb.height
    max_x = 0
    max_y = 0

    for y in range(rgb.height):
        for x in range(rgb.width):
            r, g, b = pixels[x, y]
            if min(r, g, b) < 248:
                min_x = min(min_x, x)
                min_y = min(min_y, y)
                max_x = max(max_x, x + 1)
                max_y = max(max_y, y + 1)

    return (min_x, min_y, max_x, max_y)


def frame_images():
    source = Image.open(SOURCE).convert("RGB")
    left, top, right, bottom = content_box(source)
    strip = source.crop((left, top, right, bottom))
    frame_width = strip.width / len(FRAMES)
    frames = []

    for index in range(len(FRAMES)):
        frame_left = round(frame_width * index)
        frame_right = round(frame_width * (index + 1))
        frame = strip.crop((frame_left, 0, frame_right, strip.height))
        frame_aspect = frame.width / frame.height
        if frame_aspect < FRAME_ASPECT:
            cropped_height = round(frame.width / FRAME_ASPECT)
            frame = frame.crop((0, 0, frame.width, cropped_height))
        elif frame_aspect > FRAME_ASPECT:
            cropped_width = round(frame.height * FRAME_ASPECT)
            crop_left = round((frame.width - cropped_width) / 2)
            frame = frame.crop((crop_left, 0, crop_left + cropped_width, frame.height))
        frame = frame.resize((FRAME_WIDTH, FRAME_HEIGHT), Image.Resampling.LANCZOS)
        frames.append(frame)

    return frames


def rgb565_bytes(image):
    data = bytearray()
    pixels = image.get_flattened_data() if hasattr(image, "get_flattened_data") else image.getdata()
    for r, g, b in pixels:
        value = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
        data.append(value & 0xFF)
        data.append((value >> 8) & 0xFF)
    return data


def format_bytes(data):
    lines = []
    for index in range(0, len(data), 16):
        chunk = data[index : index + 16]
        lines.append("    " + ", ".join(f"0x{byte:02x}" for byte in chunk) + ",")
    return "\n".join(lines)


def write_c_file(frames):
    pieces = [
        '#ifdef __has_include\n#if __has_include("lvgl.h")\n#ifndef LV_LVGL_H_INCLUDE_SIMPLE\n#define LV_LVGL_H_INCLUDE_SIMPLE\n#endif\n#endif\n#endif\n',
        '#if defined(LV_LVGL_H_INCLUDE_SIMPLE)\n#include "lvgl.h"\n#else\n#include "lvgl/lvgl.h"\n#endif\n',
        "#ifndef LV_ATTRIBUTE_MEM_ALIGN\n#define LV_ATTRIBUTE_MEM_ALIGN\n#endif\n",
    ]

    for (name, attr, _asset_name), frame in zip(FRAMES, frames):
        data = rgb565_bytes(frame)
        pieces.append(f"#ifndef LV_ATTRIBUTE_IMAGE_{attr}\n#define LV_ATTRIBUTE_IMAGE_{attr}\n#endif\n")
        pieces.append(
            f"const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST LV_ATTRIBUTE_IMAGE_{attr} uint8_t {name}_map[] = {{\n"
            f"{format_bytes(data)}\n"
            "};\n"
        )
        pieces.append(
            f"const lv_image_dsc_t {name} = {{\n"
            "    .header.cf    = LV_COLOR_FORMAT_RGB565,\n"
            "    .header.magic = LV_IMAGE_HEADER_MAGIC,\n"
            f"    .header.w     = {FRAME_WIDTH},\n"
            f"    .header.h     = {FRAME_HEIGHT},\n"
            f"    .data_size    = {len(data)},\n"
            f"    .data         = {name}_map,\n"
            "};\n"
        )

    OUTPUT.write_text("\n".join(pieces), encoding="utf-8")


def write_bin_assets(frames):
    ASSET_BIN_DIR.mkdir(parents=True, exist_ok=True)

    for (_name, _attr, asset_name), frame in zip(FRAMES, frames):
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
        (ASSET_BIN_DIR / asset_name).write_bytes(header + data)


def write_preview(frames):
    preview = Image.new("RGB", (FRAME_WIDTH * len(frames), FRAME_HEIGHT))
    for index, frame in enumerate(frames):
        preview.paste(frame, (index * FRAME_WIDTH, 0))
        frame.save(SOURCE_DIR / f"nukoevi-blink-frame-{index}.png")
    preview.save(PREVIEW)


def main():
    frames = frame_images()
    write_preview(frames)
    write_c_file(frames)
    write_bin_assets(frames)


if __name__ == "__main__":
    main()
