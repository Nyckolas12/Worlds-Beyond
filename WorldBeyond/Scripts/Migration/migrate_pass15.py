"""
Worlds Beyond - pass 15: the open world's terrain and shell (Plan 5B).

Run with the editor closed, after building the C++. The landscape needs the GPU, so this one runs WITH rendering:
    UnrealEditor-Cmd.exe <path>/WorldBeyond.uproject -run=pythonscript -script=<path>/Scripts/Migration/migrate_pass15.py -unattended -nosplash -AllowCommandletRendering

- DA_WorldLayout (/Game/WorldsBeyond/World/) from world_content.py: the four regions' terrain, ridges and passes, peaks,
  the volcano, lakes, roads and flat pads for villages, arenas, waystones and camps. Only when missing;
  BEYOND_REBUILD_LAYOUT=1 writes it again.
- A preview of the terrain in Saved/WorldBuilder/ (preview.png, heightmap.png / .r16, a weight PNG per layer).
- The greybox landscape material M_Beyond_LandscapeGreybox and a layer info per ground layer
  (/Game/WorldsBeyond/World/Landscape/); BEYOND_REBUILD_LANDSCAPE_MATERIAL=1 rebuilds the material.
- /Game/WorldsBeyond/Maps/Dominion, a World Partition map, if it doesn't exist: the landscape (4033 x 4033, 1 m,
  "Generated" + an empty "Sculpt" edit layer, streaming proxies of 2 x 2 components), the sky (sun, sky light,
  atmosphere, clouds, height fog; always loaded) and the open world's navigation (built around the demigods).
  An existing landscape is never touched unless BEYOND_REBUILD_TERRAIN=1 (that deletes it and your sculpting).

Every asset saved is copied to Saved/MigrationBackups/<timestamp>/ first. Report: last_run_pass15.txt.
"""
import importlib
import os
import sys

import unreal

sys.path.insert(0, os.path.join(os.path.abspath(unreal.Paths.project_dir()), "Scripts", "Migration"))
from migration_common import ASSET_TOOLS, EAL, load, log, save, warn, write_report  # noqa: E402
import world_content as content  # noqa: E402

importlib.reload(content)

WORLD = "/Game/WorldsBeyond/World"
LAYOUT_ASSET = WORLD + "/DA_WorldLayout"
LANDSCAPE_DIR = WORLD + "/Landscape"
GREYBOX = LANDSCAPE_DIR + "/M_Beyond_LandscapeGreybox"
LAYERS_DIR = LANDSCAPE_DIR + "/Layers"
MAP = "/Game/WorldsBeyond/Maps/Dominion"

REBUILD_LAYOUT = os.environ.get("BEYOND_REBUILD_LAYOUT", "") == "1"
REBUILD_MATERIAL = os.environ.get("BEYOND_REBUILD_LANDSCAPE_MATERIAL", "") == "1"
REBUILD_TERRAIN = os.environ.get("BEYOND_REBUILD_TERRAIN", "") == "1"

LIB = None
STYLES = {}
SURFACES = {}


def v2(point):
    return unreal.Vector2D(float(point[0]), float(point[1]))


def colour(values):
    return unreal.LinearColor(*(list(values) + [1.0] * (4 - len(values))))


def struct(cls, **props):
    value = cls()
    for key, item in props.items():
        value.set_editor_property(key, item)
    return value


# ---------------------------------------------------------------- the layout

def pad(pad_id, centre, radius, falloff, surface, offset=0.0):
    return struct(unreal.BeyondLayoutPad, id=unreal.Name(pad_id), centre=v2(centre), radius=float(radius), falloff=float(falloff),
                  height_offset=float(offset), surface=SURFACES[surface])


