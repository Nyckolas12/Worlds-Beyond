"""
Worlds Beyond - pass 21: the generated UI art (Plan 6) into the game.

Run with the editor closed, after building the C++ (and whenever new art has landed; it only re-imports PNGs newer than
their texture, BEYOND_REIMPORT_ART=1 imports everything again):
    UnrealEditor-Cmd.exe <path>/WorldBeyond.uproject -run=pythonscript -script=<path>/Scripts/Migration/migrate_pass21.py -unattended -nosplash -NullRHI

- Imports every Art/UI/Source/<Category>/T_*.png (made with Higgsfield, see Scripts/Art/ui_art_manifest.py) as a UI texture
  (UserInterface2D, no mips, never streamed, size capped per kind) into /Game/WorldsBeyond/UI/<folder>.
- Wires them up by name:
    T_Icon_Item_<X>            -> DA_Item_<X>.Icon
    T_Icon_Skill_<Tree>_<Node> -> the <Tree>.<Node> skill node's Icon (upgrade nodes use their ability's icon)
    T_Icon_Skill_Ability_<X>   -> the ability's Icon (Blueprint abilities without one: DA_UIArt -> Ability Icons)
    T_Icon_Element_<E>, T_Icon_Currency_<C>, T_Emblem_Bond / _Character, T_UI_Backdrop_*, T_UI_RuneCircle -> DA_UIArt
    T_Emblem_Set_<S>           -> DA_Set_<S>.Emblem
    T_Emblem_Class_<X>, T_Portrait_<Who>_<Card|FullArt|Medallion|Silhouette> -> DA_Profile_<Who>
  A picture that isn't there yet is skipped; run the pass again as more arrives.

Every asset saved is copied to Saved/MigrationBackups/<timestamp>/ first. Report: last_run_pass21.txt.
"""
import os
import sys

import unreal

sys.path.insert(0, os.path.join(os.path.abspath(unreal.Paths.project_dir()), "Scripts", "Migration"))
from migration_common import (EAL, bp_class, cdo, load, log, package_file, save, set_props, warn, write_report)  # noqa: E402

PROJECT = os.path.abspath(unreal.Paths.project_dir())
SOURCE = os.path.join(PROJECT, "Art", "UI", "Source")
UI = "/Game/WorldsBeyond/UI"
ART_SET = UI + "/DA_UIArt"
ITEMS = "/Game/WorldsBeyond/Items"
SETS = ITEMS + "/Sets"
TREES = "/Game/WorldsBeyond/SkillTrees"
PROFILES = UI + "/Profiles"
REIMPORT = os.environ.get("BEYOND_REIMPORT_ART") == "1"

# Category folder -> (content folder, max texture size; 0 keeps the source size)
CATEGORIES = {
    "Items": ("Icons/Items", 512),
    "Skills": ("Icons/Skills", 256),
    "Emblems": (None, 512),
    "Backdrops": ("Backdrops", 0),
    "Ornaments": ("Ornaments", 1024),
    "Quests": ("Quests", 1024),
    "Portraits": ("Portraits", 1024),
    "Relic": ("Relic", 1024),
}
# Opaque pictures compress plainly; icons and cut-outs keep a clean alpha with BC7
OPAQUE = ("Backdrops", "Quests")
EMBLEM_FOLDERS = {"T_Icon_Element_": "Icons/Elements", "T_Icon_Currency_": "Icons/Currency", "T_Emblem_Set_": "Icons/Sets", "T_Emblem_": "Emblems"}

ABILITIES = "/Game/WorldsBeyond/Abilities/"
BP_ABILITIES = "/Game/WorldsBeyond/Blueprints/Gameplay_Abilites/Abilites/"
# Ability icon -> the ability classes that use it
ABILITY_CLASSES = {
    "Ability_ArcaneSpikes": [ABILITIES + "Angel/GA_Angel_LightningStrike"],
    "Ability_ArcaneBolt": [ABILITIES + "Angel/GA_Angel_ArcaneBolt", BP_ABILITIES + "GA_ShootProjectile"],
    "Ability_LightningBlink": [BP_ABILITIES + "GA_Blink"],
    "Ability_HealingLight": [BP_ABILITIES + "GA_HealSpell"],
    "Ability_Sunbrand": [ABILITIES + "JiWoong/GA_JiWoong_Sunbrand"],
    "Ability_GildedStep": [ABILITIES + "JiWoong/GA_JiWoong_GildedStep"],
    "Ability_HeavensJudgment": [ABILITIES + "Duo/GA_Duo_HeavensJudgment"],
    "Ability_EclipseBrand": [ABILITIES + "Duo/GA_Duo_EclipseBrand"],
    "Ability_TempestAegis": [ABILITIES + "Duo/GA_Duo_TempestAegis"],
    "JiWoong_Combo": [ABILITIES + "JiWoong/GA_JiWoong_SwordCombo"],
}
# Skill nodes that upgrade an ability show the ability's icon
NODE_ICONS = {
    "Angel.Spikes": "Ability_ArcaneSpikes",
    "Angel.Bolt": "Ability_ArcaneBolt",
    "JiWoong.Brand": "Ability_Sunbrand",
    "JiWoong.Step": "Ability_GildedStep",
    "Duo.Wrath": "Ability_HeavensJudgment",
    "Duo.Eclipse": "Ability_EclipseBrand",
    "Duo.Aegis": "Ability_TempestAegis",
}
PROFILE_FOR = {"Angel": "DA_Profile_Angel", "JiWoong": "DA_Profile_JiWoong", "Duo": "DA_Profile_Duo",
               "Stormcaller": "DA_Profile_Angel", "RadiantHexblade": "DA_Profile_JiWoong", "BondOfHeaven": "DA_Profile_Duo"}
