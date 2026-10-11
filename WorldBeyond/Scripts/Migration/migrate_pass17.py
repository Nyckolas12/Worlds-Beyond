"""
Worlds Beyond - pass 17: the villages (Plan 5C).

Run with the editor closed, after building the C++ and running passes 14-16:
    UnrealEditor-Cmd.exe <path>/WorldBeyond.uproject -run=pythonscript -script=<path>/Scripts/Migration/migrate_pass17.py -unattended -nosplash -AllowCommandletRendering

1. Prefabs in /Game/WorldsBeyond/World/Prefabs/: a small level per house type assembled from the kit by village_kit.py
   (7 types, second looks for the common three, 4 boarded up), a market stall, a lantern post, a palisade run; each is
   packed into a Packed Level Actor Blueprint (BPP_*, one actor of instanced meshes) for the open world. Prefabs that
   exist are left alone, so edit them freely and every village follows (repack from the level's Packed Level Actor, or
   run this again); BEYOND_REBUILD_PREFABS=1 assembles them again. Pictures: Saved/WorldBuilder/prefabs/.
2. The villages in Dominion (labels WB_VIL_<village>_*), from village_content.py: houses on rings round a square, doors
   to it, clear of roads, waystones and places; yards, gardens, a well, stalls, lantern posts, gates where the roads come
   in, palisades round Duskwatch, Frostholm and Cinderhold; the abandoned villages boarded up. They replace pass 16's
   grey blocks. A village is only built when it has no houses yet (BEYOND_REBUILD_VILLAGES=1, or =mossbrook,frostholm).
   Pictures from above: Saved/WorldBuilder/villages/ (with rendering).
3. People (labels WB_NPC_*): Mossbrook's seven from Plan 4 on its square, a talker and a chatting pair in every other
   lived-in village; their rows in the dialogue pack's tables (only missing rows; BEYOND_REWRITE_DIALOGUE=1 writes them
   again) and the duo's banter for the frost, the fire, Kingsfork and each village in DA_Banter.

Every asset saved is copied to Saved/MigrationBackups/<timestamp>/ first. Report: last_run_pass17.txt.
"""
import importlib
import math
import os
import random
import sys

import unreal

sys.path.insert(0, os.path.join(os.path.abspath(unreal.Paths.project_dir()), "Scripts", "Migration"))
from migration_common import EAL, bp_class, load, log, save, warn, write_report  # noqa: E402
import dialogue_content  # noqa: E402
import dialogue_rows  # noqa: E402
import village_content as content  # noqa: E402
import village_kit as kit  # noqa: E402
import world_content  # noqa: E402

for module in (dialogue_content, dialogue_rows, content, kit, world_content):
    importlib.reload(module)

MAP = "/Game/WorldsBeyond/Maps/Dominion"
LAYOUT_ASSET = "/Game/WorldsBeyond/World/DA_WorldLayout"
PREFAB_DIR = "/Game/WorldsBeyond/World/Prefabs"
NPC_ROOT = "/Game/WorldsBeyond/NPC"
MANNEQUINS = "/Game/DialogueSystem/Demo/Characters/Mannequins"
PACK = "/Game/DialogueSystem/Blueprint/DataTable"
DT_DIALOGUE = PACK + "/DT_Dialogue"
DT_OVERHEAD = PACK + "/DT_TextOverHead"
DT_SPEAKERS = PACK + "/DT_Speakers"
PORTRAIT = "/Game/WorldsBeyond/Dialogue/M_Beyond_NoPortrait"
BANTER_ASSET = "/Game/WorldsBeyond/Dialogue/DA_Banter"
WELL = "/Game/Meshes/Well/SM_MainWell"
WELL_WOOD = "/Game/Meshes/Well/Well_Materials/MI_Well_SupportBeam01"

SAVED = os.path.join(os.path.abspath(unreal.Paths.project_saved_dir()), "WorldBuilder")
REBUILD_PREFABS = os.environ.get("BEYOND_REBUILD_PREFABS", "") == "1"
REBUILD_VILLAGES = os.environ.get("BEYOND_REBUILD_VILLAGES", "")
REWRITE = os.environ.get("BEYOND_REWRITE_DIALOGUE", "") == "1"
PACKED = os.environ.get("BEYOND_UNPACKED", "") != "1"

# Second looks for the common houses (another mix of wall pieces)
VARIANTS = {"cottage": 2, "cottage_small": 2, "gable_house": 2}
PLAZA = 12.0          # m, the open square in the middle
GAP = 2.5             # m between houses
LIGHT_COLOUR = unreal.Color(r=255, g=170, b=90, a=255)
# Lantern lights: a warm touch at dusk, not pools of light at noon
LIGHT_INTENSITY = 60.0
LIGHT_RADIUS = 650.0
TRIGGERS = {"RegionEntered": "REGION_ENTERED"}

