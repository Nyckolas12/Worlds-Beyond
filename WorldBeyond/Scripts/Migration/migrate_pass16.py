"""
Worlds Beyond - pass 16: what goes in the open world (Plan 5B, greybox).

Run with the editor closed, after building the C++ and running pass 15:
    UnrealEditor-Cmd.exe <path>/WorldBeyond.uproject -run=pythonscript -script=<path>/Scripts/Migration/migrate_pass16.py -unattended -nosplash -AllowCommandletRendering

Into /Game/WorldsBeyond/Maps/Dominion, from world_content.py (labels WB_GP_* for gameplay, WB_VIL_* for village blocks):
- the world info (lights, default weather, the world map), region volumes for the 4 regions, 14 villages and Kingsfork;
- 22 waystones (Mossbrook's starts attuned), 17 places, 10 boss arenas (Gorehide, Veyla, Hrimgar, Kael'thar and six
  empty sites for later bosses) with a checkpoint at each boss's entrance, 30 enemy camps on the region rosters (level 0:
  the region's band, put away when the party is far, back when it rests), the player start in Mossbrook;
- greybox house blocks in every village until pass 17 builds the real one (villages with pass 17 houses are left alone);
- the painted world map (T_WorldMap, a fog mask per region, DA_WorldMap in /Game/WorldsBeyond/World/Map/), baked again
  every run (BEYOND_KEEP_MAP=1 skips it).
Missing gameplay actors are added and every one is put back on the ground; BEYOND_REBUILD_GAMEPLAY=1 replaces them all.
Village blocks are only placed for villages that have none (BEYOND_REBUILD_VILLAGES=1 places them all again).

Report: last_run_pass16.txt.
"""
import importlib
import math
import os
import sys

import unreal

sys.path.insert(0, os.path.join(os.path.abspath(unreal.Paths.project_dir()), "Scripts", "Migration"))
from migration_common import EAL, load, log, save, warn, write_report  # noqa: E402
import world_content as content  # noqa: E402

importlib.reload(content)

WORLD = "/Game/WorldsBeyond/World"
LAYOUT_ASSET = WORLD + "/DA_WorldLayout"
REGIONS_DIR = WORLD + "/Regions"
MAP_DIR = WORLD + "/Map"
MAP = "/Game/WorldsBeyond/Maps/Dominion"
BOSSES = "/Game/WorldsBeyond/Enemies/Bosses"
ROSTER_ASSET = "/Game/WorldsBeyond/Enemies/DA_EnemyRoster"
CUBE = "/Engine/BasicShapes/Cube"

REBUILD_GAMEPLAY = os.environ.get("BEYOND_REBUILD_GAMEPLAY", "") == "1"
REBUILD_VILLAGES = os.environ.get("BEYOND_REBUILD_VILLAGES", "") == "1"
KEEP_MAP = os.environ.get("BEYOND_KEEP_MAP", "") == "1"

LIB = None
ACTORS = None
LAYOUT = None
WORLD_REF = None
COUNTS = {"placed": 0, "moved": 0}


def m2ue(point, z=0.0):
    """Metres east / north -> Unreal X (east) / Y (south)."""
    return unreal.Vector(float(point[0]) * 100.0, -float(point[1]) * 100.0, z)


def ue_yaw(compass):
    """Content yaw (0 east, 90 north) -> Unreal yaw (0 +X, 90 +Y = south)."""
    return -float(compass)


def ground(x, y):
    """Ground height (Unreal Z) at an Unreal X / Y: the landscape, checked against the layout's terrain."""
    landscape = LIB.get_ground_height(WORLD_REF, LAYOUT, x, y)
    layout_z = LIB.get_layout_height(LAYOUT, x, y)
    z = landscape[1] if isinstance(landscape, tuple) and landscape[0] else None
    if z is None or abs(z - layout_z) > 3000.0:
        # The landscape isn't loaded there (or not merged yet): the generated terrain is what it was built from
        return layout_z
    return z


def at_ground(point, lift=0.0):
    location = m2ue(point)
    location.z = ground(location.x, location.y) + lift
    return location


def existing_by_label():
    return {a.get_actor_label(): a for a in ACTORS.get_all_level_actors()}


