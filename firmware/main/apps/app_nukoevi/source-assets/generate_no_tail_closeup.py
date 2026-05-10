from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
SOURCE_DIR = ROOT / "source-assets"
SOURCE = SOURCE_DIR / "nukoevi-no-tail-generated.png"
OUTPUT = SOURCE_DIR / "nukoevi-no-tail-closeup.png"


def main():
    source = Image.open(SOURCE).convert("RGB")
    image = source.crop((210, 35, 1290, 845)).resize((320, 240), Image.Resampling.LANCZOS)
    image.save(OUTPUT)


if __name__ == "__main__":
    main()