WLIB = None
VLIB = None
ACTORS = None
LEVELS = None
LAYOUT = None
WORLD = None
MESHES = {}
BOUNDS = {}
COUNTS = {"prefabs": 0, "packed": 0, "houses": 0, "props": 0, "npcs": 0, "villages": 0}


# ---------------------------------------------------------------- helpers

def rendering():
    return "-nullrhi" not in unreal.SystemLibrary.get_command_line().lower()


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


def to_world(origin, yaw, local):
    """origin (x, y) cm, yaw (deg, Unreal), local (x, y) cm -> world (x, y) cm."""
    rad = math.radians(yaw)
    return (origin[0] + local[0] * math.cos(rad) - local[1] * math.sin(rad),
            origin[1] + local[0] * math.sin(rad) + local[1] * math.cos(rad))


def segment_distance(p, a, b):
    ax, ay, bx, by = a[0], a[1], b[0], b[1]
    dx, dy = bx - ax, by - ay
    length = dx * dx + dy * dy
    t = 0.0 if length == 0 else max(0.0, min(1.0, ((p[0] - ax) * dx + (p[1] - ay) * dy) / length))
    return math.hypot(p[0] - (ax + t * dx), p[1] - (ay + t * dy))


def road_clearance(p):
    """Metres from p (m) to the edge of the nearest road."""
    best = 1e9
    for _road_id, points, width, _cobble in world_content.LAYOUT["roads"]:
        for a, b in zip(points, points[1:]):
            best = min(best, segment_distance(p, a, b) - width / 2.0)
    return best


AVOID = []


def build_avoid():
    """Points the villages keep clear of: (x, y, radius) m."""
    AVOID[:] = []
    for _id, _name, centre, _yaw, _attuned in world_content.WAYSTONES:
        AVOID.append((centre[0], centre[1], 10.0))
    start, _yaw = world_content.PLAYER_START
    AVOID.append((start[0], start[1], 8.0))
    for _id, _name, _kind, centre, _radius in world_content.PLACES:
        AVOID.append((centre[0], centre[1], 10.0))
    for _id, _name, centre, _boss, _level, radius in world_content.ARENAS:
        AVOID.append((centre[0], centre[1], radius / 100.0 + 6.0))
    for _id, centre, _entries, _offset, _rule in world_content.CAMPS:
        AVOID.append((centre[0], centre[1], 14.0))


def clear_of(p, radius, taken, road_margin=2.0):
    if road_clearance(p) < radius + road_margin:
        return False
    for x, y, r in AVOID:
        if math.hypot(p[0] - x, p[1] - y) < r + radius:
            return False
    for x, y, r in taken:
        if math.hypot(p[0] - x, p[1] - y) < r + radius:
            return False
    return True


def spawn_piece(piece, origin, yaw, base_z, label, snap=False, spatial=True):
    """Spawns a kit piece placed in a local frame (origin (x, y) cm, yaw) at base_z, or at the ground under it with snap."""
    x, y = to_world(origin, yaw, piece.location[:2])
    z = (ground(x, y) if snap else base_z) + piece.location[2]
    rotation = unreal.Rotator(roll=piece.roll, pitch=piece.pitch, yaw=yaw + piece.yaw)
    if piece.key == "LIGHT":
        light = ACTORS.spawn_actor_from_class(unreal.PointLight, unreal.Vector(x, y, z), rotation)
        if light is None:
            return None
        light.set_actor_label(label)
        tune_light(light.get_component_by_class(unreal.PointLightComponent))
        return light
    mesh = MESHES.get(piece.key)
    if mesh is None:
        return None
    actor = ACTORS.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(x, y, z), rotation)
    if actor is None:
        return None
    actor.set_actor_label(label)
    actor.static_mesh_component.set_static_mesh(mesh)
    actor.set_actor_scale3d(unreal.Vector(*piece.scale))
    if piece.key == "WELL":
        fix_well(actor.static_mesh_component)
    if not spatial:
        WLIB.set_spatially_loaded(actor, False)
    COUNTS["props"] += 1
    return actor


def tune_light(component):
    component.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
    component.set_editor_property("light_color", LIGHT_COLOUR)
    component.set_editor_property("intensity", LIGHT_INTENSITY)
    component.set_editor_property("attenuation_radius", LIGHT_RADIUS)
    component.set_editor_property("cast_shadows", False)


def retune_lights():
    """Every village light gets the current settings (they were brighter in the first build)."""
    tuned = 0
    for actor in ACTORS.get_all_level_actors():
        if isinstance(actor, unreal.PointLight) and actor.get_actor_label().startswith("WB_VIL_"):
            component = actor.get_component_by_class(unreal.PointLightComponent)
            if abs(component.get_editor_property("intensity") - LIGHT_INTENSITY) > 0.5 or                     abs(component.get_editor_property("attenuation_radius") - LIGHT_RADIUS) > 0.5:
                tune_light(component)
                tuned += 1
    if tuned:
        log("village lights: %d set to %.0f cd, %.0f uu" % (tuned, LIGHT_INTENSITY, LIGHT_RADIUS))


