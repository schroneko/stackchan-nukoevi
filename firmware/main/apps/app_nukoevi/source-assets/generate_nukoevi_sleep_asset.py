from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
SOURCE_DIR = ROOT / "source-assets"
ASSET_DIR = ROOT / "assets"
SOURCE = SOURCE_DIR / "nukoevi-sleep-frame-0.png"
OUTPUT = ASSET_DIR / "nukoevi_sleep.c"

FRAME_WIDTH = 320
FRAME_HEIGHT = 240


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


def main():
    frame = Image.open(SOURCE).convert("RGB").resize((FRAME_WIDTH, FRAME_HEIGHT), Image.Resampling.LANCZOS)
    data = rgb565_bytes(frame)
    OUTPUT.write_text(
        "\n".join(
            [
                '#ifdef __has_include\n#if __has_include("lvgl.h")\n#ifndef LV_LVGL_H_INCLUDE_SIMPLE\n#define LV_LVGL_H_INCLUDE_SIMPLE\n#endif\n#endif\n#endif\n',
                '#if defined(LV_LVGL_H_INCLUDE_SIMPLE)\n#include "lvgl.h"\n#else\n#include "lvgl/lvgl.h"\n#endif\n',
                "#ifndef LV_ATTRIBUTE_MEM_ALIGN\n#define LV_ATTRIBUTE_MEM_ALIGN\n#endif\n",
                "#ifndef LV_ATTRIBUTE_IMAGE_NUKOEVI_SLEEP_DROWSY\n#define LV_ATTRIBUTE_IMAGE_NUKOEVI_SLEEP_DROWSY\n#endif\n",
                "const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST LV_ATTRIBUTE_IMAGE_NUKOEVI_SLEEP_DROWSY uint8_t nukoevi_sleep_drowsy_map[] = {\n"
                f"{format_bytes(data)}\n"
                "};\n",
                "const lv_image_dsc_t nukoevi_sleep_drowsy = {\n"
                "    .header.cf    = LV_COLOR_FORMAT_RGB565,\n"
                "    .header.magic = LV_IMAGE_HEADER_MAGIC,\n"
                f"    .header.w     = {FRAME_WIDTH},\n"
                f"    .header.h     = {FRAME_HEIGHT},\n"
                f"    .data_size    = {len(data)},\n"
                "    .data         = nukoevi_sleep_drowsy_map,\n"
                "};\n",
            ]
        ),
        encoding="utf-8",
    )


if __name__ == "__main__":
    main()
