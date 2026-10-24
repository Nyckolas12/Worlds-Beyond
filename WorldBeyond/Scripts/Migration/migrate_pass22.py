"""
Worlds Beyond - pass 22: the old empire's ruins in the open world (Plan 5F).

Run with the editor closed, after building the C++ and running passes 15-19:
    UnrealEditor-Cmd.exe <path>/WorldBeyond.uproject -run=pythonscript -script=<path>/Scripts/Migration/migrate_pass22.py -unattended -nosplash -AllowCommandletRendering

1. Nanite on for the SICKA_PERSEPOLIS meshes the ruins use (the kit ships them without it).
2. Prefabs in /Game/WorldsBeyond/World/Prefabs/: LI_Ruin_* levels assembled from ruins_content, each packed into a
   Packed Level Actor Blueprint (BPP_Ruin_*). Prefabs that exist are left alone; BEYOND_REBUILD_RUIN_PREFABS=1
   assembles them again.
3. The sites in Dominion (world_content.RUIN_SITES at the places of the same id; labels WB_RUIN_<place>*): the prefab
   on its pad (or, without one, at the lowest ground under it so no edge floats), rock outcrops round it (on the rim
   of its plateau when it has one), overgrowth from its region. Sites that are there are kept; BEYOND_REBUILD_RUINS=1
   places them all again.

Every asset saved is copied to Saved/MigrationBackups/<timestamp>/ first. Report: last_run_pass22.txt.
"""
import importlib
import math
import os
import random
import sys

import unreal

sys.path.insert(0, os.path.join(os.path.abspath(unreal.Paths.project_dir()), "Scripts", "Migration"))
from migration_common import EAL, load, log, save, warn, write_report  # noqa: E402
import ruins_content as content  # noqa: E402
import world_content  # noqa: E402

for module in (content, world_content):
    importlib.reload(module)

MAP = "/Game/WorldsBeyond/Maps/Dominion"
LAYOUT_ASSET = "/Game/WorldsBeyond/World/DA_WorldLayout"
PREFAB_DIR = "/Game/WorldsBeyond/World/Prefabs"
REBUILD_PREFABS = os.environ.get("BEYOND_REBUILD_RUIN_PREFABS", "") == "1"
REBUILD = os.environ.get("BEYOND_REBUILD_RUINS", "") == "1"
PACKED = os.environ.get("BEYOND_UNPACKED", "") != "1"

WLIB = None
VLIB = None
ACTORS = None
LEVELS = None
LAYOUT = None
WORLD = None
MESHES = {}
COUNTS = {"nanite": 0, "prefabs": 0, "packed": 0, "sites": 0, "rocks": 0, "plants": 0}


# ---------------------------------------------------------------- helpers

def editor_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def m2ue(x, y):
    return x * 100.0, -y * 100.0


def ue_yaw(compass):
    return -float(compass)


def ground(x, y):
    """Ground height (cm) at Unreal X / Y: the landscape, or the layout's terrain where it isn't loaded."""
    landscape = WLIB.get_ground_height(WORLD, LAYOUT, x, y)
    layout_z = WLIB.get_layout_height(LAYOUT, x, y)
    z = landscape[1] if isinstance(landscape, tuple) and landscape[0] else None
    if z is None or abs(z - layout_z) > 3000.0:
        return layout_z
    return z


def segment_distance(p, a, b):
    dx, dy = b[0] - a[0], b[1] - a[1]
    length = dx * dx + dy * dy
    t = 0.0 if length == 0 else max(0.0, min(1.0, ((p[0] - a[0]) * dx + (p[1] - a[1]) * dy) / length))
    return math.hypot(p[0] - (a[0] + t * dx), p[1] - (a[1] + t * dy))


def road_clearance(p):
    """Metres from p (m) to the edge of the nearest road."""
    best = 1e9
    for _road_id, points, width, _cobble in world_content.LAYOUT["roads"]:
        for a, b in zip(points, points[1:]):
            best = min(best, segment_distance(p, a, b) - width / 2.0)
    return best


def inside(polygon, p):
    hit = False
    for (ax, ay), (bx, by) in zip(polygon, polygon[1:] + polygon[:1]):
        if (ay > p[1]) != (by > p[1]) and p[0] < (bx - ax) * (p[1] - ay) / (by - ay) + ax:
            hit = not hit
    return hit


