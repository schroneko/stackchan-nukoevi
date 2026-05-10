import json
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
REPO = ROOT.parents[1]
SOURCE = REPO / "firmware/main/apps/app_nukoevi/source-assets/nukoevi-bridge-icon-source.png"
ASSETS = ROOT / "NukoeviBridge/Assets.xcassets"
APPICON = ASSETS / "AppIcon.appiconset"

IMAGES = [
    ("iphone", "20x20", "2x", 40),
    ("iphone", "20x20", "3x", 60),
    ("iphone", "29x29", "2x", 58),
    ("iphone", "29x29", "3x", 87),
    ("iphone", "40x40", "2x", 80),
    ("iphone", "40x40", "3x", 120),
    ("iphone", "60x60", "2x", 120),
    ("iphone", "60x60", "3x", 180),
    ("ipad", "20x20", "1x", 20),
    ("ipad", "20x20", "2x", 40),
    ("ipad", "29x29", "1x", 29),
    ("ipad", "29x29", "2x", 58),
    ("ipad", "40x40", "1x", 40),
    ("ipad", "40x40", "2x", 80),
    ("ipad", "76x76", "1x", 76),
    ("ipad", "76x76", "2x", 152),
    ("ipad", "83.5x83.5", "2x", 167),
    ("ios-marketing", "1024x1024", "1x", 1024),
]


def main():
    ASSETS.mkdir(parents=True, exist_ok=True)
    APPICON.mkdir(parents=True, exist_ok=True)
    image = Image.open(SOURCE).convert("RGB")

    (ASSETS / "Contents.json").write_text(
        json.dumps({"info": {"author": "xcode", "version": 1}}, indent=2) + "\n",
        encoding="utf-8",
    )

    contents = {"images": [], "info": {"author": "xcode", "version": 1}}
    for idiom, size, scale, pixels in IMAGES:
        filename = f"nukoevi-icon-{idiom}-{size.replace('.', '_').replace('x', 'x')}-{scale}.png"
        resized = image.resize((pixels, pixels), Image.Resampling.LANCZOS)
        resized.save(APPICON / filename)
        contents["images"].append(
            {
                "filename": filename,
                "idiom": idiom,
                "scale": scale,
                "size": size,
            }
        )

    (APPICON / "Contents.json").write_text(json.dumps(contents, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
