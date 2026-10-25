"""
Finishes the painted portraits (Plan 6): reads the generated renders in Art/UI/Source/Portraits/_raw/ (fetched by
fetch_art.py from downloads.tsv) and writes Art/UI/Source/Portraits/T_Portrait_<Who>_<Kind>.png for migrate_pass21.py.
Medallions get headroom so a circular crop keeps the hair; Ji-Woong's back view is mirrored so the katana sits on his
left hip. Run with the system Python: python Scripts/Art/finish_portraits.py
"""
import os
from PIL import Image, ImageDraw, ImageFilter, ImageOps

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "Art", "UI", "Source", "Portraits")
RAW = os.path.join(OUT, "_raw")
WHO = ("Angel", "JiWoong", "Duo")
KINDS = ("Card", "FullArt", "Medallion", "Silhouette")


def pad_medallion(image, scale, drop):
    """Shrinks the portrait inside a dark blurred copy of itself (drop = extra room above the head)."""
    size = image.width
    back = image.resize((size, size)).filter(ImageFilter.GaussianBlur(size * 0.04))
    back = Image.blend(back, Image.new("RGB", back.size, (4, 6, 14)), 0.82)
    inner = round(size * scale)
    small = image.resize((inner, inner), Image.LANCZOS)
    mask = Image.new("L", (inner, inner), 0)
    margin = inner * 0.05
    ImageDraw.Draw(mask).rounded_rectangle((margin, margin, inner - margin, inner - margin), radius=inner * 0.34, fill=255)
    mask = mask.filter(ImageFilter.GaussianBlur(inner * 0.07))
    top = min(size - inner, (size - inner) // 2 + round(size * drop))
    back.paste(small, ((size - inner) // 2, top), mask)
    return back


def main():
    for who in WHO:
        for kind in KINDS:
            name = "T_Portrait_%s_%s.png" % (who, kind)
            source = os.path.join(RAW, name)
            if not os.path.exists(source):
                continue
            image = Image.open(source)
            if kind == "Silhouette":
                image = image.convert("RGBA")
            else:
                image = image.convert("RGB")
            if kind == "Medallion":
                image = pad_medallion(image, 0.76 if who == "Duo" else 0.86, 0.03)
            elif kind == "FullArt" and who == "JiWoong":
                image = ImageOps.mirror(image)
            image.save(os.path.join(OUT, name))
            print(name, image.size, image.mode)


if __name__ == "__main__":
    main()