def region_at(p):
    for region_id in ("region_forest", "region_corrupted_woods", "region_frost", "region_molten"):
        if inside(world_content.REGIONS[region_id]["polygon"], p):
            return region_id
    return "region_forest"


AVOID = []


def build_avoid():
    """Points rocks and plants keep clear of: (x, y, radius) m."""
    AVOID[:] = []
    for _id, _name, centre, _yaw, _attuned in world_content.WAYSTONES:
        AVOID.append((centre[0], centre[1], content.SITE_CLEARANCE))
    for _id, centre, _entries, _offset, _rule in world_content.CAMPS:
        AVOID.append((centre[0], centre[1], content.SITE_CLEARANCE))
    for _id, _name, centre, _boss, _level, radius in world_content.ARENAS:
        AVOID.append((centre[0], centre[1], radius / 100.0 + 6.0))
    start, _yaw = world_content.PLAYER_START
    AVOID.append((start[0], start[1], content.SITE_CLEARANCE))


def clear_of(p, radius):
    if road_clearance(p) < radius + content.ROAD_MARGIN:
        return False
    return all(math.hypot(p[0] - x, p[1] - y) >= r + radius for x, y, r in AVOID)


def find_mesh(name, root):
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    for data in registry.get_assets_by_path(root, True):
        if str(data.asset_name) == name:
            return unreal.load_asset("%s.%s" % (data.package_name, data.asset_name))
    warn("no mesh %s under %s" % (name, root))
    return None


def mesh_for(path):
    if path not in MESHES:
        if path == "IVY":
            MESHES[path] = find_mesh(content.IVY_NAME, content.MEGA_PLANTS)
        elif path == "DEAD_BRANCHES":
            MESHES[path] = find_mesh(content.DEAD_BRANCHES_NAME, content.MEGA_PLANTS)
        else:
            MESHES[path] = EAL.load_asset(path)
            if MESHES[path] is None:
                warn("mesh missing: %s" % path)
    return MESHES[path]


def spawn_mesh(mesh, location, rotation, scale, label, folder, blocks):
    actor = ACTORS.spawn_actor_from_class(unreal.StaticMeshActor, location, rotation)
    if actor is None:
        warn("could not place %s" % label)
        return None
    actor.modify()
    component = actor.static_mesh_component
    component.modify()
    component.set_static_mesh(mesh)
    actor.set_actor_scale3d(unreal.Vector(scale, scale, scale))
    if not blocks:
        component.set_collision_profile_name("NoCollision")
        component.set_editor_property("can_ever_affect_navigation", False)
    actor.set_actor_label(label)
    actor.set_folder_path(folder)
    return actor


# ---------------------------------------------------------------- 1. Nanite

def step_nanite():
    for path in content.NANITE:
        mesh = EAL.load_asset(path)
        if mesh is None:
            warn("SICKA mesh missing: %s" % path)
            continue
        settings = mesh.get_editor_property("nanite_settings")
        if settings.get_editor_property("enabled"):
            continue
        settings.set_editor_property("enabled", True)
        mesh.set_editor_property("nanite_settings", settings)
        save(mesh, path)
        COUNTS["nanite"] += 1
    log("Nanite turned on for %d SICKA meshes (%d checked)" % (COUNTS["nanite"], len(content.NANITE)))


# ---------------------------------------------------------------- 2. prefabs

def prefab_path(name):
    return "%s/LI_%s" % (PREFAB_DIR, name)


def build_prefab(name, pieces):
    path = prefab_path(name)
    exists = EAL.does_asset_exist(path)
    if exists and not REBUILD_PREFABS:
        return False
    if exists:
        if not LEVELS.load_level(path):
            warn("could not open %s" % path)
            return False
        WLIB.destroy_actors([a for a in ACTORS.get_all_level_actors() if isinstance(a, unreal.StaticMeshActor)])
    elif not LEVELS.new_level(path, False):
        warn("could not make %s" % path)
        return False
    world = editor_world()
    VLIB.use_external_actors(world)
    placed = 0
    for index, p in enumerate(pieces):
        mesh = mesh_for(content.MESHES[p.key])
        if mesh is None:
            continue
        actor = ACTORS.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*p.location),
                                              unreal.Rotator(roll=p.roll, pitch=p.pitch, yaw=p.yaw))
        actor.modify()
        component = actor.static_mesh_component
        component.modify()
        component.set_static_mesh(mesh)
        actor.set_actor_scale3d(unreal.Vector(*p.scale))
        actor.set_actor_label("%s_%02d" % (p.name, index))
        placed += 1
    if not WLIB.save_world(world):
        warn("%s: some packages could not be saved" % path)
    COUNTS["prefabs"] += 1
    log("%s: %d pieces%s" % (path, placed, " (assembled again)" if exists else ""))
    return True