def fix_well(component):
    """The well's two beam slots came without materials (the sample's): give them the well's own wood."""
    wood = load(WELL_WOOD)
    mesh = component.static_mesh
    for index, slot in enumerate(mesh.get_editor_property("static_materials")):
        if slot.get_editor_property("material_interface") is None and wood is not None:
            component.set_material(index, wood)


def load_meshes():
    MESHES["WELL"] = load(WELL)
    for key, path in kit.MESHES.items():
        mesh = EAL.load_asset(path)
        if mesh is None:
            warn("kit piece %s missing (%s)" % (key, path))
            continue
        MESHES[key] = mesh
        box = mesh.get_bounding_box()
        BOUNDS[key] = ((box.min.x, box.min.y, box.min.z), (box.max.x, box.max.y, box.max.z))


# ---------------------------------------------------------------- 1. prefabs

def prefab_list():
    """name: pieces (local, the door / open side toward +X, the ground at z = 0)."""
    prefabs = {}
    for index, (name, recipe) in enumerate(kit.RECIPES.items()):
        for variant in range(1, VARIANTS.get(name, 1) + 1):
            suffix = "" if variant == 1 else "_%d" % variant
            prefabs["House_%s%s" % (name, suffix)] = kit.house(recipe, seed=index * 10 + variant, bounds=BOUNDS)
    for name in sorted({entry[:-2] for plan in content.VILLAGES.values() for entry in plan["houses"] if entry.endswith("+b")}):
        prefabs["House_%s_boarded" % name] = kit.house(kit.RECIPES[name], seed=77, boarded=True, bounds=BOUNDS)
    prefabs["Stall"] = kit.stall(5)
    prefabs["LanternPost"] = [p for p in kit.lantern_post() if p.key != "LIGHT"]
    prefabs["Palisade"] = kit.palisade_segment(3)
    return prefabs


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
    for index, piece in enumerate(pieces):
        mesh = MESHES.get(piece.key)
        if mesh is None:
            continue
        actor = ACTORS.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*piece.location),
                                              unreal.Rotator(roll=piece.roll, pitch=piece.pitch, yaw=piece.yaw))
        actor.set_actor_label("%s_%02d" % (piece.name, index))
        actor.static_mesh_component.set_static_mesh(mesh)
        actor.set_actor_scale3d(unreal.Vector(*piece.scale))
    if not WLIB.save_world(world):
        warn("%s: some packages could not be saved" % path)
    COUNTS["prefabs"] += 1
    log("%s: %d pieces%s" % (path, len(pieces), " (assembled again)" if exists else ""))
    return True


def preview_lights():
    sun = ACTORS.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 500), unreal.Rotator(roll=0, pitch=-42, yaw=-30))
    sun.light_component.set_intensity(8.0)
    fill = ACTORS.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 500), unreal.Rotator(roll=0, pitch=-25, yaw=150))
    fill.light_component.set_intensity(3.0)
    fill.light_component.set_cast_shadows(False)
    fill.light_component.set_editor_property("atmosphere_sun_light", False)
    ACTORS.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0), unreal.Rotator())
    plane = ACTORS.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, 0), unreal.Rotator())
    plane.static_mesh_component.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Plane"))
    plane.set_actor_scale3d(unreal.Vector(4000, 4000, 1))


def look(target, direction, distance):
    length = math.sqrt(sum(c * c for c in direction))
    d = [c / length for c in direction]
    location = unreal.Vector(target.x + d[0] * distance, target.y + d[1] * distance, target.z + d[2] * distance)
    yaw = math.degrees(math.atan2(-d[1], -d[0]))
    pitch = math.degrees(math.atan2(-d[2], math.sqrt(d[0] * d[0] + d[1] * d[1])))
    return location, unreal.Rotator(roll=0.0, pitch=pitch, yaw=yaw)


