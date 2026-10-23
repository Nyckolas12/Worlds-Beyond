"""
Rebuilds Art/UI/Source/manifest.json from the files on disk (Plan 6), for migrate_pass21.py:
one entry per picture with where it goes, plus the chosen 3D model per item (Tripo over SAM 3 when both exist).

    python Scripts/Art/build_art_manifest.py

Entry fields: name, category, file (relative to Art/UI/Source), job (Higgsfield job id, from downloads.tsv), and one of
    item     DA_Item_<Name>              (Items/T_Icon_Item_<Name>.png; "model" is Models/<Name>_Tripo.glb or _SAM3.glb)
    node     skill node id, e.g. Angel.Arcana1   (Skills/T_Icon_Skill_<Tree>_<Node>.png)
    ability  e.g. ArcaneSpikes                    (Skills/T_Icon_Skill_Ability_<Name>.png)
    emblem   texture name                          (Emblems/...)
    asset    texture name                          (Backdrops, Ornaments, Quests, Portraits)
"""
import json
import os
import re

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "Art", "UI", "Source")


def jobs_by_file():
    jobs = {}
    path = os.path.join(ROOT, "downloads.tsv")
    if os.path.exists(path):
        for line in open(path, encoding="utf-8"):
            line = line.strip()
            if line and not line.startswith("#") and "\t" in line:
                rel, url = line.split("\t")
                match = re.search(r"_([0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12})\.\w+$", url)
                if match:
                    jobs[rel] = match.group(1)
    return jobs


def chosen_model(name):
    for suffix in ("Tripo", "SAM3"):
        rel = "Models/%s_%s.glb" % (name, suffix)
        if os.path.exists(os.path.join(ROOT, rel)):
            return rel, suffix
    return None, None


def main():
    jobs = jobs_by_file()
    entries = []
    for category in sorted(os.listdir(ROOT)):
        folder = os.path.join(ROOT, category)
        if not os.path.isdir(folder) or category == "Models":
            continue
        for file in sorted(os.listdir(folder)):
            if not file.lower().endswith(".png"):
                continue
            name = file[:-4]
            rel = "%s/%s" % (category, file)
            entry = {"name": name, "category": category, "file": rel}
            if rel in jobs:
                entry["job"] = jobs[rel]
            if category == "Items" and name.startswith("T_Icon_Item_"):
                item = name[len("T_Icon_Item_"):]
                entry["item"] = "DA_Item_" + item
                model, kind = chosen_model(item)
                if model:
                    entry["model"] = model
                    entry["model_kind"] = kind
                    model_job = jobs.get(model)
                    if model_job:
                        entry["model_job"] = model_job
            elif category == "Skills" and name.startswith("T_Icon_Skill_"):
                rest = name[len("T_Icon_Skill_"):]
                tree, _, node = rest.partition("_")
                if tree == "Ability":
                    entry["ability"] = node
                else:
                    entry["node"] = "%s.%s" % (tree, node)
            elif category == "Emblems":
                entry["emblem"] = name
            else:
                entry["asset"] = name
            entries.append(entry)

    # Models with no icon of their own (the main-menu relic)
    icons = {e["name"][len("T_Icon_Item_"):] for e in entries if "item" in e}
    extra = []
    models = os.path.join(ROOT, "Models")
    if os.path.isdir(models):
        for file in sorted(os.listdir(models)):
            base = file[:-4].rsplit("_", 1)[0]
            if file.endswith(".glb") and base not in icons and not any(x.get("name") == base for x in extra):
                model, kind = chosen_model(base)
                extra.append({"name": base, "category": "Models", "file": model, "model": model, "model_kind": kind,
                              "model_job": jobs.get(model, "")})

    manifest = {
        "_about": "Generated UI art (Plan 6): rebuilt by Scripts/Art/build_art_manifest.py. Prompts: Scripts/Art/ui_art_manifest.py.",
        "assets": entries + extra,
    }
    with open(os.path.join(ROOT, "manifest.json"), "w", encoding="utf-8") as fh:
        json.dump(manifest, fh, indent=1)
    counts = {}
    for entry in entries + extra:
        counts[entry["category"]] = counts.get(entry["category"], 0) + 1
    with_model = sum(1 for e in entries if "model" in e)
    print("manifest.json: %s; %d items with a 3D model" % (", ".join("%s %d" % kv for kv in sorted(counts.items())), with_model))


if __name__ == "__main__":
    main()
