"""
Worlds Beyond - pass 20: the UI overhaul's foundation (Plan 6).

Run with the editor closed, after building the C++:
    UnrealEditor-Cmd.exe <path>/WorldBeyond.uproject -run=pythonscript -script=<path>/Scripts/Migration/migrate_pass20.py -unattended -nosplash -NullRHI

- The menus' fonts from Art/Fonts (SIL Open Font License, see the OFL files there): font faces FF_Cinzel_* and
  FF_EBGaramond_* and the composite fonts F_Cinzel (Regular, Bold) and F_EBGaramond (Regular, Italic, Bold) in
  /Game/WorldsBeyond/UI/Fonts. The UI settings point at them by default.
- DA_UIArt (the shared art set; the generated art fills it in pass 21) in /Game/WorldsBeyond/UI.
- Profiles: DA_Profile_Angel / _JiWoong / _Duo (class title, motto, element, accent) in /Game/WorldsBeyond/UI/Profiles,
  set as Profile on BP_Angel and BP_Ji-Woong and as Duo Profile on BP_PC.
- IA_CharacterMenu on C, IA_QuestLog on J, IA_Pause on P and the gamepad's Menu button in IMC_Default, set on BP_PC.
  Keys another action already uses are reported and left alone, and raw key events in the demigods' and the
  controller's Blueprints are listed.

Every asset saved is copied to Saved/MigrationBackups/<timestamp>/ first. Report: last_run_pass20.txt.
"""
import os
import re
import sys

import unreal

sys.path.insert(0, os.path.join(os.path.abspath(unreal.Paths.project_dir()), "Scripts", "Migration"))
from migration_common import (ASSET_TOOLS, EAL, cdo, load, log, make_key, map_keys, save, set_props, warn,  # noqa: E402
                              write_report)

PC = "/Game/WorldsBeyond/Blueprints/BP_PC"
ANGEL = "/Game/WorldsBeyond/Characters/Angel/BP_Angel"
JIWOONG = "/Game/WorldsBeyond/Characters/Ji-Woong/BP_Ji-Woong"
IMC = "/Game/Input/IMC_Default"
UI = "/Game/WorldsBeyond/UI"
FONTS = UI + "/Fonts"
PROFILES = UI + "/Profiles"
ART_SET = UI + "/DA_UIArt"
FONT_DIR = os.path.join(os.path.abspath(unreal.Paths.project_dir()), "Art", "Fonts")

FAMILIES = {
    "F_Cinzel": [("Regular", "Cinzel-Regular.ttf"), ("Bold", "Cinzel-Bold.ttf")],
    "F_EBGaramond": [("Regular", "EBGaramond-Regular.ttf"), ("Italic", "EBGaramond-Italic.ttf"), ("Bold", "EBGaramond-Bold.ttf")],
}

PROFILE_DATA = {
    "DA_Profile_Angel": dict(title="Stormcaller", motto="Command the elements. Control the storm.",
                             element="STORM", accent=(0.06, 0.32, 1.0)),
    "DA_Profile_JiWoong": dict(title="Radiant Hexblade", motto="Steel in one hand, the sun in the other.",
                               element="RADIANT", accent=(1.0, 0.55, 0.08)),
    "DA_Profile_Duo": dict(title="Bond of Heaven", motto="Two demigods. One judgment.",
                           element="ECLIPSE", accent=(0.38, 0.18, 1.0)),
}

NEW_KEYS = [("IA_CharacterMenu", "C"), ("IA_QuestLog", "J"), ("IA_Pause", "P"), ("IA_Pause", "Gamepad_Special_Right")]


# ---------------------------------------------------------------- fonts

def step_fonts():
    lib = unreal.BeyondUIEditorLibrary
    fonts = {}
    for family, faces in FAMILIES.items():
        names = []
        assets = []
        for typeface, file_name in faces:
            path = os.path.join(FONT_DIR, file_name)
            if not os.path.exists(path):
                warn("missing font file %s (download it into Art/Fonts)" % path)
                continue
            face_path = "%s/FF_%s_%s" % (FONTS, family[2:], typeface)
            face = lib.import_font_face(path, face_path)
            if face is None:
                warn("could not import %s" % path)
                continue
            save(face, face_path)
            names.append(typeface)
            assets.append(face)
        if not assets:
            continue
        font_path = "%s/%s" % (FONTS, family)
        font = lib.make_composite_font(font_path, names, assets)
        if font is None:
            warn("could not make %s" % font_path)
            continue
        save(font, font_path)
        fonts[family] = font
        log("%s: %s" % (font_path, ", ".join(names)))
    return fonts


# ---------------------------------------------------------------- art set and profiles

def ensure_data_asset(path, cls):
    if EAL.does_asset_exist(path):
        return load(path), False
    folder, name = path.rsplit("/", 1)
    asset = ASSET_TOOLS.create_asset(name, folder, cls, unreal.DataAssetFactory())
    if asset is None:
        warn("could not create %s" % path)
    return asset, True


def step_art_set():
    art, created = ensure_data_asset(ART_SET, unreal.BeyondUIArtSet)
    if art is None:
        return None
    save(art, ART_SET)
    glow = art.get_editor_property("glow")
    log("%s %s (glow %s)" % (ART_SET, "created" if created else "kept", glow))
    return art


