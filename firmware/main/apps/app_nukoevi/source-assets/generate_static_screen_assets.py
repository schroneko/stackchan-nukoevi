from pathlib import Path
import hashlib

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
SOURCE_DIR = ROOT / "source-assets"
ASSET_DIR = ROOT / "assets"
SOURCE = SOURCE_DIR / "nukoevi-no-tail-closeup.png"
OUTPUT = ASSET_DIR / "nukoevi_screen.c"
PREVIEW = SOURCE_DIR / "nukoevi-current-screen-preview.png"
PREVIEW_LATEST = SOURCE_DIR / "nukoevi-current-screen-preview-latest.txt"

FRAME_WIDTH = 320
FRAME_HEIGHT = 240

FRAMES = [
    ("nukoevi_screen_open", "NUKOEVI_SCREEN_OPEN"),
    ("nukoevi_screen_half_a", "NUKOEVI_SCREEN_HALF_A"),
    ("nukoevi_screen_closed", "NUKOEVI_SCREEN_CLOSED"),
    ("nukoevi_screen_half_b", "NUKOEVI_SCREEN_HALF_B"),
]


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


def write_c_file(frame):
    pieces = [
        '#ifdef __has_include\n#if __has_include("lvgl.h")\n#ifndef LV_LVGL_H_INCLUDE_SIMPLE\n#define LV_LVGL_H_INCLUDE_SIMPLE\n#endif\n#endif\n#endif\n',
        '#if defined(LV_LVGL_H_INCLUDE_SIMPLE)\n#include "lvgl.h"\n#else\n#include "lvgl/lvgl.h"\n#endif\n',
        "#ifndef LV_ATTRIBUTE_MEM_ALIGN\n#define LV_ATTRIBUTE_MEM_ALIGN\n#endif\n",
    ]

    for name, attr in FRAMES:
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


def main():
    frame = Image.open(SOURCE).convert("RGB").resize((FRAME_WIDTH, FRAME_HEIGHT), Image.Resampling.LANCZOS)
    frame.save(PREVIEW)
    frame_hash = hashlib.sha256(frame.tobytes()).hexdigest()[:12]
    versioned_preview = SOURCE_DIR / f"nukoevi-preview-{frame_hash}.png"
    frame.save(versioned_preview)
    PREVIEW_LATEST.write_text(str(versioned_preview) + "\n", encoding="utf-8")
    for index in range(4):
        frame.save(SOURCE_DIR / f"nukoevi-blink-frame-{index}.png")
    write_c_file(frame)


if __name__ == "__main__":
    main()
