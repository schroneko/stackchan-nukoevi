from pathlib import Path

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "source-assets" / "nukoevi-final-source.png"
OUT_DIR = ROOT / "source-assets"

CROPS = [
    ("nukoevi-face-centered-no-tail.png", (330, 40, 1290, 760)),
    ("nukoevi-face-tail-safe.png", (0, 60, 1120, 900)),
    ("nukoevi-face-large.png", (190, 35, 1250, 830)),
    ("nukoevi-face-larger.png", (245, 40, 1210, 764)),
    ("nukoevi-face-max.png", (300, 35, 1160, 680)),
]


def main():
    source = Image.open(SOURCE).convert("RGB")
    contact = Image.new("RGB", (320 * len(CROPS), 240), "white")
    draw = ImageDraw.Draw(contact)

    for index, (name, box) in enumerate(CROPS):
        image = source.crop(box).resize((320, 240), Image.Resampling.LANCZOS)
        image.save(OUT_DIR / name)
        contact.paste(image, (index * 320, 0))
        draw.text((index * 320 + 8, 8), name.removeprefix("nukoevi-face-").removesuffix(".png"), fill=(60, 35, 25))

    contact.save(OUT_DIR / "nukoevi-face-large-contact.png")


if __name__ == "__main__":
    main()
