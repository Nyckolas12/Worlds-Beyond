"""
Prints generate_image_batch requests for the next missing skill icons on Nano Banana 2 (black background, keyed later
with key_black.py), from the prompts in ui_art_manifest.py:  python Scripts/Art/nano_requests.py <count> [skip]
"""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ui_art_manifest as m  # noqa: E402

count = int(sys.argv[1])
skip = int(sys.argv[2]) if len(sys.argv) > 2 else 0
missing = [e for e in m.SKILLS if not os.path.exists(m.png_path(e))][skip:skip + count]
requests = []
for index, entry in enumerate(missing):
    prompt = entry["prompt"].replace("Transparent background.", "On a plain pure black background.")
    prompt = prompt.replace("inside the skill-tree nodes", "inside the round skill-tree nodes")
    requests.append({"index": index, "params": {"model": "nano_banana_2", "prompt": prompt, "aspect_ratio": "1:1", "resolution": "1k",
                                                "medias": [{"role": "image_references", "value": m.REF_SKILLTREE}]}})
print(json.dumps(requests))
print(json.dumps({str(i): e["name"] for i, e in enumerate(missing)}))
