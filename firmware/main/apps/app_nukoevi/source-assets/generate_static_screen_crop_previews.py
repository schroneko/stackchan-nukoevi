from pathlib import Path

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "source-assets" / "nukoevi-final-source.png"
OUT_DIR = ROOT / "source-assets"

CROPS = [
    ("nukoevi-static-crop-safe.png", (88, 40, 1360, 994)),
    ("nukoevi-static-crop-balanced.png", (130, 70, 1330, 970)),
    ("nukoevi-static-crop-close.png", (160, 80, 1320, 950)),
]


def main():
    source = Image.open(SOURCE).convert("RGB")

    for name, box in CROPS:
        cropped = source.crop(box)
        cropped = cropped.resize((320, 240), Image.Resampling.LANCZOS)
        cropped.save(OUT_DIR / name)

    contact = Image.new("RGB", (320 * len(CROPS), 240), (255, 255, 255))
    draw = ImageDraw.Draw(contact)
    for index, (name, _) in enumerate(CROPS):
        image = Image.open(OUT_DIR / name).convert("RGB")
        contact.paste(image, (index * 320, 0))
        draw.text((index * 320 + 8, 8), name.removeprefix("nukoevi-static-crop-").removesuffix(".png"), fill=(60, 35, 25))
    contact.save(OUT_DIR / "nukoevi-static-crop-contact.png")


if __name__ == "__main__":
    main()