def step_layout():
    exists = EAL.does_asset_exist(LAYOUT_ASSET)
    if exists and not REBUILD_LAYOUT:
        log("%s kept (BEYOND_REBUILD_LAYOUT=1 writes it again)" % LAYOUT_ASSET)
        return load(LAYOUT_ASSET)
    layout = load(LAYOUT_ASSET) if exists else ASSET_TOOLS.create_asset("DA_WorldLayout", WORLD, unreal.BeyondWorldLayout, unreal.DataAssetFactory())
    if layout is None:
        warn("could not create %s" % LAYOUT_ASSET)
        return None
    L = content.LAYOUT
    layout.set_editor_property("seed", int(L["seed"]))
    layout.set_editor_property("components_per_side", int(L["components"]))
    layout.set_editor_property("sections_per_component", int(L["sections"]))
    layout.set_editor_property("quads_per_section", int(L["quads"]))
    layout.set_editor_property("height_scale_z", float(L["height_scale_z"]))
    layout.set_editor_property("border_width", float(L["border_width"]))
    layout.set_editor_property("border_height", float(L["border_height"]))
    layout.set_editor_property("region_blend", float(L["region_blend"]))
    layout.set_editor_property("border_warp", float(L["border_warp"]))
    layout.set_editor_property("border_warp_scale", float(L["border_warp_scale"]))

    regions = []
    for region_id, (style, base, hills, scale) in content.TERRAIN.items():
        r = content.REGIONS[region_id]
        regions.append(struct(unreal.BeyondLayoutRegion, region_id=unreal.Name(region_id), style=STYLES[style],
                              polygon=[v2(p) for p in r["polygon"]], base_height=float(base), hill_height=float(hills),
                              hill_scale=float(scale), map_colour=colour(r["colour"])))
    layout.set_editor_property("regions", regions)

    layout.set_editor_property("ridges", [
        struct(unreal.BeyondLayoutRidge, points=[v2(p) for p in points], height=float(height), width=float(width),
               passes=[v2(p) for p in passes], pass_width=float(pass_width))
        for points, height, width, passes, pass_width in L["ridges"]])
    layout.set_editor_property("peaks", [
        struct(unreal.BeyondLayoutPeak, centre=v2(centre), radius=float(radius), height=float(height), sharpness=float(sharp))
        for centre, radius, height, sharp in L["peaks"]])
    vol = L["volcano"]
    layout.set_editor_property("volcano", struct(
        unreal.BeyondLayoutVolcano, enabled=True, centre=v2(vol["centre"]), radius=float(vol["radius"]),
        rim_radius=float(vol["rim_radius"]), rim_height=float(vol["rim_height"]), floor_height=float(vol["floor_height"]),
        lava_level=float(vol["lava_level"]), breach_bearing=float(vol["breach_bearing"]), breach_half_angle=float(vol["breach_half_angle"])))
    layout.set_editor_property("lakes", [
        struct(unreal.BeyondLayoutLake, id=unreal.Name(lake_id), centre=v2(centre), radius=float(radius), depth=float(depth), frozen=bool(frozen))
        for lake_id, centre, radius, depth, frozen in L["lakes"]])
    layout.set_editor_property("roads", [
        struct(unreal.BeyondLayoutRoad, id=unreal.Name(road_id), points=[v2(p) for p in points], width=float(width), cobble=bool(cobble))
        for road_id, points, width, cobble in L["roads"]])

    # Pads, later ones win: camps and arenas, waystones, then villages and the crossroads
    pads = []
    for camp_id, centre, _entries, _offset, _rule in content.CAMPS:
        pads.append(pad("camp_" + camp_id, centre, 12, 12, "Dirt"))
    for arena_id, _name, centre, boss, _level, radius in content.ARENAS:
        pads.append(pad("arena_" + arena_id, centre, radius / 100.0 + 6.0, 25, "Rock" if boss else "Ground"))
    for stone_id, _name, centre, _yaw, _attuned in content.WAYSTONES:
        pads.append(pad("waystone_" + stone_id, centre, 9, 10, "Cobble"))
    for region_id, r in content.REGIONS.items():
        if r["kind"] == "Village":
            pads.append(pad(region_id, r["centre"], r["radius"] * 0.85, 30, "Dirt"))
        elif r["kind"] == "Area":
            pads.append(pad(region_id, r["centre"], r["radius"] * 0.6, 30, "Dirt"))
    mossbrook = content.REGIONS["village_mossbrook"]["centre"]
    pads.append(pad("mossbrook_square", mossbrook, 18, 6, "Cobble"))
    layout.set_editor_property("pads", pads)

    save(layout, LAYOUT_ASSET)
    log("%s: %d regions, %d ridges, %d peaks, %d lakes, %d roads, %d pads" % (
        LAYOUT_ASSET, len(regions), len(L["ridges"]), len(L["peaks"]), len(L["lakes"]), len(L["roads"]), len(pads)))
    return layout


# ---------------------------------------------------------------- material and layers

def dirty_packages():
    return {p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()}


def save_if_new(asset):
    """Saves an asset the world builder just made or changed (its package is dirty)."""
    path = asset.get_path_name().split(".")[0]
    if path in dirty_packages():
        save(asset, path)


def step_material():
    material = LIB.create_greybox_landscape_material(GREYBOX, REBUILD_MATERIAL)
    if material is None:
        warn("could not build %s" % GREYBOX)
        return None, []
    save_if_new(material)
    infos = LIB.create_layer_infos(LAYERS_DIR)
    for info in infos:
        if info is not None:
            save_if_new(info)
    log("landscape material %s, %d layer infos in %s" % (GREYBOX, len(infos), LAYERS_DIR))
    return material, infos


# ---------------------------------------------------------------- the map

def editor_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def actors_of(world, cls):
    return list(unreal.GameplayStatics.get_all_actors_of_class(world, cls))


def step_map():
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if EAL.does_asset_exist(MAP):
        if not levels.load_level(MAP):
            warn("could not open %s" % MAP)
            return None
        log("opened %s" % MAP)
    else:
        if not levels.new_level(MAP, True):
            warn("could not create %s" % MAP)
            return None
        log("created %s (World Partition)" % MAP)
    return editor_world()


