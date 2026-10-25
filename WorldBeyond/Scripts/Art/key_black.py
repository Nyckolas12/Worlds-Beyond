"""
Turns glowing art painted on black into transparent PNGs (alpha from the brightest channel, colour un-premultiplied).
Used for the skill icons made with Nano Banana 2, which has no transparent background (Plan 6).
The black-background originals are kept in <folder>/_black/.

    python Scripts/Art/key_black.py Skills T_Icon_Skill_Ability_Sunbrand [more names...]
    python Scripts/Art/key_black.py Skills --all      (every PNG in the folder that has no alpha yet)
"""
import os
import shutil
import sys

import numpy as np
from PIL import Image

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "Art", "UI", "Source")


def has_alpha(image):
    return image.mode == "RGBA" and image.getextrema()[3][0] < 250


def key(path):
    image = Image.open(path)
    if has_alpha(image):
        return False
    folder, file = os.path.split(path)
    keep = os.path.join(folder, "_black")
    os.makedirs(keep, exist_ok=True)
    if not os.path.exists(os.path.join(keep, file)):
        shutil.copy2(path, os.path.join(keep, file))
    rgb = np.asarray(image.convert("RGB")).astype(np.float32) / 255.0
    alpha = np.clip((rgb.max(axis=2) - 0.04) / 0.5, 0.0, 1.0)
    colour = np.where(alpha[..., None] > 0, np.clip(rgb / np.maximum(alpha[..., None], 1e-3), 0.0, 1.0), 0.0)
    Image.fromarray((np.dstack([colour, alpha]) * 255).astype(np.uint8), "RGBA").save(path)
    return True


def main():
    folder = os.path.join(ROOT, sys.argv[1])
    names = sys.argv[2:]
    if names == ["--all"]:
        names = [f[:-4] for f in os.listdir(folder) if f.endswith(".png")]
    keyed = [n for n in names if os.path.exists(os.path.join(folder, n + ".png")) and key(os.path.join(folder, n + ".png"))]
    print("keyed %d: %s" % (len(keyed), ", ".join(keyed)))


if __name__ == "__main__":
    main()