def step_profiles():
    profiles = {}
    for name, data in PROFILE_DATA.items():
        path = "%s/%s" % (PROFILES, name)
        profile, created = ensure_data_asset(path, unreal.BeyondCharacterProfile)
        if profile is None:
            continue
        profile.set_editor_property("class_title", unreal.Text(data["title"]))
        profile.set_editor_property("motto", unreal.Text(data["motto"]))
        profile.set_editor_property("primary_element", getattr(unreal.BeyondElement, data["element"]))
        profile.set_editor_property("accent_color", unreal.LinearColor(*data["accent"], 1.0))
        save(profile, path)
        profiles[name] = profile
        log("%s %s: %s - %s" % (path, "created" if created else "updated", data["title"], data["motto"]))
    return profiles


# ---------------------------------------------------------------- keys

def existing_mappings():
    imc = load(IMC)
    found = {}
    if imc is None:
        return found
    for mapping in imc.get_editor_property("default_key_mappings").get_editor_property("mappings"):
        action = mapping.get_editor_property("action")
        key = str(mapping.get_editor_property("key").get_editor_property("key_name"))
        found.setdefault(key, set()).add(action.get_name() if action else "")
    return found


def blueprint_key_events(asset_path):
    """Raw key events (K2Node_InputKey) in a Blueprint's graphs, best effort from the JSON graph export."""
    asset = load(asset_path)
    if asset is None or not hasattr(unreal, "JsonObjectGraphFunctionLibrary"):
        return None
    try:
        file_name = unreal.JsonObjectGraphFunctionLibrary.write_package_to_temp_file(asset, "pass20_keys", unreal.JsonStringifyOptions())
    except Exception as e:  # noqa: BLE001 - only a report
        warn("could not export %s's graphs (%s)" % (asset_path, e))
        return None
    if not file_name or not os.path.exists(file_name):
        return None
    with open(file_name, encoding="utf-8", errors="ignore") as fh:
        text = fh.read()
    keys = set()
    for match in re.finditer(r"K2Node_InputKey", text):
        window = text[match.start(): match.start() + 4000]
        key = re.search(r'"(?:InputKey|KeyName)"\s*:\s*\{?\s*"?(?:KeyName"\s*:\s*")?([A-Za-z_0-9]+)"', window)
        if key:
            keys.add(key.group(1))
    return keys


def step_keys():
    mappings = existing_mappings()
    wanted = {}
    for action, key in NEW_KEYS:
        others = mappings.get(key, set()) - {action}
        if others:
            warn("%s is already mapped to %s in IMC_Default: %s not mapped there (pick another key for it)"
                 % (key, ", ".join(sorted(others)), action))
            continue
        wanted.setdefault(action, []).append(key)

    for path in (ANGEL, JIWOONG, PC):
        keys = blueprint_key_events(path)
        if keys is None:
            log("%s: raw key events not checked" % path)
            continue
        clashes = sorted(keys & {key for _, key in NEW_KEYS})
        log("%s: raw key events %s" % (path, ", ".join(sorted(keys)) or "none"))
        if clashes:
            warn("%s also reacts to %s in its graph: disable them under Input -> Legacy Keys To Disable" % (path, ", ".join(clashes)))

    actions = {}
    for action, keys in wanted.items():
        for key in keys:
            actions.update(map_keys(IMC, {action: key}))
    return actions


# ---------------------------------------------------------------- assign

def step_assign(profiles, actions):
    for path, profile_name in ((ANGEL, "DA_Profile_Angel"), (JIWOONG, "DA_Profile_JiWoong")):
        bp = load(path)
        if bp is not None and profiles.get(profile_name):
            set_props(bp, path, profile=profiles[profile_name])
            log("%s: Profile = %s" % (path, profile_name))

    pc = load(PC)
    if pc is None:
        warn("%s not found; set Duo Profile and the Character Menu / Quest Log / Pause actions by hand" % PC)
        return
    props = {}
    if profiles.get("DA_Profile_Duo"):
        props["duo_profile"] = profiles["DA_Profile_Duo"]
    for action, prop in (("IA_CharacterMenu", "character_menu_action"), ("IA_QuestLog", "quest_log_action"), ("IA_Pause", "pause_action")):
        if actions.get(action):
            props[prop] = actions[action]
    if props:
        set_props(pc, PC, **props)
        log("%s: %s" % (PC, ", ".join(sorted(props))))


def main():
    log("pass 20")
    missing = [name for name in ("BeyondUIEditorLibrary", "BeyondUIArtSet", "BeyondCharacterProfile", "BeyondElement")
               if not hasattr(unreal, name)]
    if missing:
        warn("the editor is running an old build of the WorldBeyond modules (%s missing). Close the editor, build "
             "WorldBeyondEditor (Development Editor, Win64) until it says Build succeeded, then run this script again. "
             "Nothing was changed." % ", ".join(missing))
        write_report("last_run_pass20.txt")
        raise RuntimeError("WorldBeyond C++ not rebuilt: %s missing" % ", ".join(missing))

    step_fonts()
    step_art_set()
    profiles = step_profiles()
    actions = step_keys()
    step_assign(profiles, actions)
    write_report("last_run_pass20.txt")


main()
