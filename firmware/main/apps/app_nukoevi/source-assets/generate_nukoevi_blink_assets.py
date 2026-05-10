from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
SOURCE_DIR = ROOT / "source-assets"
ASSET_DIR = ROOT / "assets"
SOURCE = SOURCE_DIR / "nukoevi-blink-imagegen.png"
OUTPUT = ASSET_DIR / "nukoevi_screen.c"
PREVIEW = SOURCE_DIR / "nukoevi-blink-preview.png"

FRAME_WIDTH = 320
FRAME_HEIGHT = 240
SOURCE_FRAME_WIDTHS = [443, 444, 443, 444]
SOURCE_FRAME_X = [0, 443, 887, 1330]
SOURCE_FRAME_Y = 300
SOURCE_FRAME_HEIGHT = 333

FRAMES = [
    ("nukoevi_screen_open", "NUKOEVI_SCREEN_OPEN", 0),
    ("nukoevi_screen_half_a", "NUKOEVI_SCREEN_HALF_A", 1),
    ("nukoevi_screen_closed", "NUKOEVI_SCREEN_CLOSED", 2),
    ("nukoevi_screen_half_b", "NUKOEVI_SCREEN_HALF_B", 3),
]


def frame_images():
    source = Image.open(SOURCE).convert("RGB")
    frames = []
    for x, width in zip(SOURCE_FRAME_X, SOURCE_FRAME_WIDTHS):
        frame = source.crop((x, SOURCE_FRAME_Y, x + width, SOURCE_FRAME_Y + SOURCE_FRAME_HEIGHT))
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

    for name, attr, index in FRAMES:
        data = rgb565_bytes(frames[index])
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


def write_preview(frames):
    preview = Image.new("RGB", (FRAME_WIDTH * len(frames), FRAME_HEIGHT))
    for index, frame in enumerate(frames):
        preview.paste(frame, (index * FRAME_WIDTH, 0))
    preview.save(PREVIEW)
    for index, frame in enumerate(frames):
        frame.save(SOURCE_DIR / f"nukoevi-blink-frame-{index}.png")


def main():
    frames = frame_images()
    write_preview(frames)
    write_c_file(frames)


if __name__ == "__main__":
    main()