def step_prefabs():
    prefabs = prefab_list()
    built = [name for name, pieces in prefabs.items() if build_prefab(name, pieces)]
    if not built:
        log("prefabs: all %d there (BEYOND_REBUILD_PREFABS=1 assembles them again)" % len(prefabs))

    # Pack them, then photograph them, from a scratch level: packing instances the prefab into the current world
    unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    world = editor_world()
    packed = []
    if PACKED:
        for name in prefabs:
            bp_path = VLIB.get_packed_prefab_path(prefab_path(name))
            if EAL.does_asset_exist(bp_path) and name not in built:
                continue
            blueprint = VLIB.pack_prefab(prefab_path(name))
            if blueprint is None:
                warn("could not pack %s: villages use its Level Instance" % name)
                continue
            save(blueprint, bp_path)
            packed.append(name)
            COUNTS["packed"] += 1
        log("packed %d prefabs into Packed Level Actors" % len(packed))
    out = os.path.join(SAVED, "prefabs")
    pictures = [name for name in prefabs if name in built or name in packed or not os.path.exists(os.path.join(out, "%s.png" % name))]
    if pictures and rendering():
        preview_lights()
        os.makedirs(out, exist_ok=True)
        for index, name in enumerate(pictures):
            origin = unreal.Vector(index * 3000.0, 0.0, 0.0)
            VLIB.place_prefab(world, prefab_path(name), unreal.Transform(location=origin), "Preview_" + name, PACKED)
            small = name in ("Stall", "LanternPost", "Palisade")
            target = unreal.Vector(origin.x, origin.y, 150.0 if small else 300.0)
            location, rotation = look(target, (1.0, 0.8, 0.45), 900.0 if small else 1500.0)
            VLIB.capture_view(world, location, rotation, os.path.join(out, "%s.png" % name), 60.0, 800, 600, 0.3)
        VLIB.release_capture()
        log("prefab pictures: %s" % out)
    # A clean level again, so opening the map doesn't ask about this one
    unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    return prefabs


# ---------------------------------------------------------------- 2. villages

def village_ids_to_build(existing):
    wanted = []
    names = {part.strip() for part in REBUILD_VILLAGES.split(",") if part.strip() and part.strip() != "1"}
    for region_id in content.VILLAGES:
        short = region_id.split("_", 1)[1] if "_" in region_id else region_id
        has_houses = any(label.startswith("WB_VIL_%s_Home_" % region_id) for label in existing)
        rebuild = REBUILD_VILLAGES == "1" or short in names or region_id in names
        if has_houses and not rebuild:
            continue
        wanted.append(region_id)
    return wanted


def house_prefab(entry, index):
    boarded = entry.endswith("+b")
    name = entry[:-2] if boarded else entry
    if boarded:
        return name, "House_%s_boarded" % name
    variants = VARIANTS.get(name, 1)
    variant = 1 + index % variants
    return name, "House_%s%s" % (name, "" if variant == 1 else "_%d" % variant)


def footprint_corners(recipe, centre_ue, yaw):
    front, back, half = kit.footprint(recipe)
    corners = [(front, half), (front, -half), (-back, half), (-back, -half), (0.0, 0.0)]
    return [to_world(centre_ue, yaw, c) for c in corners]


def plan_houses(region_id, region, plan, taken):
    """[(prefab, recipe name, centre (m), yaw (Unreal), floor z (cm))]."""
    rng = random.Random(plan["seed"])
    cx, cy = region["centre"]
    radius = float(region["radius"])
    # Inside the flattened pad (pass 15: villages 0.85 of the radius, areas 0.6)
    limit = radius * (0.85 if region["kind"] == "Village" else 0.6) - 1.0
    entries = list(enumerate(plan["houses"]))
    placed = []
    # Outer rings for whatever doesn't fit where the plan says (roads through a village take room)
    rings = list(plan["rings"])
    rings += [f for f in (rings[-1] + 0.18, rings[-1] + 0.32) if f * radius + 6.0 <= limit]
    for ring_fraction in rings:
        if not entries:
            break
        ring = ring_fraction * radius
        start = rng.uniform(0.0, 360.0)
        angle = start
        while entries and angle < start + 360.0:
            index, entry = entries[0]
            name, prefab = house_prefab(entry, index)
            recipe = kit.RECIPES[name]
            front, back, half = [v / 100.0 for v in kit.footprint(recipe)]
            size = math.hypot(max(front, back), half)
            point = (cx + math.cos(math.radians(angle)) * ring, cy + math.sin(math.radians(angle)) * ring)
            compass = math.degrees(math.atan2(cy - point[1], cx - point[0])) + rng.uniform(-8.0, 8.0)
            yaw = ue_yaw(compass)
            ok = (math.hypot(point[0] - cx, point[1] - cy) + size <= limit and clear_of(point, size + GAP / 2.0, taken))
            if ok:
                centre_ue = m2ue(*point)
                heights = [ground(x, y) for x, y in footprint_corners(recipe, centre_ue, yaw)]
                ok = max(heights) - min(heights) <= 120.0
            if ok:
                placed.append((prefab, name, point, yaw, max(heights) - 10.0))
                taken.append((point[0], point[1], size + GAP / 2.0))
                entries.pop(0)
                angle += math.degrees((2.0 * size + GAP) / ring)
            else:
                angle += 3.0
    if entries:
        warn("%s: no room for %d houses (%s)" % (region_id, len(entries), ", ".join(e for _i, e in entries)))
    return placed


def ring_points(centre, ring, count, start, step_check, taken, size):
    """Up to count points spread on a ring (m), each clear of roads and things."""
    points = []
    for i in range(count):
        base = start + 360.0 * i / max(count, 1)
        for nudge in (0.0, 8.0, -8.0, 16.0, -16.0, 24.0, -24.0):
            angle = base + nudge
            p = (centre[0] + math.cos(math.radians(angle)) * ring, centre[1] + math.sin(math.radians(angle)) * ring)
            if step_check(p) and clear_of(p, size, taken):
                points.append((p, angle))
                taken.append((p[0], p[1], size))
                break
    return points


