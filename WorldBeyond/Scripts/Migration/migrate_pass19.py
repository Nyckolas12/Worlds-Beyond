"""
Worlds Beyond - pass 19: trees, shrubs and rocks for the open world (Plan 5E).

Run with the editor closed, after building the C++ and running passes 15-18:
    UnrealEditor-Cmd.exe <path>/WorldBeyond.uproject -run=pythonscript -script=<path>/Scripts/Migration/migrate_pass19.py -unattended -nosplash -AllowCommandletRendering

1. Measures every mesh flora_content.py names (size, static or skeletal, collision) into the report.
2. Assets in /Game/WorldsBeyond/World/Flora/: an FT_ foliage type per static mesh (paint more by hand with them in
   Foliage mode), the charred dead pine materials, the wind data when the plugin's isn't there. Collision on the meshes:
   the spruce get a capsule round the trunk, cliffs and rocks without simple collision use their triangles. Made when
   missing; BEYOND_REBUILD_FLORA_TYPES=1 makes them (and the collision) again.
3. Places flora_content.LANDMARKS (the sandstone mesas) and scatters flora_content.PLANTINGS over Dominion from the generated terrain (layers, slopes, groves, lake shores,
   regions), clear of the roads, villages, waystones, camps, arenas and places: static meshes into the World Partition
   foliage, the skeletal Megaplants into flora tiles (WB_Flora_<x>_<y>: instanced skinned meshes in the dynamic wind,
   invisible trunk colliders). Only when no flora is there yet: BEYOND_REBUILD_FLORA=1 clears pass 19's flora and
   scatters again, BEYOND_REBUILD_FLORA=forest,frost only in those regions (forest, frost, molten, blight).
4. The wind actor WB_Flora_Wind (always loaded) and the Flora streaming grid (the foliage actors and tiles stream on it,
   loaded further out than the main grid).
5. Pictures in Saved/WorldBuilder/flora/ (with rendering; BEYOND_SKIP_PICTURES=1 skips them), taken after the save. The
   Megaplants show at rest in them: a headless scene capture doesn't run the dynamic wind (an editor viewport and play do).

Every asset saved is copied to Saved/MigrationBackups/<timestamp>/ first. Report: last_run_pass19.txt.
"""
import importlib
import math
import os
import sys
import time

import unreal

sys.path.insert(0, os.path.join(os.path.abspath(unreal.Paths.project_dir()), "Scripts", "Migration"))
from migration_common import ASSET_TOOLS, EAL, load, log, save, warn, write_report  # noqa: E402
import flora_content as content  # noqa: E402
import world_content  # noqa: E402

for module in (content, world_content):
    importlib.reload(module)

MAP = "/Game/WorldsBeyond/Maps/Dominion"
LAYOUT_ASSET = "/Game/WorldsBeyond/World/DA_WorldLayout"
FLORA_DIR = "/Game/WorldsBeyond/World/Flora"
WIND_DATA_PATH = FLORA_DIR + "/DWD_Beyond_Wind"
WIND_LABEL = "WB_Flora_Wind"
TILE_PREFIX = "WB_Flora_"
SAVED = os.path.join(os.path.abspath(unreal.Paths.project_saved_dir()), "WorldBuilder", "flora")

REBUILD_TYPES = os.environ.get("BEYOND_REBUILD_FLORA_TYPES", "") == "1"
REBUILD = os.environ.get("BEYOND_REBUILD_FLORA", "").strip().lower()
SKIP_PICTURES = os.environ.get("BEYOND_SKIP_PICTURES", "") == "1"

KIND_ORDER = ["cliff", "tree", "rock", "shrub"]
REGION_NAMES = {"forest": "Forest", "elderwood": "Forest", "frost": "Frost", "rimewood": "Frost", "molten": "Molten",
                "cinderlands": "Molten", "blight": "Blight", "blightwood": "Blight"}
REGION_TITLES = ["Elderwood", "Rimewood", "Cinderlands", "Blightwood"]

FLIB = None
WLIB = None
VLIB = None
GLIB = None
ACTORS = None
LAYOUT = None
WORLD = None
MESHES = {}
MATERIALS = {}


# ---------------------------------------------------------------- helpers

def rendering():
    return "-nullrhi" not in unreal.SystemLibrary.get_command_line().lower()


def editor_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def m2ue(x, y):
    return x * 100.0, -y * 100.0


def dirty_packages():
    return {p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()}