def step_landscape(world, layout, material, infos):
    existing = actors_of(world, unreal.Landscape)
    if existing and not REBUILD_TERRAIN:
        log("the landscape exists (BEYOND_REBUILD_TERRAIN=1 replaces it, losing hand sculpting)")
        return existing[0]
    if existing:
        LIB.load_landscape_proxies(world)
        removed = LIB.delete_landscapes(world)
        log("deleted the old landscape (%d actors)" % removed)
    landscape = LIB.create_landscape(world, layout, material, infos, 2)
    if landscape is None:
        warn("could not make the landscape: run the pass in the editor or import Saved/WorldBuilder/heightmap.r16 by hand")
        return None
    proxies = len(actors_of(world, unreal.LandscapeStreamingProxy))
    log("made the landscape: %d streaming proxies" % proxies)
    return landscape


def spawn(cls, label, location, rotation=None):
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actor = actors.spawn_actor_from_class(cls, location, rotation or unreal.Rotator(0.0, 0.0, 0.0))
    if actor is None:
        warn("could not place %s" % label)
        return None
    actor.set_actor_label(label)
    LIB.set_spatially_loaded(actor, False)
    return actor


def step_shell(world):
    labels = {a.get_actor_label() for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()}
    placed = 0
    if "WB_Sun" not in labels:
        sun = spawn(unreal.DirectionalLight, "WB_Sun", unreal.Vector(0.0, 0.0, 60000.0), unreal.Rotator(roll=0.0, pitch=-38.0, yaw=-35.0))
        if sun is not None:
            light = sun.get_component_by_class(unreal.DirectionalLightComponent)
            light.set_editor_property("intensity", 9.0)
            light.set_editor_property("atmosphere_sun_light", True)
            light.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
            placed += 1
    if "WB_SkyLight" not in labels:
        sky = spawn(unreal.SkyLight, "WB_SkyLight", unreal.Vector(0.0, 0.0, 60000.0))
        if sky is not None:
            component = sky.get_component_by_class(unreal.SkyLightComponent)
            component.set_editor_property("real_time_capture", True)
            component.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
            placed += 1
    if "WB_SkyAtmosphere" not in labels and spawn(unreal.SkyAtmosphere, "WB_SkyAtmosphere", unreal.Vector(0.0, 0.0, 0.0)) is not None:
        placed += 1
    if "WB_Clouds" not in labels and spawn(unreal.VolumetricCloud, "WB_Clouds", unreal.Vector(0.0, 0.0, 0.0)) is not None:
        placed += 1
    if "WB_HeightFog" not in labels:
        fog = spawn(unreal.ExponentialHeightFog, "WB_HeightFog", unreal.Vector(0.0, 0.0, 2000.0))
        if fog is not None:
            component = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
            component.set_fog_density(0.02)
            component.set_volumetric_fog(True)
            placed += 1
    log("sky: %d actors placed" % placed)
    if LIB.configure_open_world(world):
        log("navigation: built around the demigods at runtime")


# ---------------------------------------------------------------- main

def main():
    global LIB
    log("pass 15")
    missing = [name for name in ("BeyondWorldLayout", "BeyondWorldBuilderLibrary", "BeyondLayoutRegion", "BeyondOpenWorldNavigationSystem")
               if not hasattr(unreal, name)]
    if missing:
        warn("the editor is running an old build (%s missing). Close the editor, build WorldBeyondEditor (Development Editor, "
             "Win64) until it says Build succeeded, then run this script again. Nothing was changed." % ", ".join(missing))
        write_report("last_run_pass15.txt")
        raise RuntimeError("C++ not rebuilt: %s missing" % ", ".join(missing))
    LIB = unreal.BeyondWorldBuilderLibrary
    STYLES.update({"Forest": unreal.BeyondTerrainStyle.FOREST, "Blight": unreal.BeyondTerrainStyle.BLIGHT,
                   "Frost": unreal.BeyondTerrainStyle.FROST, "Molten": unreal.BeyondTerrainStyle.MOLTEN})
    SURFACES.update({"Dirt": unreal.BeyondPadSurface.DIRT, "Cobble": unreal.BeyondPadSurface.COBBLE,
                     "Rock": unreal.BeyondPadSurface.ROCK, "Ground": unreal.BeyondPadSurface.GROUND})

    layout = step_layout()
    if layout is None:
        write_report("last_run_pass15.txt")
        return
    LIB.clear_terrain_cache()
    folder = LIB.write_terrain_preview(layout)
    log("terrain preview: %s" % folder)
    material, infos = step_material()
    world = step_map()
    if world is None:
        write_report("last_run_pass15.txt")
        return
    step_landscape(world, layout, material, infos)
    step_shell(world)
    if LIB.save_world(world):
        log("saved %s" % MAP)
    else:
        warn("some packages of %s could not be saved" % MAP)
    write_report("last_run_pass15.txt")


main()