def road_crossings(centre, ring):
    """Where the roads cross a circle (m): [(point, road direction (deg), road width)]."""
    hits = []
    for _road_id, points, width, _cobble in world_content.LAYOUT["roads"]:
        for a, b in zip(points, points[1:]):
            ax, ay = a[0] - centre[0], a[1] - centre[1]
            dx, dy = b[0] - a[0], b[1] - a[1]
            qa = dx * dx + dy * dy
            qb = 2.0 * (ax * dx + ay * dy)
            qc = ax * ax + ay * ay - ring * ring
            disc = qb * qb - 4.0 * qa * qc
            if qa == 0 or disc < 0:
                continue
            for sign in (-1.0, 1.0):
                t = (-qb + sign * math.sqrt(disc)) / (2.0 * qa)
                if 0.0 <= t <= 1.0:
                    p = (a[0] + dx * t, a[1] + dy * t)
                    if all(math.hypot(p[0] - h[0][0], p[1] - h[0][1]) > 5.0 for h in hits):
                        hits.append((p, math.degrees(math.atan2(dy, dx)), width))
    return hits


def build_village(region_id, plan):
    region = world_content.REGIONS[region_id]
    prefix = "WB_VIL_%s_" % region_id
    rng = random.Random(plan["seed"] * 7 + 1)
    cx, cy = region["centre"]
    radius = float(region["radius"])
    kind = plan["props"]
    abandoned = kind == "empty"

    # Room taken: the square, Mossbrook's people, the villagers
    taken = [(cx, cy, PLAZA)]
    for _bp, _label, offset, _facing, _partners in (content.MOSSBROOK if region_id == "village_mossbrook" else []):
        taken.append((cx + offset[0], cy + offset[1], 1.5))
    for villager in content.VILLAGERS.values():
        if villager["village"] == region_id:
            (ox, oy), _facing = villager["at"]
            taken.append((cx + ox, cy + oy, 1.5))

    houses = plan_houses(region_id, region, plan, list(taken))
    for index, (prefab, name, point, yaw, floor) in enumerate(houses):
        x, y = m2ue(*point)
        label = "%sHome_%02d_%s" % (prefix, index, prefab.replace("House_", ""))
        actor = VLIB.place_prefab(WORLD, prefab_path(prefab), unreal.Transform(location=unreal.Vector(x, y, floor),
                                                                            rotation=unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw)),
                                  label, PACKED)
        if actor is None:
            warn("%s: could not place %s" % (label, prefab))
            continue
        COUNTS["houses"] += 1
        recipe = kit.RECIPES[name]
        taken.append((point[0], point[1], math.hypot(*[v / 100.0 for v in kit.footprint(recipe)[1:]]) + 1.0))

        # Yards by the side walls
        unit = 100.0 * kit.SCALE
        sides = [((0.0, -recipe["width"] / 2.0 * unit - 30.0), -90.0), ((0.0, recipe["width"] / 2.0 * unit + 30.0), 90.0),
                 ((-recipe["depth"] / 2.0 * unit - 30.0, 0.0), 180.0)]
        for side_index in rng.sample(range(len(sides)), rng.randint(1, 2)):
            offset, side_yaw = sides[side_index]
            origin = to_world((x, y), yaw, offset)
            for n, piece in enumerate(kit.yard_props(rng, "empty" if abandoned else "busy")):
                spawn_piece(piece, origin, yaw + side_yaw, 0.0, "%sProp_%02d_%d%d" % (prefix, index, side_index, n), snap=True)

        # A fenced garden behind some houses
        if rng.random() < (0.25 if abandoned else 0.4):
            depth = rng.uniform(400.0, 550.0)
            back = -recipe["depth"] / 2.0 * unit - 60.0
            centre_ue = to_world((x, y), yaw, (back - depth / 2.0, 0.0))
            centre_m = (centre_ue[0] / 100.0, -centre_ue[1] / 100.0)
            garden_size = math.hypot(depth / 200.0, recipe["width"] * kit.SCALE / 2.0)
            if clear_of(centre_m, garden_size, taken[:-1] + [(cx, cy, PLAZA)], road_margin=1.0):
                origin = to_world((x, y), yaw, (back, 0.0))
                for n, piece in enumerate(kit.garden(recipe["width"] * unit, depth, broken=abandoned, rng=rng)):
                    spawn_piece(piece, origin, yaw + 180.0, 0.0, "%sGarden_%02d_%d" % (prefix, index, n), snap=True)
                taken.append((centre_m[0], centre_m[1], garden_size))

    # The square: a well, stalls, lantern posts
    # The well near the middle, off the road and clear of the people
    for ox, oy in ((0, 0), (7, 0), (-7, 0), (0, 7), (0, -7), (5, 5), (-5, 5), (5, -5), (-5, -5), (9, 4), (-9, -4)):
        p = (cx + ox, cy + oy)
        if clear_of(p, 1.6, taken[1:], road_margin=2.5):
            well = kit._p("WELL", 0.0, 0.0, 0.0, rng.uniform(0.0, 360.0), (1.0, 1.0, 1.0), "Well")
            spawn_piece(well, m2ue(*p), 0.0, 0.0, prefix + "Well", snap=True)
            taken.append((p[0], p[1], 1.6))
            break
    for n, (p, angle) in enumerate(ring_points((cx, cy), PLAZA + 3.0, plan["stalls"], rng.uniform(0, 360),
                                               lambda q: road_clearance(q) > 2.5, taken, 2.5)):
        x, y = m2ue(*p)
        yaw = ue_yaw(angle + 180.0)
        VLIB.place_prefab(WORLD, prefab_path("Stall"), unreal.Transform(location=unreal.Vector(x, y, ground(x, y)),
                                                                     rotation=unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw)),
                          "%sStall_%d" % (prefix, n), PACKED)
    for n, (p, angle) in enumerate(ring_points((cx, cy), PLAZA + 1.0, plan["lanterns"], rng.uniform(0, 360),
                                               lambda q: road_clearance(q) > 1.5, taken, 1.0)):
        x, y = m2ue(*p)
        yaw = ue_yaw(angle + 180.0)
        z = ground(x, y)
        VLIB.place_prefab(WORLD, prefab_path("LanternPost"), unreal.Transform(location=unreal.Vector(x, y, z),
                                                                           rotation=unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw)),
                          "%sLantern_%d" % (prefix, n), PACKED)
        for piece in kit.lantern_post():
            if piece.key == "LIGHT":
                spawn_piece(piece, (x, y), yaw, z, "%sLanternLight_%d" % (prefix, n))

    # Gates where the roads come in, a palisade round some
    if plan["gates"]:
        for n, (p, direction, width) in enumerate(road_crossings((cx, cy), radius * 0.72)):
            x, y = m2ue(*p)
            yaw = ue_yaw(direction)
            for k, piece in enumerate(kit.gate(width * 100.0 + 200.0)):
                if abandoned and piece.key in ("LANTERN", "LIGHT"):
                    continue
                spawn_piece(piece, (x, y), yaw, ground(x, y), "%sGate_%d_%d" % (prefix, n, k), snap=piece.key in ("POST",))
    if plan["palisade"]:
        ring = radius * 0.8
        count = int(math.ceil(2.0 * math.pi * ring / 3.4))
        placed = 0
        for i in range(count):
            angle = 360.0 * i / count
            p = (cx + math.cos(math.radians(angle)) * ring, cy + math.sin(math.radians(angle)) * ring)
            if road_clearance(p) < 3.0 or not clear_of(p, 1.8, [], road_margin=3.0):
                continue
            x, y = m2ue(*p)
            VLIB.place_prefab(WORLD, prefab_path("Palisade"), unreal.Transform(location=unreal.Vector(x, y, ground(x, y) - 20.0),
                                                                            rotation=unreal.Rotator(roll=0.0, pitch=0.0, yaw=ue_yaw(angle))),
                              "%sPalisade_%03d" % (prefix, i), PACKED)
            placed += 1
        log("%s: palisade of %d runs" % (region_id, placed))
    COUNTS["villages"] += 1
    log("%s: %d of %d houses" % (region_id, len(houses), len(plan["houses"])))