def save_if_dirty(asset):
    if asset is None:
        return
    path = asset.get_path_name().split(".")[0]
    if path in dirty_packages():
        save(asset, path)


def set_props(struct, values, what):
    for key, value in values.items():
        try:
            struct.set_editor_property(key, value)
        except Exception as error:  # noqa: BLE001 - report every property the build doesn't know
            warn("%s: could not set %s (%s)" % (what, key, error))


def colour(rgb):
    return unreal.LinearColor(float(rgb[0]), float(rgb[1]), float(rgb[2]), 1.0)


def ground(x, y):
    landscape = WLIB.get_ground_height(WORLD, LAYOUT, x, y)
    layout_z = WLIB.get_layout_height(LAYOUT, x, y)
    z = landscape[1] if isinstance(landscape, tuple) and landscape[0] else None
    if z is None or abs(z - layout_z) > 3000.0:
        return layout_z
    return z


def mesh(path):
    """A static or skeletal mesh by path (None and a warning when missing)."""
    if path not in MESHES:
        asset = EAL.load_asset(path) if EAL.does_asset_exist(path) else None
        if asset is None:
            warn("%s not found" % path)
        elif not isinstance(asset, (unreal.StaticMesh, unreal.SkeletalMesh)):
            warn("%s is a %s, not a mesh" % (path, asset.get_class().get_name()))
            asset = None
        MESHES[path] = asset
    return MESHES[path]


def mesh_paths(spec):
    for paths, _weight in spec["meshes"]:
        for path in paths:
            yield path


def nice_name(asset):
    """FT_ name for a mesh: Fab meshes are called <id>_tier_2, their folder says what they are."""
    path = asset.get_path_name().split(".")[0]
    name = asset.get_name()
    if "/Fab/Megascans/3D/" in path:
        folder = path.split("/Fab/Megascans/3D/")[1].split("/")[0]
        name = folder.rsplit("_", 1)[0] if "_" in folder else folder
    return name


def only_regions():
    if REBUILD in ("", "1", "all"):
        return []
    names = []
    for key in REBUILD.split(","):
        key = key.strip()
        if key in REGION_NAMES:
            names.append(REGION_NAMES[key])
        elif key:
            warn("BEYOND_REBUILD_FLORA: no region %r (forest, frost, molten, blight)" % key)
    return names


# ---------------------------------------------------------------- 1. measure

def step_measure():
    used = []
    for spec in content.PLANTINGS.values():
        for path in mesh_paths(spec):
            if path not in used:
                used.append(path)
    log("meshes (%d):" % len(used))
    for path in used:
        asset = mesh(path)
        if asset is None:
            continue
        bounds = asset.get_bounds()
        e = bounds.box_extent
        kind = "static" if isinstance(asset, unreal.StaticMesh) else "skeletal"
        extra = ""
        if isinstance(asset, unreal.StaticMesh):
            extra = ", %d simple collision shapes" % FLIB.get_simple_collision_count(asset)
        log("  %s: %s, %.1f x %.1f x %.1f m (top %.1f m)%s" % (
            asset.get_name(), kind, e.x * 0.02, e.y * 0.02, e.z * 0.02, FLIB.get_mesh_height(asset), extra))


# ---------------------------------------------------------------- 2. assets

