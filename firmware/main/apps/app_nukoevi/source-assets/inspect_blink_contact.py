from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "source-assets" / "nukoevi-blink-imagegen-contact.png"


def main():
    image = Image.open(SOURCE).convert("RGB")
    pixels = image.load()
    min_x = image.width
    min_y = image.height
    max_x = 0
    max_y = 0

    for y in range(image.height):
        for x in range(image.width):
            r, g, b = pixels[x, y]
            if min(r, g, b) < 248:
                min_x = min(min_x, x)
                min_y = min(min_y, y)
                max_x = max(max_x, x + 1)
                max_y = max(max_y, y + 1)

    content_width = max_x - min_x
    content_height = max_y - min_y
    frame_width = content_width / 4
    print(f"image={image.width}x{image.height}")
    print(f"content_box=({min_x},{min_y},{max_x},{max_y})")
    print(f"content={content_width}x{content_height}")
    print(f"source_frame={frame_width:.1f}x{content_height}")
    print(f"source_frame_aspect={frame_width / content_height:.4f}")
    print(f"target_aspect={320 / 240:.4f}")


if __name__ == "__main__":
    main()