def capture_village(region_id):
    region = world_content.REGIONS[region_id]
    cx, cy = m2ue(*region["centre"])
    radius = float(region["radius"]) * 100.0
    target = unreal.Vector(cx, cy, ground(cx, cy) + 300.0)
    out = os.path.join(SAVED, "villages")
    os.makedirs(out, exist_ok=True)
    location, rotation = look(target, (-0.6, 0.8, 0.75), radius * 1.6)
    VLIB.capture_view(WORLD, location, rotation, os.path.join(out, "%s.png" % region_id), 60.0, 1280, 720, 0.5)
    location, rotation = look(target, (0.7, -0.5, 0.35), radius * 0.75)
    VLIB.capture_view(WORLD, location, rotation, os.path.join(out, "%s_close.png" % region_id), 60.0, 1280, 720, 0.5)


# ---------------------------------------------------------------- 3. people

def place_npcs(region_ids):
    base = bp_class(NPC_ROOT + "/BP_NPC_Base")
    looks = {
        "Manny": (load(MANNEQUINS + "/Meshes/SKM_Manny"), bp_class(MANNEQUINS + "/Animations/ABP_Manny")),
        "Quinn": (load(MANNEQUINS + "/Meshes/SKM_Quinn"), bp_class(MANNEQUINS + "/Animations/ABP_Quinn")),
    }
    existing = {a.get_actor_label(): a for a in ACTORS.get_all_level_actors()}
    placed = {}

    def spawn(cls, label, village, offset, facing):
        if label in existing:
            placed[label] = existing[label]
            return None
        region = world_content.REGIONS[village]
        x, y = m2ue(region["centre"][0] + offset[0], region["centre"][1] + offset[1])
        actor = ACTORS.spawn_actor_from_class(cls, unreal.Vector(x, y, ground(x, y) + 95.0),
                                              unreal.Rotator(roll=0.0, pitch=0.0, yaw=ue_yaw(facing)))
        if actor is None:
            warn("could not place %s" % label)
            return None
        actor.set_actor_label(label)
        placed[label] = actor
        COUNTS["npcs"] += 1
        return actor

    if "village_mossbrook" in region_ids:
        for bp_name, npc_id, offset, facing, _partners in content.MOSSBROOK:
            cls = bp_class("%s/%s" % (NPC_ROOT, bp_name))
            if cls is None:
                warn("%s missing: run migrate_pass13.py" % bp_name)
                continue
            spawn(cls, "WB_NPC_" + npc_id, "village_mossbrook", offset, facing)
        for _bp, npc_id, _offset, _facing, partners in content.MOSSBROOK:
            actor = placed.get("WB_NPC_" + npc_id)
            if actor is not None and partners:
                actor.set_editor_property("chatter_partners", [placed["WB_NPC_" + p] for p in partners if "WB_NPC_" + p in placed])

    if base is None:
        warn("BP_NPC_Base missing: run migrate_pass13.py")
        return
    for npc_id, v in content.VILLAGERS.items():
        if v["village"] not in region_ids:
            continue
        offset, facing = v["at"]
        actor = spawn(base, "WB_NPC_" + npc_id, v["village"], offset, facing)
        if actor is None:
            continue
        mesh, anim = looks[v["look"][0]]
        folder = "Manny" if v["look"][0] == "Manny" else "Quinn"
        materials = [load("%s/Materials/Instances/%s/%s" % (MANNEQUINS, folder, m)) for m in v["look"][1]]
        conversations = []
        for start, required, blocked, once in v.get("conversations", []):
            c = unreal.BeyondNPCConversation()
            c.set_editor_property("start_row", start)
            c.set_editor_property("required_flags", required)
            c.set_editor_property("blocked_by_flags", blocked)
            c.set_editor_property("once", once)
            conversations.append(c)
        chatter = []
        for start, required, blocked in v.get("chatter", []):
            c = unreal.BeyondNPCChatter()
            c.set_editor_property("start_row", start)
            c.set_editor_property("required_flags", required)
            c.set_editor_property("blocked_by_flags", blocked)
            c.set_editor_property("needs_partners", True)
            chatter.append(c)
        for key, value in (("npc_id", npc_id), ("display_name", unreal.Text(v["name"])), ("npc_mesh", mesh), ("npc_anim_class", anim),
                           ("npc_materials", [m for m in materials if m is not None]), ("npc_scale", float(v.get("scale", 1.0))),
                           ("can_talk", bool(conversations)), ("conversations", conversations), ("chatter", chatter),
                           ("greet_rows", list(v.get("greet", []))), ("wander_radius", float(v.get("wander", 0.0)) * 100.0)):
            try:
                actor.set_editor_property(key, value)
            except Exception as error:  # keep going; report at the end
                warn("WB_NPC_%s: could not set %s (%s)" % (npc_id, key, error))
    for npc_id, v in content.VILLAGERS.items():
        actor = placed.get("WB_NPC_" + npc_id)
        if actor is not None and v.get("partners"):
            actor.set_editor_property("chatter_partners", [placed["WB_NPC_" + p] for p in v["partners"] if "WB_NPC_" + p in placed])