def place(cls, label, location, yaw, existing, spatially_loaded=False):
    """The actor with this label (moved back onto the ground if it drifted), or a new one."""
    actor = existing.get(label)
    if actor is not None and not REBUILD_GAMEPLAY:
        current = actor.get_actor_location()
        if abs(current.z - location.z) > 30.0 or abs(current.x - location.x) > 1.0 or abs(current.y - location.y) > 1.0:
            actor.set_actor_location(location, False, True)
            COUNTS["moved"] += 1
        return actor, False
    if actor is not None:
        LIB.destroy_actors([actor])
    actor = ACTORS.spawn_actor_from_class(cls, location, unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw))
    if actor is None:
        warn("could not place %s" % label)
        return None, False
    actor.set_actor_label(label)
    LIB.set_spatially_loaded(actor, spatially_loaded)
    COUNTS["placed"] += 1
    return actor, True


def region_def(region_id):
    path = "%s/DA_Region_%s" % (REGIONS_DIR, region_id)
    return load(path) if EAL.does_asset_exist(path) else None


# ---------------------------------------------------------------- steps

def step_open():
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not EAL.does_asset_exist(MAP) or not levels.load_level(MAP):
        warn("%s missing: run migrate_pass15.py first" % MAP)
        return None
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    loaded = LIB.load_landscape_proxies(world)
    # Village blocks stream with the land: load every scripted actor so re-runs see (and replace) them
    scripted = LIB.load_actors_with_label_prefix(world, "WB_")
    log("opened %s (%d landscape proxies loaded for heights, %d scripted actors)" % (MAP, loaded, scripted))
    return world


def step_world_info(existing):
    info, _new = place(unreal.BeyondWorldInfo, "WB_GP_WorldInfo", at_ground((0, 0), 500.0), 0.0, existing)
    if info is None:
        return None
    info.set_editor_property("world_name", unreal.Text(content.WORLD_NAME))
    for prop, label in (("sun", "WB_Sun"), ("sky_light", "WB_SkyLight"), ("height_fog", "WB_HeightFog")):
        if label in existing:
            info.set_editor_property(prop, existing[label])
    w = content.WEATHER["crossroads"]
    preset = unreal.BeyondWeatherPreset()
    preset.set_editor_property("sun_intensity", float(w["sun"]))
    preset.set_editor_property("fog_density", float(w["fog"]))
    preset.set_editor_property("fog_color", unreal.LinearColor(*w["fog_colour"], 1.0))
    info.set_editor_property("default_weather", preset)
    return info


def step_regions(existing):
    for region_id, r in content.REGIONS.items():
        definition = region_def(region_id)
        if definition is None:
            warn("DA_Region_%s missing: run migrate_pass14.py" % region_id)
            continue
        label = "WB_GP_Region_%s" % region_id
        volume, _new = place(unreal.BeyondRegionVolume, label, at_ground(r["centre"], 200.0), 0.0, existing)
        if volume is None:
            continue
        volume.set_editor_property("region", definition)
        volume.set_editor_property("priority", content.PRIORITY[r["kind"]])
        if r.get("polygon"):
            points = []
            for p in r["polygon"]:
                location = m2ue(p)
                location.z = ground(location.x, location.y) + 500.0
                points.append(location)
            volume.set_outline(points)
        else:
            volume.set_editor_property("radius", float(r["radius"]) * 100.0)
    log("region volumes: %d" % len(content.REGIONS))


def step_waystones(existing):
    for stone_id, name, centre, yaw, attuned in content.WAYSTONES:
        stone, _new = place(unreal.BeyondWaystone, "WB_GP_Waystone_%s" % stone_id, at_ground(centre), ue_yaw(yaw), existing)
        if stone is None:
            continue
        stone.set_editor_property("waystone_id", unreal.Name(stone_id))
        stone.set_editor_property("display_name", unreal.Text(name))
        stone.set_editor_property("start_attuned", bool(attuned))
    log("waystones: %d" % len(content.WAYSTONES))


POI_KINDS = {}


def step_places(existing):
    for place_id, name, kind, centre, radius in content.PLACES:
        poi, _new = place(unreal.BeyondPointOfInterest, "WB_GP_Place_%s" % place_id, at_ground(centre, 100.0), 0.0, existing)
        if poi is None:
            continue
        poi.set_editor_property("poi_id", unreal.Name(place_id))
        poi.set_editor_property("display_name", unreal.Text(name))
        poi.set_editor_property("kind", POI_KINDS[kind])
        poi.set_editor_property("discovery_radius", float(radius) * 100.0)
    log("places: %d" % len(content.PLACES))


