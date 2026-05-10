from pathlib import Path

from PIL import Image, ImageFilter


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "source-assets" / "nukoevi-close-source.jpeg"
OUT_DIR = ROOT / "source-assets"


def make_preview(name, size, x, y):
    source = Image.open(SOURCE).convert("RGB")
    scale = max(320 / source.width, 240 / source.height)
    background = source.resize((round(source.width * scale), round(source.height * scale)), Image.Resampling.LANCZOS)
    left = (background.width - 320) // 2
    top = (background.height - 240) // 2
    background = background.crop((left, top, left + 320, top + 240)).filter(ImageFilter.GaussianBlur(14))

    foreground = source.resize((size, size), Image.Resampling.LANCZOS)
    background.paste(foreground, (x, y))
    background.save(OUT_DIR / name)


def main():
    make_preview("nukoevi-close-crop-pendant.png", 252, 34, -12)
    make_preview("nukoevi-close-crop-pendant-tail.png", 248, 38, -8)
    make_preview("nukoevi-close-crop-tight.png", 260, 30, -20)


if __name__ == "__main__":
    main()