def npcs_of(region_id):
    labels = ["WB_NPC_" + npc_id for _bp, npc_id, _o, _f, _p in content.MOSSBROOK] if region_id == "village_mossbrook" else []
    labels += ["WB_NPC_" + npc_id for npc_id, v in content.VILLAGERS.items() if v["village"] == region_id]
    return labels


def step_tables():
    speakers = dict(dialogue_content.SPEAKERS)
    speakers.update(content.SPEAKERS)
    dialogue = dialogue_rows.build_dialogue_rows(content.CONVERSATIONS, content.BANTER, speakers)
    overhead = dialogue_rows.build_overhead_rows(content.CHATTER)
    refs = []
    for npc_id, v in content.VILLAGERS.items():
        refs += [(npc_id, "conversation", c[0]) for c in v.get("conversations", [])]
        refs += [(npc_id, "chatter", c[0]) for c in v.get("chatter", [])]
        refs += [(npc_id, "greeting", g) for g in v.get("greet", [])]
    for problem in dialogue_rows.check_links(dialogue, overhead, speakers, refs):
        warn("village dialogue: broken link %s" % problem)
    portrait = PORTRAIT + "." + PORTRAIT.rsplit("/", 1)[1] if EAL.does_asset_exist(PORTRAIT) else None
    for path, rows in ((DT_SPEAKERS, dialogue_rows.build_speaker_rows(content.SPEAKERS, portrait)),
                       (DT_DIALOGUE, dialogue), (DT_OVERHEAD, overhead)):
        table = load(path)
        if table is None:
            warn("%s missing: is the dialogue pack installed?" % path)
            continue
        added, replaced, missing = dialogue_rows.merge_rows(table, rows, REWRITE)
        if added < 0:
            warn("%s: the rows could not be imported (see the log for the row that failed)" % path)
            continue
        if missing:
            warn("%s: rows missing after the import: %s" % (path, ", ".join(missing[:10])))
        if added or replaced:
            save(table, path)
        log("%s: %d rows added, %d rewritten" % (path, added, replaced))


