"""
Worlds Beyond - pass 14: the open world's systems data (Plan 5A).

Run with the editor closed, after building the C++:
    UnrealEditor-Cmd.exe <path>/WorldBeyond.uproject -run=pythonscript -script=<path>/Scripts/Migration/migrate_pass14.py -unattended -nosplash -NullRHI

- IA_WorldMap on M (and the gamepad's View button) in IMC_Default; BP_PC's World Map Action.
- BP_PC's party: Party Classes = BP_Angel, BP_Ji-Woong (placed ones are still used; a missing one is spawned beside the
  leader, so an open-world map only needs one demigod placed and never loses Ji-Woong to an unloaded cell).
- Region definitions from world_content.py in /Game/WorldsBeyond/World/Regions (DA_Region_<id>): 4 regions, 14
  villages and Kingsfork Crossroads. Only missing ones are created; BEYOND_REBUILD_REGIONS=1 writes them all again.
- The music stems they use get Looping on.
- Eight frost / molten enemies (copies of the Plan 3 roster with a new id, region, tint and sturdier health) added to
  DA_EnemyRoster; BEYOND_REBUILD_ROSTER=1 makes them again.
- Kael'thar is tagged Region.Molten and Hrimgar Region.Frost (pass 11 had the Plan 3 roster regions).

Every asset saved is copied to Saved/MigrationBackups/<timestamp>/ first. Report: last_run_pass14.txt.
"""
import importlib
import os
import sys

import unreal

sys.path.insert(0, os.path.join(os.path.abspath(unreal.Paths.project_dir()), "Scripts", "Migration"))
from migration_common import (ASSET_TOOLS, EAL, bp_class, cdo, load, log, map_keys, save, tag, warn,  # noqa: E402
                              write_report)
import world_content as content  # noqa: E402

importlib.reload(content)

PC = "/Game/WorldsBeyond/Blueprints/BP_PC"
ANGEL = "/Game/WorldsBeyond/Characters/Angel/BP_Angel"
JIWOONG = "/Game/WorldsBeyond/Characters/Ji-Woong/BP_Ji-Woong"
IMC = "/Game/Input/IMC_Default"
REGIONS = "/Game/WorldsBeyond/World/Regions"
ENEMIES = "/Game/WorldsBeyond/Enemies"
ROSTER = ENEMIES + "/Roster"
BOSSES = ENEMIES + "/Bosses"
ROSTER_ASSET = ENEMIES + "/DA_EnemyRoster"

REBUILD_REGIONS = os.environ.get("BEYOND_REBUILD_REGIONS", "") == "1"
REBUILD_ROSTER = os.environ.get("BEYOND_REBUILD_ROSTER", "") == "1"

KINDS = {"Region": unreal.BeyondRegionKind.REGION, "Village": unreal.BeyondRegionKind.VILLAGE, "Area": unreal.BeyondRegionKind.AREA}


def colour(values):
    return unreal.LinearColor(*(list(values) + [1.0] * (4 - len(values))))


def struct(cls, **props):
    value = cls()
    for key, item in props.items():
        value.set_editor_property(key, item)
    return value


# ---------------------------------------------------------------- input and the party

def step_input():
    actions = map_keys(IMC, {"IA_WorldMap": "M"})
    map_keys(IMC, {"IA_WorldMap": "Gamepad_Special_Left"})
    pc = load(PC)
    if pc is None:
        warn("%s not found; set World Map Action and the party's Party Classes on your player controller by hand" % PC)
        return
    defaults = cdo(pc)
    defaults.set_editor_property("world_map_action", actions["IA_WorldMap"])

    angel = bp_class(ANGEL)
    jiwoong = bp_class(JIWOONG)
    party = defaults.get_editor_property("party_component")
    if party is None:
        warn("%s has no Party Component" % PC)
    elif angel is None or jiwoong is None:
        warn("BP_Angel / BP_Ji-Woong not found: Party Classes left as they were")
    else:
        party.set_editor_property("party_classes", [angel, jiwoong])
        log("%s: Party Classes = BP_Angel, BP_Ji-Woong" % PC)
    save(pc, PC)
    log("%s: M (and the gamepad's View button) opens the world map" % PC)


# ---------------------------------------------------------------- music

def step_music():
    for path in content.LOOPING_MUSIC:
        wave = load(path)
        if wave is None or not isinstance(wave, unreal.SoundWave):
            continue
        if not wave.get_editor_property("looping"):
            wave.set_editor_property("looping", True)
            save(wave, path)
            log("%s loops" % path.rsplit("/", 1)[1])


# ---------------------------------------------------------------- region definitions

def weather_preset(key):
    w = content.WEATHER[key]
    preset = struct(unreal.BeyondWeatherPreset,
                    sun_intensity=float(w["sun"]), sun_color=colour(w["sun_colour"]),
                    sky_light_intensity=float(w["sky"]), sky_light_color=colour(w["sky_colour"]),
                    fog_density=float(w["fog"]), fog_height_falloff=float(w["falloff"]), fog_color=colour(w["fog_colour"]),
                    saturation=float(w["saturation"]), tint=colour(w["tint"]), exposure_bias=float(w["exposure"]))
    if w.get("precipitation"):
        system = load(w["precipitation"])
        if system is not None:
            preset.set_editor_property("precipitation", system)
    return preset