def step_prefabs():
    built = [name for name, pieces in content.PREFABS.items() if build_prefab(name, pieces)]
    if not built:
        log("ruin prefabs: all %d there (BEYOND_REBUILD_RUIN_PREFABS=1 assembles them again)" % len(content.PREFABS))
    unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    if PACKED:
        for name in content.PREFABS:
            bp_path = VLIB.get_packed_prefab_path(prefab_path(name))
            if EAL.does_asset_exist(bp_path) and name not in built:
                continue
            blueprint = VLIB.pack_prefab(prefab_path(name))
            if blueprint is None:
                warn("could not pack %s: the sites use its Level Instance" % name)
                continue
            save(blueprint, bp_path)
            COUNTS["packed"] += 1
        log("packed %d ruin prefabs into Packed Level Actors" % COUNTS["packed"])
    unreal.EditorLoadingAndSavingUtils.new_blank_map(False)


# ---------------------------------------------------------------- 3. the sites

def plateau_at(centre):
    for plateau_id, p_centre, radius, cliff, _height, _steps, _warp in world_content.LAYOUT.get("plateaus", []):
        if math.hypot(centre[0] - p_centre[0], centre[1] - p_centre[1]) < 5.0:
            return plateau_id, radius, cliff
    return None


def site_floor(centre, reach_m, padded):
    x, y = m2ue(*centre)
    if padded:
        return ground(x, y)
    samples = [ground(x, y)]
    for k in range(8):
        a = k * math.pi / 4.0
        samples.append(ground(x + math.cos(a) * reach_m * 70.0, y + math.sin(a) * reach_m * 70.0))
    return min(samples)


def place_rocks(place_id, centre, count, reach, plateau, rng):
    placed = 0
    for k in range(count):
        for _attempt in range(12):
            angle = rng.uniform(0.0, 2.0 * math.pi)
            if plateau is not None:
                distance = plateau[1] * rng.uniform(0.95, 1.2)
            else:
                distance = reach + rng.uniform(4.0, 14.0)
            p = (centre[0] + math.cos(angle) * distance, centre[1] + math.sin(angle) * distance)
            if clear_of(p, 6.0):
                break
        else:
            continue
        path, (low, high), sink = rng.choice(content.OUTCROPS)
        mesh = mesh_for(path)
        if mesh is None:
            continue
        scale = rng.uniform(low, high)
        box = mesh.get_bounding_box()
        height = (box.max.z - box.min.z) * scale
        x, y = m2ue(*p)
        z = ground(x, y) - box.min.z * scale - sink * height
        yaw = ue_yaw(math.degrees(angle)) + rng.uniform(-30.0, 30.0)
        actor = spawn_mesh(mesh, unreal.Vector(x, y, z), unreal.Rotator(roll=rng.uniform(-6.0, 6.0), pitch=rng.uniform(-6.0, 6.0), yaw=yaw),
                           scale, "WB_RUIN_%s_Rock_%02d" % (place_id, k), "Ruins/" + place_id, True)
        placed += actor is not None
    COUNTS["rocks"] += placed
    return placed


def place_plants(place_id, centre, count, reach, rng):
    meshes, (low, high) = content.OVERGROWTH.get(region_at(centre), ([], (1.0, 1.0)))
    meshes = [m for m in (mesh_for(path) for path in meshes) if m is not None]
    if not meshes:
        return 0
    placed = 0
    for k in range(count):
        angle = rng.uniform(0.0, 2.0 * math.pi)
        distance = rng.uniform(reach * 0.2, reach + 10.0)
        p = (centre[0] + math.cos(angle) * distance, centre[1] + math.sin(angle) * distance)
        if road_clearance(p) < 1.5:
            continue
        x, y = m2ue(*p)
        actor = spawn_mesh(rng.choice(meshes), unreal.Vector(x, y, ground(x, y) - 5.0),
                           unreal.Rotator(roll=0.0, pitch=0.0, yaw=rng.uniform(0.0, 360.0)), rng.uniform(low, high),
                           "WB_RUIN_%s_Grow_%02d" % (place_id, k), "Ruins/" + place_id, False)
        placed += actor is not None
    COUNTS["plants"] += placed
    return placed