def material(name):
    if name in MATERIALS:
        return MATERIALS[name]
    spec = content.MATERIALS.get(name)
    path = "%s/%s" % (FLORA_DIR, name)
    instance = None
    if spec is None:
        warn("no material %s in flora_content.MATERIALS" % name)
    elif EAL.does_asset_exist(path) and not REBUILD_TYPES:
        instance = load(path)
    else:
        parent = load(spec["parent"]) if EAL.does_asset_exist(spec["parent"]) else None
        if parent is None:
            warn("%s: parent %s not found" % (name, spec["parent"]))
        else:
            instance = load(path) if EAL.does_asset_exist(path) else ASSET_TOOLS.create_asset(
                name, FLORA_DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
            unreal.MaterialEditingLibrary.set_material_instance_parent(instance, parent)
            for parameter, value in spec.get("vectors", {}).items():
                unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(instance, parameter, colour(value))
            unreal.MaterialEditingLibrary.update_material_instance(instance)
            save(instance, path)
            log("made %s" % path)
    MATERIALS[name] = instance
    return instance


def wind_data():
    for path in content.WIND_DATA:
        try:
            asset = unreal.load_asset(path)
        except Exception:  # noqa: BLE001 - a missing plugin asset
            asset = None
        if asset is not None:
            return asset
    if EAL.does_asset_exist(WIND_DATA_PATH):
        return load(WIND_DATA_PATH)
    asset = FLIB.create_wind_data(WIND_DATA_PATH)
    if asset is not None:
        save_if_dirty(asset)
        log("made %s (the plugin's wind data isn't there)" % WIND_DATA_PATH)
    else:
        warn("no wind data: the Megaplants stand still")
    return asset


def fix_collision(asset, collision):
    """Collision on a static mesh as a planting asks (saved, backed up first)."""
    path = asset.get_path_name().split(".")[0]
    if isinstance(collision, tuple) and collision[0] == "trunk":
        if FLIB.set_trunk_collision(asset, float(collision[1]), float(collision[2])):
            log("  %s: a trunk capsule (%.2f m, %.1f m tall at scale 1)" % (asset.get_name(), collision[1], collision[2]))
    elif collision == "complex" or (collision == "mesh" and FLIB.get_simple_collision_count(asset) == 0):
        if FLIB.use_complex_as_simple(asset):
            log("  %s: collision from its triangles" % asset.get_name())
    if path in dirty_packages():
        save(asset, path)


def tinted(parent, tints):
    """A child of a mesh's material with tint parameters set (in Flora/Tints; remade with BEYOND_REBUILD_FLORA_TYPES=1)."""
    names = {str(n) for n in unreal.MaterialEditingLibrary.get_vector_parameter_names(parent)}
    values = {k: v for k, v in tints.items() if k in names}
    if not values:
        return None
    name = "MI_Beyond_%s_Tinted" % parent.get_name()
    path = "%s/Tints/%s" % (FLORA_DIR, name)
    if path in MATERIALS:
        return MATERIALS[path]
    if EAL.does_asset_exist(path) and not REBUILD_TYPES:
        instance = load(path)
    else:
        instance = load(path) if EAL.does_asset_exist(path) else ASSET_TOOLS.create_asset(
            name, FLORA_DIR + "/Tints", unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        unreal.MaterialEditingLibrary.set_material_instance_parent(instance, parent)
        for parameter, value in values.items():
            unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(instance, parameter, colour(value))
        unreal.MaterialEditingLibrary.update_material_instance(instance)
        save(instance, path)
    MATERIALS[path] = instance
    return instance


def overrides_for(spec, asset):
    """Material overrides: named instances (MATERIALS), or tinted children of the mesh's own materials (tints)."""
    if spec.get("materials"):
        return [material(name) for name in spec["materials"]]
    tints = content.TINTS.get(spec.get("tint")) if spec.get("tint") else None
    if not tints or asset is None:
        return []
    slots = asset.get_editor_property("static_materials") if isinstance(asset, unreal.StaticMesh) else asset.get_editor_property("materials")
    out = []
    for slot in slots:
        parent = slot.get_editor_property("material_interface")
        out.append(tinted(parent, tints) if parent is not None else None)
    return out if any(out) else []


def foliage_settings(spec, asset=None):
    settings = unreal.BeyondFoliageSettings()
    cull = spec.get("cull", (0, 0))
    paint = spec.get("paint", (0.1, (1.0, 1.0)))
    overrides = overrides_for(spec, asset)
    set_props(settings, {
        "cull_start": float(cull[0]),
        "cull_end": float(cull[1]),
        "cast_shadow": bool(spec.get("shadow", True)),
        "blocks": spec.get("collision") is not None,
        "override_materials": overrides,
        "paint_density": float(paint[0]),
        "paint_scale": unreal.Vector2D(float(paint[1][0]), float(paint[1][1])),
        "paint_sink": float(spec.get("sink_m", 0.0)),
        "paint_max_slope": float(spec.get("slope", (0, 30))[1]),
        "paint_align_to_normal": float(spec.get("align", 0.0)) >= 0.5,
    }, "foliage settings")
    return settings


def step_assets():
    """{static mesh path: foliage type}; collision fixed on the meshes as their first planting asks."""
    types = {}
    made = 0
    for _name, spec in ordered_plantings() + list(content.LANDMARK_KINDS.items()):
        for path in mesh_paths(spec):
            asset = mesh(path)
            if asset is None or not isinstance(asset, unreal.StaticMesh) or path in types:
                continue
            ft_path = "%s/FT_%s" % (FLORA_DIR, nice_name(asset))
            if EAL.does_asset_exist(ft_path) and not REBUILD_TYPES:
                types[path] = load(ft_path)
                continue
            if spec.get("collision") is not None:
                fix_collision(asset, spec["collision"])
            foliage_type = FLIB.create_foliage_type(ft_path, asset, foliage_settings(spec, asset))
            save_if_dirty(foliage_type)
            types[path] = foliage_type
            made += 1
    log("foliage types: %d (%d made now; BEYOND_REBUILD_FLORA_TYPES=1 makes them again)" % (len(types), made))
    return types


# ---------------------------------------------------------------- 3. scatter

def ordered_plantings():
    return sorted(content.PLANTINGS.items(), key=lambda kv: KIND_ORDER.index(kv[1]["kind"]))


def build_planting(name, spec):
    planting = unreal.BeyondFloraPlanting()
    meshes = []
    for paths, weight in spec["meshes"]:
        for path in paths:
            asset = mesh(path)
            if asset is not None:
                entry = unreal.BeyondFloraMesh()
                entry.set_editor_property("mesh", asset)
                entry.set_editor_property("weight", float(weight))
                meshes.append(entry)
    kinds = {"cliff": unreal.BeyondFloraKind.CLIFF, "tree": unreal.BeyondFloraKind.TREE, "rock": unreal.BeyondFloraKind.ROCK,
             "shrub": unreal.BeyondFloraKind.SHRUB}
    regions = spec["regions"]
    values = {
        "name": unreal.Name(name),
        "kind": kinds[spec["kind"]],
        "meshes": meshes,
        "regions": unreal.LinearColor(*[float(v) for v in regions]),
        "layers": [unreal.Name(layer) for layer in spec.get("layers", [])],
        "min_layer_weight": float(spec.get("min_layer", 0.4)),
        "avoid_layers": [unreal.Name(layer) for layer in spec.get("avoid", content.ROAD_LAYERS)],
        "max_avoid_weight": float(spec.get("max_avoid", content.ROAD_WEIGHT)),
        "min_slope": float(spec.get("slope", (0, 30))[0]),
        "max_slope": float(spec.get("slope", (0, 30))[1]),
        "grove_min": float(spec.get("grove", (0.0, 1.0))[0]),
        "grove_max": float(spec.get("grove", (0.0, 1.0))[1]),
        "density": float(spec["density"]),
        "sink": float(spec.get("sink", 0.0)),
        "sink_metres": float(spec.get("sink_m", 0.05)),
        "footprint": float(spec.get("footprint", 0.0)),
        "align_to_slope": float(spec.get("align", 0.0)),
        "max_tilt": float(spec.get("tilt", 0.0)),
        "face_downhill": bool(spec.get("downhill", False)),
        "yaw_offset": float(spec.get("yaw", 0.0)),
        "road_clearance": float(spec.get("road", 4.0)),
    }
    if "size" in spec:
        values["size"] = unreal.Vector2D(float(spec["size"][0]), float(spec["size"][1]))
    if "scale" in spec:
        values["scale"] = unreal.Vector2D(float(spec["scale"][0]), float(spec["scale"][1]))
    if spec.get("shore"):
        values.update({"shore": True, "shore_min": float(spec["shore"][0]), "shore_max": float(spec["shore"][1]),
                       "lakes": [unreal.Name(lake) for lake in spec.get("lakes", [])]})
    if spec.get("altitude"):
        values["altitude"] = unreal.Vector2D(float(spec["altitude"][0]), float(spec["altitude"][1]))
    set_props(planting, values, "planting %s" % name)
    return planting


def exclusion_data():
    """(id, (x, y) m, radius m, kind) of every gameplay site the flora keeps clear of"""
    c = content.CLEARANCE
    out = []
    for region_id, region in world_content.REGIONS.items():
        if region["kind"] == "Village":
            out.append((region_id, region["centre"], region["radius"] + c["village"], "village"))
        elif region["kind"] == "Area":
            out.append((region_id, region["centre"], region["radius"] + c["area"], "area"))
    for waystone in world_content.WAYSTONES:
        out.append(("waystone_" + waystone[0], waystone[2], c["waystone"], "waystone"))
    for camp in world_content.CAMPS:
        out.append((camp[0], camp[1], c["camp"], "camp"))
    for arena in world_content.ARENAS:
        out.append((arena[0], arena[2], arena[5] / 100.0 + c["arena"], "arena"))
    for place in world_content.PLACES:
        if place[2] != "Lake":
            out.append((place[0], place[3], place[4] * c["place_share"], "place"))
    out.append(("player_start", world_content.PLAYER_START[0], c["player_start"], "village"))
    return out


def build_exclusions(extra=()):
    out = []
    for ident, centre, radius, kind in list(exclusion_data()) + list(extra):
        exclusion = unreal.BeyondFloraExclusion()
        set_props(exclusion, {"id": unreal.Name(ident), "centre": unreal.Vector2D(float(centre[0]), float(centre[1])),
                              "radius": float(radius), "shrubs_allowed": kind in content.SHRUBS_AT}, "exclusion %s" % ident)
        out.append(exclusion)
    return out


def road_edge(x, y):
    """Distance (m) from a point to the nearest road's edge"""
    best = 1.0e9
    for _road_id, points, width, _cobble in world_content.LAYOUT["roads"]:
        for (ax, ay), (bx, by) in zip(points, points[1:]):
            dx, dy = bx - ax, by - ay
            t = max(0.0, min(1.0, ((x - ax) * dx + (y - ay) * dy) / (dx * dx + dy * dy or 1.0)))
            best = min(best, math.hypot(x - (ax + t * dx), y - (ay + t * dy)) - width * 0.5)
    return best


def landmarks(regions):
    """The landmarks that fit (with their footprint), in the regions being scattered"""
    out = []
    for name, kind, centre, yaw, scale in content.LANDMARKS:
        spec = content.LANDMARK_KINDS.get(kind)
        asset = mesh(next(mesh_paths(spec))) if spec else None
        if asset is None:
            warn("landmark %s: no kind %s or no mesh" % (name, kind))
            continue
        if regions and str(FLIB.get_region_at(LAYOUT, *m2ue(*centre))) not in regions:
            continue
        extent = asset.get_bounds().box_extent
        radius = max(extent.x, extent.y) * scale / 100.0
        edge = road_edge(*centre)
        clash = [ident for ident, at, r, _kind in exclusion_data() if math.hypot(centre[0] - at[0], centre[1] - at[1]) < r + radius * 0.8]
        if edge < radius + content.LANDMARK_ROAD_MARGIN or clash:
            warn("landmark %s skipped: %s" % (name, ("a road %.0f m away (it reaches %.0f m)" % (edge, radius)) if not clash
                                              else "too close to " + ", ".join(clash)))
            continue
        out.append(dict(name=name, spec=spec, asset=asset, centre=centre, yaw=yaw, scale=scale, radius=radius, extent=extent))
    return out


def place_landmarks(placed, types):
    for item in placed:
        asset, scale, extent = item["asset"], item["scale"], item["extent"]
        x, y = m2ue(*item["centre"])
        # Unreal's yaw turns from east toward the south; the landmark's yaw is a bearing from east toward the north
        yaw = -float(item["yaw"])
        # On the lowest ground under it, so no edge floats
        lowest = ground(x, y)
        for i in range(8):
            lx, ly = extent.x * scale * 0.7 * math.cos(math.radians(i * 45.0)), extent.y * scale * 0.7 * math.sin(math.radians(i * 45.0))
            px = x + lx * math.cos(math.radians(yaw)) - ly * math.sin(math.radians(yaw))
            py = y + lx * math.sin(math.radians(yaw)) + ly * math.cos(math.radians(yaw))
            lowest = min(lowest, ground(px, py))
        height = FLIB.get_mesh_height(asset) * 100.0 * scale
        z = lowest - item["spec"].get("sink", 0.0) * height
        transform = unreal.Transform(unreal.Vector(x, y, z), unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw), unreal.Vector(scale, scale, scale))
        foliage_type = types.get(asset.get_path_name().split(".")[0])
        if foliage_type is None:
            warn("landmark %s: no foliage type" % item["name"])
            continue
        unreal.InstancedFoliageActor.add_instances(WORLD, foliage_type, [transform])
        log("landmark %s: %s, %.0f m long, at (%.0f, %.0f)" % (item["name"], asset.get_name(), item["radius"] * 2.0, *item["centre"]))


def skinned_settings(spec, wind):
    settings = unreal.BeyondSkinnedFloraSettings()
    cull = spec.get("cull", (0, 0))
    collision = spec.get("collision")
    trunk = collision if isinstance(collision, tuple) and collision[0] == "trunk" else (None, 0.0, 0.0)
    set_props(settings, {
        "cull_start": float(cull[0]),
        "cull_end": float(cull[1]),
        "cast_shadow": bool(spec.get("shadow", True)),
        "wind": wind,
        "override_materials": [material(name) for name in spec.get("materials", [])],
        "trunk_radius": float(trunk[1]),
        "trunk_height": float(trunk[2]),
        "tile_size": float(content.TILE_SIZE),
        "label_prefix": TILE_PREFIX,
        "runtime_grid": unreal.Name(content.FLORA_GRID["name"]),
    }, "skinned settings")
    return settings


def all_foliage_types():
    """Every FT_ in the Flora folder (also those a changed flora_content.py no longer names)."""
    types = []
    for path in EAL.list_assets(FLORA_DIR, recursive=False, include_folder=False):
        path = path.split(".")[0]
        if path.rsplit("/", 1)[1].startswith("FT_"):
            asset = load(path)
            if isinstance(asset, unreal.FoliageType):
                types.append(asset)
    return types


def flora_count():
    foliage = sum(FLIB.count_foliage_instances(WORLD, ft) for ft in all_foliage_types())
    plants = sum(tile.get_plant_count() for tile in unreal.GameplayStatics.get_all_actors_of_class(WORLD, unreal.BeyondFloraTile))
    return foliage, plants


def step_scatter(types, wind):
    regions = only_regions()
    foliage, plants = flora_count()
    if foliage + plants > 0 and not REBUILD:
        log("flora: already there (%d foliage instances, %d Megaplants; BEYOND_REBUILD_FLORA=1 scatters again)" % (foliage, plants))
        return False
    if foliage + plants > 0:
        removed = FLIB.clear_flora(WORLD, LAYOUT, all_foliage_types(), regions)
        log("flora: cleared %d instances %s" % (removed, "in " + ", ".join(regions) if regions else "everywhere"))

    plantings = ordered_plantings()
    specs = dict(plantings)
    placed_landmarks = landmarks(regions)
    place_landmarks(placed_landmarks, types)
    kept_clear = [(item["name"], item["centre"], item["radius"] * 0.9, "landmark") for item in placed_landmarks]
    started = time.time()
    batches = FLIB.compute_flora(WORLD, LAYOUT, [build_planting(name, spec) for name, spec in plantings], build_exclusions(kept_clear),
                                 int(content.SEED), [unreal.Name(r) for r in regions], float(content.GROVE_SCALE))
    log("scattered in %.1f s: %d batches" % (time.time() - started, len(batches)))

    totals = {}
    skinned_tiles = set()
    started = time.time()
    for batch in batches:
        name = str(batch.get_editor_property("planting"))
        spec = specs[name]
        asset = batch.get_editor_property("mesh")
        transforms = batch.get_editor_property("transforms")
        counts = list(batch.get_editor_property("region_counts"))
        total = totals.setdefault(name, [0, 0, 0, 0])
        for index, count in enumerate(counts[:4]):
            total[index] += count
        if isinstance(asset, unreal.StaticMesh):
            foliage_type = types.get(asset.get_path_name().split(".")[0])
            if foliage_type is None:
                warn("%s: no foliage type for %s" % (name, asset.get_name()))
                continue
            unreal.InstancedFoliageActor.add_instances(WORLD, foliage_type, transforms)
        else:
            touched = FLIB.place_skinned_flora(WORLD, asset, transforms, skinned_settings(spec, wind))
            skinned_tiles.add((name, touched))
    log("placed in %.1f s" % (time.time() - started))
    log("%-18s %8s %8s %8s %8s %8s" % ("planting", "total", *[t[:8] for t in REGION_TITLES]))
    grand = 0
    for name, _spec in plantings:
        total = totals.get(name, [0, 0, 0, 0])
        grand += sum(total)
        log("%-18s %8d %8d %8d %8d %8d" % (name, sum(total), *total))
    log("flora: %d placed" % grand)
    return True


def step_verify():
    """After the save: every foliage actor and flora tile has its file on disk"""
    FLIB.load_flora_actors(WORLD)
    content_dir = os.path.abspath(unreal.Paths.project_content_dir())
    actors = list(unreal.GameplayStatics.get_all_actors_of_class(WORLD, unreal.InstancedFoliageActor)) +         list(unreal.GameplayStatics.get_all_actors_of_class(WORLD, unreal.BeyondFloraTile))
    missing = []
    for actor in actors:
        package = actor.get_package().get_name()
        if package.startswith("/Game/") and not os.path.exists(os.path.join(content_dir, package[len("/Game/"):] + ".uasset")):
            missing.append(actor.get_actor_label())
    foliage, plants = flora_count()
    log("after the save: %d foliage actors and tiles (%d foliage instances, %d Megaplants), %d without a file" % (
        len(actors), foliage, plants, len(missing)))
    if missing:
        warn("saved flora actors missing on disk: %s" % ", ".join(missing[:10]))


# ---------------------------------------------------------------- 4. wind, streaming

def step_wind():
    WLIB.load_actors_with_label_prefix(WORLD, WIND_LABEL)
    existing = [a for a in ACTORS.get_all_level_actors() if a.get_actor_label() == WIND_LABEL]
    if existing:
        return existing[0]
    # By its class path: the plugin's content isn't in a headless run's asset registry (load_blueprint_class fails)
    try:
        wind_class = unreal.load_class(None, content.WIND_ACTOR + "." + content.WIND_ACTOR.rsplit("/", 1)[1] + "_C")
    except Exception:  # noqa: BLE001
        wind_class = None
    if wind_class is None:
        warn("no wind actor (%s): the Megaplants sway only with the default wind" % content.WIND_ACTOR)
        return None
    sx, sy = m2ue(*world_content.PLAYER_START[0])
    actor = ACTORS.spawn_actor_from_class(wind_class, unreal.Vector(sx, sy, ground(sx, sy) + 2000.0), unreal.Rotator(0.0, 0.0, 0.0))
    if actor is None:
        warn("could not place the wind actor")
        return None
    actor.set_actor_label(WIND_LABEL)
    actor.set_folder_path("Flora")
    WLIB.set_spatially_loaded(actor, False)
    log("placed %s (%s)" % (WIND_LABEL, wind_class.get_name()))
    return actor


def step_streaming():
    grid = content.FLORA_GRID
    if FLIB.configure_flora_streaming(WORLD, unreal.Name(grid["name"]), float(grid["cell"]), float(grid["range"])):
        changed = FLIB.assign_flora_grid(WORLD, unreal.Name(grid["name"]))
        log("streaming: the %s grid (%.0f m cells, loaded within %.0f m), %d foliage actors and tiles moved onto it" % (
            grid["name"], grid["cell"], grid["range"], changed))
    else:
        warn("streaming: no %s grid (the flora streams with the main grid)" % grid["name"])


# ---------------------------------------------------------------- 5. pictures

def look_at(eye, target):
    dx, dy, dz = target.x - eye.x, target.y - eye.y, target.z - eye.z
    yaw = math.degrees(math.atan2(dy, dx))
    pitch = math.degrees(math.atan2(dz, math.sqrt(dx * dx + dy * dy)))
    return unreal.Rotator(roll=0.0, pitch=pitch, yaw=yaw)


def view_target(spec, direction):
    if isinstance(spec, str):
        x, y = world_content.REGIONS[spec]["centre"]
    elif spec[0] == "lake":
        lake = next(l for l in world_content.LAYOUT["lakes"] if l[0] == spec[1])
        length = math.hypot(*direction) or 1.0
        x = lake[1][0] + direction[0] / length * (lake[2] - 15.0)
        y = lake[1][1] + direction[1] / length * (lake[2] - 15.0)
    elif spec[0] == "volcano":
        x, y = world_content.LAYOUT["volcano"]["centre"]
    else:
        x, y = spec
    ux, uy = m2ue(x, y)
    return unreal.Vector(ux, uy, ground(ux, uy))


def shots():
    out = []
    for name, spec, direction in content.VIEWS:
        target = view_target(spec, direction)
        length = math.hypot(*direction) or 1.0
        dx, dy = direction[0] / length * 100.0, -direction[1] / length * 100.0
        back = 40.0 if isinstance(spec, tuple) and spec[0] == "lake" else 30.0
        eye_xy = (target.x + dx * back, target.y + dy * back)
        eye = unreal.Vector(eye_xy[0], eye_xy[1], ground(*eye_xy) + 170.0)
        out.append(("%s_eye" % name, eye, look_at(eye, unreal.Vector(target.x, target.y, target.z + 300.0))))
        high = unreal.Vector(target.x + dx * 110.0, target.y + dy * 110.0, target.z + 6000.0)
        out.append(("%s_aerial" % name, high, look_at(high, target)))
    for name, (x, y), height in (("overview_elderwood", (-400, -900), 350.0), ("overview_north", (0, 1000), 450.0),
                                 ("overview_blightwood", (1300, -800), 300.0)):
        ux, uy = m2ue(x, y)
        target = unreal.Vector(ux, uy, ground(ux, uy))
        eye = unreal.Vector(ux - 30000.0, uy + 30000.0, target.z + height * 100.0)
        out.append((name, eye, look_at(eye, target)))
    return out


def step_pictures():
    if SKIP_PICTURES or not rendering():
        log("pictures skipped (%s)" % ("BEYOND_SKIP_PICTURES=1" if SKIP_PICTURES else "no rendering"))
        return
    # The save let everything go: load it again (nothing is saved after this)
    WLIB.load_landscape_proxies(WORLD)
    WLIB.load_actors_with_label_prefix(WORLD, "WB_")
    FLIB.load_flora_actors(WORLD)
    unreal.SystemLibrary.execute_console_command(WORLD, "r.Streaming.FullyLoadUsedTextures 1")
    # The dynamic wind doesn't run for a headless scene capture (no Megaplant would show): they're taken at rest
    unreal.SystemLibrary.execute_console_command(WORLD, "r.InstancedSkinnedMeshes.ForceRefPose 1")
    planned = shots()
    instances = GLIB.grow_grass_at(WORLD, [location for _name, location, _rotation in planned])
    log("grass grown around %d cameras (%d instances ready at once)" % (len(planned), instances))
    os.makedirs(SAVED, exist_ok=True)
    taken = 0
    for name, location, rotation in planned:
        taken += VLIB.capture_view(WORLD, location, rotation, os.path.join(SAVED, "%s.png" % name), 70.0, 1280, 720, 0.5, 0.0, 30)
    VLIB.release_capture()
    unreal.SystemLibrary.execute_console_command(WORLD, "r.InstancedSkinnedMeshes.ForceRefPose 0")
    log("pictures: %d of %d in %s (the Megaplants at rest: the wind needs a viewport)" % (taken, len(planned), SAVED))


# ---------------------------------------------------------------- main

def main():
    global FLIB, WLIB, VLIB, GLIB, ACTORS, LAYOUT, WORLD
    started = time.time()
    log("pass 19")
    missing = [name for name in ("BeyondFloraLibrary", "BeyondFloraTile", "BeyondFloraPlanting", "BeyondWorldBuilderLibrary",
                                 "BeyondVillageBuilderLibrary", "BeyondGroundLibrary") if not hasattr(unreal, name)]
    if missing:
        warn("the editor is running an old build (%s missing). Close the editor, build WorldBeyondEditor (Development Editor, "
             "Win64) until it says Build succeeded, then run this script again. Nothing was changed." % ", ".join(missing))
        write_report("last_run_pass19.txt")
        raise RuntimeError("C++ not rebuilt: %s missing" % ", ".join(missing))
    FLIB = unreal.BeyondFloraLibrary
    WLIB = unreal.BeyondWorldBuilderLibrary
    VLIB = unreal.BeyondVillageBuilderLibrary
    GLIB = unreal.BeyondGroundLibrary
    ACTORS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    LAYOUT = load(LAYOUT_ASSET)
    if LAYOUT is None or not EAL.does_asset_exist(MAP):
        warn("Dominion or its layout missing: run migrate_pass15.py to migrate_pass18.py first")
        write_report("last_run_pass19.txt")
        return

    step_measure()
    types = step_assets()
    wind = wind_data()

    if not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(MAP):
        warn("could not open %s" % MAP)
        write_report("last_run_pass19.txt")
        return
    WORLD = editor_world()
    WLIB.load_landscape_proxies(WORLD)
    WLIB.load_actors_with_label_prefix(WORLD, "WB_")
    FLIB.load_flora_actors(WORLD)

    step_scatter(types, wind)
    step_wind()
    step_streaming()

    if WLIB.save_world(WORLD):
        log("saved %s" % MAP)
    else:
        warn("some packages of %s could not be saved" % MAP)
    step_verify()
    step_pictures()
    log("pass 19 took %.1f min" % ((time.time() - started) / 60.0))
    write_report("last_run_pass19.txt")


main()