def soft_sound(path):
    if not path:
        return None
    sound = load(path)
    if sound is None:
        warn("sound %s not found (left empty)" % path)
    return sound


def step_regions():
    made = kept = 0
    for region_id, r in content.REGIONS.items():
        name = "DA_Region_%s" % region_id
        path = "%s/%s" % (REGIONS, name)
        exists = EAL.does_asset_exist(path)
        if exists and not REBUILD_REGIONS:
            kept += 1
            continue
        asset = load(path) if exists else ASSET_TOOLS.create_asset(name, REGIONS, unreal.BeyondRegionDefinition, unreal.DataAssetFactory())
        if asset is None:
            warn("could not create %s" % path)
            continue
        props = dict(region_id=unreal.Name(region_id), display_name=unreal.Text(r["name"]),
                     subtitle=unreal.Text(r["subtitle"] or ""), kind=KINDS[r["kind"]],
                     parent_id=unreal.Name(r["parent"] or "None"), region_tag=tag(r["tag"]),
                     level_min=int(r["band"][0]), level_max=int(r["band"][1]),
                     banter_context=unreal.Name(r["banter"] or region_id), map_colour=colour(r["colour"]),
                     override_weather=r["weather"] is not None)
        if r["weather"] is not None:
            props["weather"] = weather_preset(r["weather"])
        for key, value in props.items():
            try:
                asset.set_editor_property(key, value)
            except Exception as e:
                warn("%s: could not set %s (%s)" % (path, key, e))
        for key in ("music", "ambience"):
            sound = soft_sound(r.get(key))
            if sound is not None:
                asset.set_editor_property(key, sound)
        save(asset, path)
        made += 1
    log("%s: %d region definitions written, %d kept (BEYOND_REBUILD_REGIONS=1 rewrites them)" % (REGIONS, made, kept))


# ---------------------------------------------------------------- frost and molten enemies

def step_roster():
    roster = load(ROSTER_ASSET)
    if roster is None:
        warn("%s missing: run migrate_pass10.py first" % ROSTER_ASSET)
        return
    enemies = [e for e in roster.get_editor_property("enemies") if e is not None]
    listed = {e.get_path_name() for e in enemies}
    added = []
    for name, source, enemy_id, display, region, tint, health, description in content.ROSTER_VARIANTS:
        path = "%s/%s" % (ROSTER, name)
        exists = EAL.does_asset_exist(path)
        if exists and not REBUILD_ROSTER:
            enemy = load(path)
        else:
            if exists:
                EAL.delete_asset(path)
            source_path = "%s/%s" % (ROSTER, source)
            if not EAL.does_asset_exist(source_path):
                warn("%s missing (pass 10): %s skipped" % (source_path, name))
                continue
            enemy = EAL.duplicate_asset(source_path, path)
            if enemy is None:
                warn("could not copy %s to %s" % (source_path, path))
                continue
            enemy.set_editor_property("enemy_id", unreal.Name(enemy_id))
            enemy.set_editor_property("display_name", unreal.Text(display))
            enemy.set_editor_property("description", unreal.Text(description))
            enemy.set_editor_property("region", tag(region))
            enemy.set_editor_property("tint", colour(tint))
            enemy.set_editor_property("max_health", float(enemy.get_editor_property("max_health")) * health)
            save(enemy, path)
            log("built %s (%s, copy of %s)" % (path, enemy_id, source))
        if enemy is not None and enemy.get_path_name() not in listed:
            added.append(enemy)
    if added:
        roster.set_editor_property("enemies", enemies + added)
        save(roster, ROSTER_ASSET)
    log("%s: %d frost / molten enemies added" % (ROSTER_ASSET, len(added)))


def step_boss_regions():
    for name, region in content.BOSS_REGIONS.items():
        path = "%s/%s" % (BOSSES, name)
        boss = load(path) if EAL.does_asset_exist(path) else None
        if boss is None:
            warn("%s missing (pass 11)" % path)
            continue
        current = boss.get_editor_property("region")
        if str(current.get_editor_property("tag_name")) != region:
            boss.set_editor_property("region", tag(region))
            save(boss, path)
            log("%s: region %s" % (name, region))


# ---------------------------------------------------------------- main

def main():
    log("pass 14")
    missing = [name for name in ("BeyondRegionDefinition", "BeyondWeatherPreset", "BeyondRegionVolume", "BeyondWaystone",
                                 "BeyondPointOfInterest", "BeyondWorldInfo", "BeyondWorldSubsystem", "BeyondWorldMapWidget",
                                 "BeyondRegionBannerWidget", "BeyondOpenWorldNavigationSystem", "BeyondHazardVolume")
               if not hasattr(unreal, name)]
    if missing:
        warn("the editor is running an old build of the WorldBeyond module (%s missing). Close the editor, build "
             "WorldBeyondEditor (Development Editor, Win64) until it says Build succeeded, then run this script again. "
             "Nothing was changed." % ", ".join(missing))
        write_report("last_run_pass14.txt")
        raise RuntimeError("WorldBeyond C++ not rebuilt: %s missing" % ", ".join(missing))

    step_input()
    step_music()
    step_regions()
    step_roster()
    step_boss_regions()
    write_report("last_run_pass14.txt")


main()