def step_banter():
    banter = load(BANTER_ASSET)
    if banter is None:
        warn("%s missing: run migrate_pass13.py" % BANTER_ASSET)
        return
    lines = list(banter.get_editor_property("lines"))
    have = {str(line.get_editor_property("id")): i for i, line in enumerate(lines)}
    added = 0
    for banter_id, (trigger, context, once, cooldown, _leader, _lines) in content.BANTER.items():
        if banter_id in have and not REWRITE:
            continue
        line = unreal.BeyondBanterLine()
        line.set_editor_property("id", banter_id)
        line.set_editor_property("trigger", getattr(unreal.BeyondBanterTrigger, TRIGGERS[trigger]))
        line.set_editor_property("context", context or "None")
        line.set_editor_property("start_row", "Banter_" + banter_id)
        line.set_editor_property("once", bool(once))
        line.set_editor_property("cooldown", float(cooldown))
        line.set_editor_property("weight", 1.0)
        if banter_id in have:
            lines[have[banter_id]] = line
        else:
            lines.append(line)
        added += 1
    if added:
        banter.set_editor_property("lines", lines)
        save(banter, BANTER_ASSET)
    log("%s: %d banter lines added or rewritten (%d in all)" % (BANTER_ASSET, added, len(lines)))


# ---------------------------------------------------------------- main

def main():
    global WLIB, VLIB, ACTORS, LEVELS, LAYOUT, WORLD
    log("pass 17")
    missing = [name for name in ("BeyondWorldBuilderLibrary", "BeyondVillageBuilderLibrary", "BeyondNPCCharacter", "BeyondBanterLine")
               if not hasattr(unreal, name)]
    if missing:
        warn("the editor is running an old build (%s missing). Build WorldBeyondEditor first. Nothing was changed." % ", ".join(missing))
        write_report("last_run_pass17.txt")
        raise RuntimeError("C++ not rebuilt: %s missing" % ", ".join(missing))
    WLIB = unreal.BeyondWorldBuilderLibrary
    VLIB = unreal.BeyondVillageBuilderLibrary
    ACTORS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    LEVELS = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    LAYOUT = load(LAYOUT_ASSET)
    if LAYOUT is None or not EAL.does_asset_exist(MAP):
        warn("Dominion or its layout missing: run migrate_pass15.py and migrate_pass16.py first")
        write_report("last_run_pass17.txt")
        return

    load_meshes()
    step_tables()
    step_banter()
    step_prefabs()

    if not LEVELS.load_level(MAP):
        warn("could not open %s" % MAP)
        write_report("last_run_pass17.txt")
        return
    WORLD = editor_world()
    WLIB.load_landscape_proxies(WORLD)
    WLIB.load_actors_with_label_prefix(WORLD, "WB_")
    build_avoid()
    existing = {a.get_actor_label(): a for a in ACTORS.get_all_level_actors()}
    wanted = village_ids_to_build(existing)
    for region_id in wanted:
        prefix = "WB_VIL_%s_" % region_id
        old = [a for label, a in existing.items() if label.startswith(prefix) or label in npcs_of(region_id)]
        if old:
            WLIB.destroy_actors(old)
        build_village(region_id, content.VILLAGES[region_id])
    place_npcs(set(content.VILLAGES))
    retune_lights()
    if not wanted:
        log("villages: all built (BEYOND_REBUILD_VILLAGES=1 or =mossbrook,frostholm builds them again)")
    if wanted and rendering():
        for region_id in wanted:
            capture_village(region_id)
        VLIB.release_capture()
        log("village pictures: %s" % os.path.join(SAVED, "villages"))
    log("built %d prefabs (%d packed), %d villages: %d houses, %d props and lights, %d people" % (
        COUNTS["prefabs"], COUNTS["packed"], COUNTS["villages"], COUNTS["houses"], COUNTS["props"], COUNTS["npcs"]))
    if WLIB.save_world(WORLD):
        log("saved %s" % MAP)
    else:
        warn("some packages of %s could not be saved" % MAP)
    write_report("last_run_pass17.txt")


main()