PORTRAIT_PROPERTY = {"Card": "card_portrait", "FullArt": "full_art", "Medallion": "medallion", "Silhouette": "silhouette"}

STATS = {"imported": 0, "kept": 0, "assigned": 0}


# ---------------------------------------------------------------- import

def content_folder(category, name):
    folder, _ = CATEGORIES[category]
    if folder:
        return UI + "/" + folder
    for prefix, emblem_folder in EMBLEM_FOLDERS.items():
        if name.startswith(prefix):
            return UI + "/" + emblem_folder
    return UI + "/Emblems"


def import_texture(png, category):
    name = os.path.splitext(os.path.basename(png))[0]
    folder = content_folder(category, name)
    path = folder + "/" + name
    existing = package_file(path)
    if existing and not REIMPORT and os.path.getmtime(existing) >= os.path.getmtime(png):
        STATS["kept"] += 1
        return load(path)

    task = unreal.AssetImportTask()
    task.set_editor_property("filename", png)
    task.set_editor_property("destination_path", folder)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", False)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = load(path)
    if texture is None:
        warn("could not import %s" % png)
        return None
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT if category in OPAQUE
                                else unreal.TextureCompressionSettings.TC_BC7)
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property("never_stream", True)
    texture.set_editor_property("srgb", True)
    max_size = CATEGORIES[category][1]
    if max_size:
        texture.set_editor_property("max_texture_size", max_size)
    save(texture, path)
    STATS["imported"] += 1
    return texture


def step_import():
    textures = {}
    for category in CATEGORIES:
        directory = os.path.join(SOURCE, category)
        if not os.path.isdir(directory):
            continue
        for file_name in sorted(os.listdir(directory)):
            if not file_name.lower().endswith(".png"):
                continue
            if not file_name.startswith("T_"):
                warn("skipped %s/%s (not named T_...; see Scripts/Art/ui_art_manifest.py)" % (category, file_name))
                continue
            texture = import_texture(os.path.join(directory, file_name), category)
            if texture is not None:
                textures[os.path.splitext(file_name)[0]] = texture
    log("textures: %d imported, %d up to date" % (STATS["imported"], STATS["kept"]))
    return textures


# ---------------------------------------------------------------- wiring

def assign(asset, path, prop, value):
    try:
        current = asset.get_editor_property(prop)
        if current == value:
            return False
        asset.set_editor_property(prop, value)
        save(asset, path)
        STATS["assigned"] += 1
        return True
    except Exception as e:  # noqa: BLE001 - report and carry on
        warn("%s: could not set %s (%s)" % (path, prop, e))
        return False


def step_items(textures):
    by_name = {}
    for path in EAL.list_assets(ITEMS, recursive=True, include_folder=False):
        package = path.split(".")[0]
        by_name[package.rsplit("/", 1)[-1]] = package
    done = 0
    for name, texture in textures.items():
        if not name.startswith("T_Icon_Item_"):
            continue
        item_name = "DA_Item_" + name[len("T_Icon_Item_"):]
        path = by_name.get(item_name)
        if not path:
            warn("%s: no item %s yet" % (name, item_name))
            continue
        item = load(path)
        if item is not None:
            assign(item, path, "icon", texture)
            done += 1
    log("item icons: %d" % done)


def skill_texture(textures, node_id):
    own = textures.get("T_Icon_Skill_" + node_id.replace(".", "_"))
    if own is not None:
        return own
    ability = NODE_ICONS.get(node_id)
    return textures.get("T_Icon_Skill_" + ability) if ability else None


def step_skill_nodes(textures):
    for tree_name in ("DA_SkillTree_Angel", "DA_SkillTree_JiWoong", "DA_SkillTree_Duo"):
        path = TREES + "/" + tree_name
        tree = load(path)
        if tree is None:
            continue
        nodes = list(tree.get_editor_property("nodes"))
        changed = 0
        for node in nodes:
            texture = skill_texture(textures, str(node.get_editor_property("id")))
            if texture is not None and node.get_editor_property("icon") != texture:
                node.set_editor_property("icon", texture)
                changed += 1
        if changed:
            tree.set_editor_property("nodes", nodes)
            save(tree, path)
            STATS["assigned"] += changed
        log("%s: %d node icons set" % (tree_name, changed))


