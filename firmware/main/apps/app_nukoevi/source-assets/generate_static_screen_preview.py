from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "source-assets" / "nukoevi-final-source.png"
OUTPUT = ROOT / "source-assets" / "nukoevi-static-screen-preview.png"


def main():
    source = Image.open(SOURCE).convert("RGB")
    image = source.resize((320, 240), Image.Resampling.LANCZOS)
    image.save(OUTPUT)


if __name__ == "__main__":
    main()