def step_arenas(existing):
    bosses = 0
    for arena_id, name, centre, boss_name, level, radius in content.ARENAS:
        arena, _new = place(unreal.BeyondBossArena, "WB_GP_Arena_%s" % arena_id, at_ground(centre, 20.0), 0.0, existing)
        if arena is None:
            continue
        boss = load("%s/%s" % (BOSSES, boss_name)) if boss_name else None
        arena.set_editor_property("boss", boss)
        arena.set_editor_property("boss_level", int(level))
        arena.set_editor_property("arena_id", unreal.Name(arena_id))
        arena.set_editor_property("arena_radius", float(radius))
        arena.set_editor_property("engage_radius", float(radius) * 0.75)
        arena.set_editor_property("remember_defeat", True)
        arena.set_editor_property("boss_spawn_radius", 15000.0)
        arena.set_editor_property("boss_despawn_radius", 25000.0)
        if boss is None:
            continue
        bosses += 1
        # A checkpoint at the entrance, on the side facing the nearest waystone
        nearest = min(content.WAYSTONES, key=lambda w: (w[2][0] - centre[0]) ** 2 + (w[2][1] - centre[1]) ** 2)
        dx, dy = nearest[2][0] - centre[0], nearest[2][1] - centre[1]
        length = max(math.hypot(dx, dy), 1.0)
        entrance_m = (centre[0] + dx / length * (radius / 100.0 + 5.0), centre[1] + dy / length * (radius / 100.0 + 5.0))
        facing = math.degrees(math.atan2(-dy, -dx))
        place(unreal.BeyondCheckpoint, "WB_GP_Checkpoint_%s" % arena_id, at_ground(entrance_m, 60.0), ue_yaw(facing), existing)
    log("arenas: %d (%d with a boss, %d empty sites)" % (len(content.ARENAS), bosses, len(content.ARENAS) - bosses))


RULES = {}


def step_camps(existing):
    roster = load(ROSTER_ASSET)
    enemies = {str(e.get_editor_property("enemy_id")): e for e in roster.get_editor_property("enemies") if e is not None} if roster else {}
    missing = set()
    for camp_id, centre, entries, offset, rule in content.CAMPS:
        spawner, _new = place(unreal.BeyondEnemySpawner, "WB_GP_Camp_%s" % camp_id, at_ground(centre, 50.0), 0.0, existing)
        if spawner is None:
            continue
        spawn_entries = []
        for enemy_id, count in entries:
            definition = enemies.get(enemy_id)
            if definition is None:
                missing.add(enemy_id)
                continue
            entry = unreal.BeyondSpawnEntry()
            entry.set_editor_property("enemy", definition)
            entry.set_editor_property("count", int(count))
            entry.set_editor_property("level", 0)
            spawn_entries.append(entry)
        spawner.set_editor_property("entries", spawn_entries)
        spawner.set_editor_property("level_offset", int(offset))
        spawner.set_editor_property("spawn_radius", 550.0)
        spawner.set_editor_property("elite_chance", 0.12)
        spawner.set_editor_property("activation_radius", 5500.0)
        spawner.set_editor_property("deactivation_radius", 15000.0)
        spawner.set_editor_property("respawn", RULES[rule])
    if missing:
        warn("enemies not in the roster (run migrate_pass14.py): %s" % ", ".join(sorted(missing)))
    log("camps: %d" % len(content.CAMPS))


def step_player_start(existing):
    centre, yaw = content.PLAYER_START
    starts = [a for a in ACTORS.get_all_level_actors() if isinstance(a, unreal.PlayerStart)]
    LIB.destroy_actors([extra for extra in starts if extra.get_actor_label() != "WB_GP_PlayerStart"])
    place(unreal.PlayerStart, "WB_GP_PlayerStart", at_ground(centre, 120.0), ue_yaw(yaw), existing)