def step_abilities(textures, art):
    blueprint_icons = dict(art.get_editor_property("ability_icons")) if art else {}
    for icon_name, classes in ABILITY_CLASSES.items():
        texture = textures.get("T_Icon_Skill_" + icon_name)
        if texture is None:
            continue
        for class_path in classes:
            blueprint = load(class_path)
            if blueprint is None:
                continue
            default = cdo(blueprint)
            try:
                default.get_editor_property("icon")
                if default.get_editor_property("icon") != texture:
                    set_props(blueprint, class_path, icon=texture)
                    STATS["assigned"] += 1
            except Exception:  # noqa: BLE001 - a Blueprint ability without an Icon property
                blueprint_icons[unreal.Name(class_path.rsplit("/", 1)[-1] + "_C")] = texture
    if art is not None:
        assign(art, ART_SET, "ability_icons", blueprint_icons)
    log("ability icons wired (%d Blueprint abilities through DA_UIArt)" % len(blueprint_icons))


def step_art_set(textures, art):
    if art is None:
        return
    elements = dict(art.get_editor_property("element_icons"))
    for name, texture in textures.items():
        if name.startswith("T_Icon_Element_"):
            element = getattr(unreal.BeyondElement, name[len("T_Icon_Element_"):].upper(), None)
            if element is not None:
                elements[element] = texture
    assign(art, ART_SET, "element_icons", elements)

    currencies = dict(art.get_editor_property("currency_icons"))
    for key, enum_name in (("Gold", "GOLD"), ("Crystals", "CRYSTALS"), ("EchoShards", "ECHO_SHARDS"), ("ReverieFragments", "REVERIE_FRAGMENTS")):
        texture = textures.get("T_Icon_Currency_" + key)
        if texture is not None:
            currencies[getattr(unreal.BeyondCurrency, enum_name)] = texture
    assign(art, ART_SET, "currency_icons", currencies)

    for texture_name, prop in (("T_Icon_Currency_SkillPoint", "skill_point_icon"), ("T_Icon_Currency_BondPoint", "bond_point_icon"),
                               ("T_Emblem_Bond", "bond_emblem"), ("T_Emblem_Character", "character_emblem"),
                               ("T_UI_Backdrop_Menu", "menu_backdrop"), ("T_UI_Backdrop_MainMenu", "main_menu_backdrop"),
                               ("T_UI_RuneCircle", "rune_circle")):
        if texture_name in textures:
            assign(art, ART_SET, prop, textures[texture_name])
    log("DA_UIArt: %d element icons, %d currency icons" % (len(elements), len(currencies)))


def step_sets(textures):
    for name, texture in textures.items():
        if name.startswith("T_Emblem_Set_"):
            path = SETS + "/DA_Set_" + name[len("T_Emblem_Set_"):]
            armor_set = load(path) if EAL.does_asset_exist(path) else None
            if armor_set is not None:
                assign(armor_set, path, "emblem", texture)


def step_profiles(textures):
    for name, texture in textures.items():
        if name.startswith("T_Emblem_Class_"):
            profile = PROFILE_FOR.get(name[len("T_Emblem_Class_"):])
            prop = "emblem"
        elif name.startswith("T_Portrait_"):
            parts = name[len("T_Portrait_"):].split("_")
            profile = PROFILE_FOR.get(parts[0]) if len(parts) == 2 else None
            prop = PORTRAIT_PROPERTY.get(parts[1]) if len(parts) == 2 else None
        else:
            continue
        if not profile or not prop:
            warn("%s: no profile field for it" % name)
            continue
        path = PROFILES + "/" + profile
        asset = load(path)
        if asset is not None:
            assign(asset, path, prop, texture)


def main():
    log("pass 21")
    missing = [name for name in ("BeyondUIArtSet", "BeyondCharacterProfile", "BeyondElement", "BeyondCurrency") if not hasattr(unreal, name)]
    if missing:
        warn("the editor is running an old build of the WorldBeyond module (%s missing). Close the editor, build WorldBeyondEditor, "
             "then run this script again. Nothing was changed." % ", ".join(missing))
        write_report("last_run_pass21.txt")
        raise RuntimeError("WorldBeyond C++ not rebuilt: %s missing" % ", ".join(missing))
    if not os.path.isdir(SOURCE):
        warn("no generated art yet in %s" % SOURCE)
        write_report("last_run_pass21.txt")
        return

    textures = step_import()
    art = load(ART_SET)
    step_items(textures)
    step_skill_nodes(textures)
    step_abilities(textures, art)
    step_art_set(textures, art)
    step_sets(textures)
    step_profiles(textures)
    log("%d references set" % STATS["assigned"])
    write_report("last_run_pass21.txt")


main()
