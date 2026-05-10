from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
SOURCE_DIR = ROOT / "source-assets"
ASSET_DIR = ROOT / "assets"
OUTPUT = ASSET_DIR / "nukoevi_motion.c"
FRAME_WIDTH = 320
FRAME_HEIGHT = 240
FRAME_ASPECT = FRAME_WIDTH / FRAME_HEIGHT

SEQUENCES = [
    (
        "talk",
        SOURCE_DIR / "nukoevi-talk-imagegen-strip.png",
    ),
    (
        "sleep",
        SOURCE_DIR / "nukoevi-sleep-imagegen-strip.png",
    ),
]

EMBEDDED_FRAMES = [
    ("talk", 2, "nukoevi_talk_open", "NUKOEVI_TALK_OPEN"),
    ("sleep", 0, "nukoevi_sleep_drowsy", "NUKOEVI_SLEEP_DROWSY"),
    ("sleep", 3, "nukoevi_sleep_asleep", "NUKOEVI_SLEEP_ASLEEP"),
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
        crop_height = round(frame.width / FRAME_ASPECT)
        top = round((frame.height - crop_height) / 2)
        frame = frame.crop((0, top, frame.width, top + crop_height))

    return frame.resize((FRAME_WIDTH, FRAME_HEIGHT), Image.Resampling.LANCZOS)


def split_strip(source):
    image = Image.open(source).convert("RGB")
    strip = image.crop(content_bbox(image))
    panel_width = strip.width / 4
    frames = []

    for index in range(4):
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


def c_array(name, attr, frame):
    data = rgb565_bytes(frame)
    lines = [
        f"#ifndef LV_ATTRIBUTE_IMAGE_{attr}",
        f"#define LV_ATTRIBUTE_IMAGE_{attr}",
        "#endif",
        "",
        f"const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST LV_ATTRIBUTE_IMAGE_{attr} uint8_t {name}_map[] = {{",
    ]

    for offset in range(0, len(data), 16):
        row = ", ".join(f"0x{byte:02x}" for byte in data[offset : offset + 16])
        lines.append(f"    {row},")

    lines.extend(
        [
            "};",
            "",
            f"const lv_image_dsc_t {name} = {{",
            "    .header.magic = LV_IMAGE_HEADER_MAGIC,",
            "    .header.cf    = LV_COLOR_FORMAT_RGB565,",
            f"    .header.w     = {FRAME_WIDTH},",
            f"    .header.h     = {FRAME_HEIGHT},",
            f"    .data_size    = sizeof({name}_map),",
            f"    .data         = {name}_map,",
            "};",
            "",
        ]
    )
    return "\n".join(lines)


def write_preview(sequence_name, frames):
    preview = Image.new("RGB", (FRAME_WIDTH * len(frames), FRAME_HEIGHT))
    for index, frame in enumerate(frames):
        frame.save(SOURCE_DIR / f"nukoevi-{sequence_name}-frame-{index}.png")
        preview.paste(frame, (FRAME_WIDTH * index, 0))
    preview.save(SOURCE_DIR / f"nukoevi-{sequence_name}-preview.png")


def main():
    ASSET_DIR.mkdir(parents=True, exist_ok=True)
    source = [
        "#ifdef __has_include",
        '#if __has_include("lvgl.h")',
        "#ifndef LV_LVGL_H_INCLUDE_SIMPLE",
        "#define LV_LVGL_H_INCLUDE_SIMPLE",
        "#endif",
        "#endif",
        "#endif",
        "",
        "#if defined(LV_LVGL_H_INCLUDE_SIMPLE)",
        '#include "lvgl.h"',
        "#else",
        '#include "lvgl/lvgl.h"',
        "#endif",
        "",
        "#ifndef LV_ATTRIBUTE_MEM_ALIGN",
        "#define LV_ATTRIBUTE_MEM_ALIGN",
        "#endif",
        "",
    ]

    generated_frames = {}
    for sequence_name, strip_source in SEQUENCES:
        frames = split_strip(strip_source)
        write_preview(sequence_name, frames)
        generated_frames[sequence_name] = frames

    for sequence_name, frame_index, name, attr in EMBEDDED_FRAMES:
        source.append(c_array(name, attr, generated_frames[sequence_name][frame_index]))

    OUTPUT.write_text("\n".join(source), encoding="utf-8")


if __name__ == "__main__":
    main()