def step_sites():
    existing = {a.get_actor_label(): a for a in ACTORS.get_all_level_actors()}
    old = [a for label, a in existing.items() if label.startswith("WB_RUIN_")]
    if REBUILD and old:
        WLIB.destroy_actors(old)
        log("cleared %d ruin actors" % len(old))
        existing = {a.get_actor_label(): a for a in ACTORS.get_all_level_actors()}
    build_avoid()
    places = {place[0]: place for place in world_content.PLACES}
    rng = random.Random(content.SEED)
    for place_id, site in world_content.RUIN_SITES.items():
        label = "WB_RUIN_%s" % place_id
        if label in existing:
            log("%s kept (BEYOND_REBUILD_RUINS=1 places it again)" % label)
            continue
        place = places.get(place_id)
        prefab = content.PREFAB_OF.get(site["prefab"])
        if place is None or prefab is None:
            warn("%s: no place or no prefab %s" % (place_id, site["prefab"]))
            continue
        centre = place[3]
        reach = content.FOOTPRINT[prefab]
        z = site_floor(centre, reach, bool(site.get("pad")))
        x, y = m2ue(*centre)
        transform = unreal.Transform(location=unreal.Vector(x, y, z),
                                     rotation=unreal.Rotator(roll=0.0, pitch=0.0, yaw=ue_yaw(site["yaw"])),
                                     scale=unreal.Vector(1.0, 1.0, 1.0))
        actor = VLIB.place_prefab(WORLD, prefab_path(prefab), transform, label, PACKED)
        if actor is None:
            warn("could not place %s" % label)
            continue
        actor.set_folder_path("Ruins/" + place_id)
        COUNTS["sites"] += 1
        rocks = place_rocks(place_id, centre, int(site.get("outcrops", 0)), reach, plateau_at(centre), rng)
        plants = place_plants(place_id, centre, int(site.get("overgrowth", 0)), reach, rng)
        log("%s: %s at %.1f m, %d rocks, %d plants" % (place_id, prefab, z / 100.0, rocks, plants))


# ---------------------------------------------------------------- main

def main():
    global WLIB, VLIB, ACTORS, LEVELS, LAYOUT, WORLD
    log("pass 22")
    missing = [name for name in ("BeyondWorldBuilderLibrary", "BeyondVillageBuilderLibrary", "BeyondLayoutPlateau") if not hasattr(unreal, name)]
    if missing:
        warn("the editor is running an old build (%s missing). Close the editor, build WorldBeyondEditor (Development Editor, "
             "Win64) until it says Build succeeded, then run this script again. Nothing was changed." % ", ".join(missing))
        write_report("last_run_pass22.txt")
        raise RuntimeError("C++ not rebuilt: %s missing" % ", ".join(missing))
    WLIB = unreal.BeyondWorldBuilderLibrary
    VLIB = unreal.BeyondVillageBuilderLibrary
    ACTORS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    LEVELS = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    LAYOUT = load(LAYOUT_ASSET)
    if LAYOUT is None or not EAL.does_asset_exist(MAP):
        warn("Dominion or its layout missing: run passes 15 to 19 first")
        write_report("last_run_pass22.txt")
        return
    step_nanite()
    step_prefabs()
    if not LEVELS.load_level(MAP):
        warn("could not open %s" % MAP)
        write_report("last_run_pass22.txt")
        return
    WORLD = editor_world()
    WLIB.load_landscape_proxies(WORLD)
    WLIB.load_actors_with_label_prefix(WORLD, "WB_")
    step_sites()
    if WLIB.save_world(WORLD):
        log("saved %s" % MAP)
    else:
        warn("some packages of %s could not be saved" % MAP)
    log("ruins: %(sites)d sites, %(rocks)d rocks, %(plants)d plants; %(prefabs)d prefabs assembled, %(packed)d packed, "
        "Nanite on %(nanite)d meshes" % COUNTS)
    write_report("last_run_pass22.txt")


# BEYOND_IMPORT_ONLY=1: import the steps without running them
if os.environ.get("BEYOND_IMPORT_ONLY", "") != "1":
    main()