def step_villages(existing):
    cube = load(CUBE)
    placed = 0
    for region_id, r in content.REGIONS.items():
        if r["kind"] != "Village":
            continue
        prefix = "WB_VIL_%s_" % region_id
        # Every actor with the prefix (copies left by older runs share labels)
        mine = [a for a in ACTORS.get_all_level_actors() if a.get_actor_label().startswith(prefix)]
        # Pass 17's real village is there: never grey blocks over it
        if any(a.get_actor_label().startswith(prefix + "Home_") for a in mine):
            continue
        if mine and not REBUILD_VILLAGES:
            continue
        LIB.destroy_actors(mine)
        radius = float(r["radius"])
        count = 4 + int(radius / 18.0)
        abandoned = "abandoned" in r["name"]
        for index in range(count):
            angle = 2.0 * math.pi * index / count + 0.35
            ring = radius * (0.55 if index % 2 == 0 else 0.72)
            point = (r["centre"][0] + math.cos(angle) * ring, r["centre"][1] + math.sin(angle) * ring)
            location = at_ground(point)
            width, depth, height = 7.0 + (index % 3), 5.5 + (index % 2) * 1.5, 4.5 + (index % 3) * 0.8
            location.z += height * 50.0
            facing = math.degrees(math.atan2(r["centre"][1] - point[1], r["centre"][0] - point[0]))
            block = ACTORS.spawn_actor_from_class(unreal.StaticMeshActor, location, unreal.Rotator(roll=0.0, pitch=0.0, yaw=ue_yaw(facing)))
            if block is None:
                continue
            block.set_actor_label("%sHouse_%02d" % (prefix, index))
            block.static_mesh_component.set_static_mesh(cube)
            block.set_actor_scale3d(unreal.Vector(width, depth, height) * (0.85 if abandoned else 1.0))
            placed += 1
    log("greybox village blocks: %d" % placed)


def step_map(info):
    if KEEP_MAP:
        log("world map kept (BEYOND_KEEP_MAP=1)")
        return
    data = LIB.bake_world_map(LAYOUT, MAP_DIR, 2048)
    if data is None:
        warn("could not paint the world map")
        return
    dirty = {p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()}
    for asset in [data, data.get_editor_property("map_texture")] + list(data.get_editor_property("region_fog_masks").values()):
        if asset is None:
            continue
        path = asset.get_path_name().split(".")[0]
        if path in dirty:
            save(asset, path)
    if info is not None:
        info.set_editor_property("map_data", data)
    log("painted the world map (%s)" % MAP_DIR)


# ---------------------------------------------------------------- main

def main():
    global LIB, ACTORS, LAYOUT, WORLD_REF
    log("pass 16")
    missing = [name for name in ("BeyondWorldBuilderLibrary", "BeyondWorldInfo", "BeyondRegionVolume", "BeyondWaystone",
                                 "BeyondPointOfInterest", "BeyondEnemySpawner", "BeyondBossArena") if not hasattr(unreal, name)]
    if missing:
        warn("the editor is running an old build (%s missing). Build WorldBeyondEditor first. Nothing was changed." % ", ".join(missing))
        write_report("last_run_pass16.txt")
        raise RuntimeError("C++ not rebuilt: %s missing" % ", ".join(missing))
    LIB = unreal.BeyondWorldBuilderLibrary
    ACTORS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    POI_KINDS.update({"Landmark": unreal.BeyondPoiKind.LANDMARK, "Cave": unreal.BeyondPoiKind.CAVE, "Shrine": unreal.BeyondPoiKind.SHRINE,
                      "Ruins": unreal.BeyondPoiKind.RUINS, "Vista": unreal.BeyondPoiKind.VISTA, "Lake": unreal.BeyondPoiKind.LAKE,
                      "Camp": unreal.BeyondPoiKind.CAMP, "ArenaSite": unreal.BeyondPoiKind.ARENA_SITE})
    RULES.update({"Never": unreal.BeyondRespawnRule.NEVER, "OnPartyWipe": unreal.BeyondRespawnRule.ON_PARTY_WIPE,
                  "AfterDelay": unreal.BeyondRespawnRule.AFTER_DELAY, "OnRest": unreal.BeyondRespawnRule.ON_REST})

    LAYOUT = load(LAYOUT_ASSET)
    if LAYOUT is None:
        warn("%s missing: run migrate_pass15.py first" % LAYOUT_ASSET)
        write_report("last_run_pass16.txt")
        return
    WORLD_REF = step_open()
    if WORLD_REF is None:
        write_report("last_run_pass16.txt")
        return
    existing = existing_by_label()
    info = step_world_info(existing)
    step_regions(existing)
    step_waystones(existing)
    step_places(existing)
    step_arenas(existing)
    step_camps(existing)
    step_player_start(existing)
    step_villages(existing_by_label())
    step_map(info)
    LIB.configure_open_world(WORLD_REF)
    log("gameplay actors: %d placed, %d put back on the ground" % (COUNTS["placed"], COUNTS["moved"]))
    if LIB.save_world(WORLD_REF):
        log("saved %s" % MAP)
    else:
        warn("some packages of %s could not be saved" % MAP)
    write_report("last_run_pass16.txt")


main()
