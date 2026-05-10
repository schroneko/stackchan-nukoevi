from pathlib import Path

from PIL import Image, ImageFilter


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "source-assets" / "nukoevi-source.png"
OUT_DIR = ROOT / "source-assets"


def blurred_background(source):
    scale = max(320 / source.width, 240 / source.height)
    background = source.resize((round(source.width * scale), round(source.height * scale)), Image.Resampling.LANCZOS)
    left = (background.width - 320) // 2
    top = (background.height - 240) // 2
    background = background.crop((left, top, left + 320, top + 240))
    return background.filter(ImageFilter.GaussianBlur(16))


def main():
    source = Image.open(SOURCE).convert("RGB")

    face_crop = source.crop((0, 0, source.width, round(source.width * 3 / 4)))
    face_crop = face_crop.resize((320, 240), Image.Resampling.LANCZOS)
    face_crop.save(OUT_DIR / "nukoevi-stackchan-face-crop.png")

    for size, name, x, y in [
        (250, "nukoevi-stackchan-face-safe.png", 35, -5),
        (260, "nukoevi-stackchan-face-balanced.png", 30, -8),
    ]:
        background = blurred_background(source)
        foreground = source.resize((size, size), Image.Resampling.LANCZOS)
        background.paste(foreground, (x, y))
        background.save(OUT_DIR / name)


if __name__ == "__main__":
    main()
