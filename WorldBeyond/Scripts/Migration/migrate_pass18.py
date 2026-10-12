"""
Worlds Beyond - pass 18: the open world's ground (Plan 5D).

Run with the editor closed, after building the C++ and running passes 15-17:
    UnrealEditor-Cmd.exe <path>/WorldBeyond.uproject -run=pythonscript -script=<path>/Scripts/Migration/migrate_pass18.py -unattended -nosplash -AllowCommandletRendering

1. The textured landscape (from ground_content.py): M_Beyond_Landscape, a height blend of the 11 ground layers (Megascans,
   the village sample's soils, Fab's wild grass and snow, Paragon rock / gravel), each a set of texture parameters; the
   region mask T_Beyond_RegionMask grades the Blightwood (violet, desaturated), the Rimewood (cold; trampled snow on its
   paths, snow between cobbles and on rock tops) and the Cinderlands (warm, charred; embers in the ash). Its instance
   MI_Beyond_Landscape goes on the landscape and all 256 streaming proxies; tune it there. Six grass types grow grass,
   ferns, moss, twigs, dead weeds, snow clumps and stones on their layers. The ground textures are capped at 2048 px
   (BEYOND_GROUND_FULL_RES=1 leaves them). Only made when missing: BEYOND_REBUILD_GROUND=1 rebuilds the material, the
   mask and the grass types (the instance keeps your changes; BEYOND_RESET_GROUND_INSTANCE=1 clears them).
2. Water, ice, lava: M_Beyond_Water (Single Layer Water; MI_Water_Elderwood, MI_Water_Blight), M_Beyond_Ice, M_Beyond_Lava
   in /Game/WorldsBeyond/World/Water/; surfaces on Mirrormere, Reedpool, Blackmere (water, deep water under their
   middles), Glassmere (ice you can walk on) and the caldera (lava pools just under Kael'thar's floor, which burn).
   Labels WB_Water_*, WB_Ice_*, WB_Lava_*, WB_Hazard_*; kept when they exist, BEYOND_REBUILD_WATER=1 makes them again.
3. Pictures in Saved/WorldBuilder/ground/ (with rendering; BEYOND_SKIP_PICTURES=1 skips them), taken after the save with
   the grass grown around each camera (that dirties the landscape, so nothing is saved after them).

Every asset saved is copied to Saved/MigrationBackups/<timestamp>/ first. Report: last_run_pass18.txt.
"""
import importlib
import math
import os
import sys

import unreal

sys.path.insert(0, os.path.join(os.path.abspath(unreal.Paths.project_dir()), "Scripts", "Migration"))
from migration_common import ASSET_TOOLS, EAL, load, log, save, warn, write_report  # noqa: E402
import ground_content as content  # noqa: E402
import world_content  # noqa: E402

for module in (content, world_content):
    importlib.reload(module)

MAP = "/Game/WorldsBeyond/Maps/Dominion"
LAYOUT_ASSET = "/Game/WorldsBeyond/World/DA_WorldLayout"
LANDSCAPE_DIR = "/Game/WorldsBeyond/World/Landscape"
GROUND_MATERIAL = LANDSCAPE_DIR + "/M_Beyond_Landscape"
GROUND_INSTANCE = LANDSCAPE_DIR + "/MI_Beyond_Landscape"
REGION_MASK = LANDSCAPE_DIR + "/T_Beyond_RegionMask"
GRASS_DIR = LANDSCAPE_DIR + "/Grass"
WATER_DIR = "/Game/WorldsBeyond/World/Water"
WATER_MATERIAL = WATER_DIR + "/M_Beyond_Water"
ICE_MATERIAL = WATER_DIR + "/M_Beyond_Ice"
LAVA_MATERIAL = WATER_DIR + "/M_Beyond_Lava"
CYLINDER = "/Engine/BasicShapes/Cylinder"
SAVED = os.path.join(os.path.abspath(unreal.Paths.project_saved_dir()), "WorldBuilder", "ground")

REBUILD_GROUND = os.environ.get("BEYOND_REBUILD_GROUND", "") == "1"
RESET_INSTANCE = os.environ.get("BEYOND_RESET_GROUND_INSTANCE", "") == "1"
REBUILD_WATER = os.environ.get("BEYOND_REBUILD_WATER", "") == "1"
FULL_RES = os.environ.get("BEYOND_GROUND_FULL_RES", "") == "1"
SKIP_PICTURES = os.environ.get("BEYOND_SKIP_PICTURES", "") == "1"
MAX_TEXTURE = 2048

MEL = unreal.MaterialEditingLibrary
GLIB = None
WLIB = None
VLIB = None
ACTORS = None
LAYOUT = None
WORLD = None
USED_TEXTURES = {}


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
    """Saves an asset the C++ just made or changed (its package is dirty)."""
    if asset is None:
        return
    path = asset.get_path_name().split(".")[0]
    if path in dirty_packages():
        save(asset, path)


def first_asset(candidates, what, kind=None):
    """The first candidate that exists (and is a kind), or None."""
    for path in candidates or []:
        if EAL.does_asset_exist(path):
            asset = EAL.load_asset(path)
            if asset is not None and (kind is None or isinstance(asset, kind)):
                if candidates.index(path) > 0:
                    log("%s: using %s (the first choice is missing)" % (what, path))
                return asset
    if candidates:
        warn("%s: none of %s found" % (what, ", ".join(candidates)))
    return None


def texture(candidates, what):
    tex = first_asset(candidates, what, unreal.Texture)
    if tex is not None:
        USED_TEXTURES[tex.get_path_name().split(".")[0]] = tex
    return tex


def ground(x, y):
    """Ground height (cm) at Unreal X / Y: the landscape, or the layout's terrain where it isn't loaded."""
    landscape = WLIB.get_ground_height(WORLD, LAYOUT, x, y)
    layout_z = WLIB.get_layout_height(LAYOUT, x, y)
    z = landscape[1] if isinstance(landscape, tuple) and landscape[0] else None
    if z is None or abs(z - layout_z) > 3000.0:
        return layout_z
    return z


def lake_level(lake_id):
    result = WLIB.get_lake_level(LAYOUT, unreal.Name(lake_id))
    if isinstance(result, tuple):
        return result[1] if result[0] else None
    return result


def colour(rgb):
    return unreal.LinearColor(float(rgb[0]), float(rgb[1]), float(rgb[2]), 1.0)


def set_props(struct, values, what):
    for key, value in values.items():
        try:
            struct.set_editor_property(key, value)
        except Exception as error:  # noqa: BLE001 - report every property the build doesn't know
            warn("%s: could not set %s (%s)" % (what, key, error))


# ---------------------------------------------------------------- 1. the ground

def channel(name):
    channels = unreal.BeyondGroundChannel
    return {None: channels.NONE, "R": channels.R, "G": channels.G, "B": channels.B, "A": channels.A}[name]


def frost_variant(name):
    variants = unreal.BeyondGroundFrostVariant
    return {None: variants.NONE, "TrampledSnow": variants.TRAMPLED_SNOW, "SnowInGaps": variants.SNOW_IN_GAPS,
            "SnowOnTop": variants.SNOW_ON_TOP}[name]


def glow(name):
    glows = unreal.BeyondGroundGlow
    return {None: glows.NONE, "Embers": glows.EMBERS, "Veins": glows.VEINS}[name]


def build_look(layer, spec):
    look = unreal.BeyondGroundLayerLook()
    roughness_map = texture(spec.get("roughness"), "%s roughness" % layer)
    height_map = texture(spec.get("height"), "%s height" % layer)
    values = {
        "layer": unreal.Name(layer),
        "base_color": texture(spec.get("base"), "%s base colour" % layer),
        "normal": texture(spec.get("normal"), "%s normal" % layer),
        "roughness_map": roughness_map,
        "roughness_channel": channel(spec.get("roughness_channel") if roughness_map else None),
        "height_map": height_map,
        "height_channel": channel("R" if height_map else None),
        "tile_size": float(spec.get("tile", 3.0)),
        "tint": colour(spec.get("tint", (1.0, 1.0, 1.0))),
        "saturation": float(spec.get("saturation", 1.0)),
        "brightness": float(spec.get("brightness", 1.0)),
        "roughness": float(spec.get("roughness_value", 0.85)),
        "roughness_scale": float(spec.get("roughness_scale", 1.0)),
        "normal_strength": float(spec.get("normal_strength", 1.0)),
        "height_contrast": float(spec.get("height_contrast", 1.0)),
        "height_offset": float(spec.get("height_offset", 0.0)),
        "triplanar": bool(spec.get("triplanar", False)),
        "frost_variant": frost_variant(spec.get("frost")),
        "frost_amount": float(spec.get("frost_amount", 1.0)),
    }
    for region in ("frost", "molten", "blight"):
        turn = spec.get("%s_layer" % region)
        if turn:
            values["%s_layer" % region] = unreal.Name(turn[0])
            values["%s_layer_amount" % region] = float(turn[1])
    patch = spec.get("patch")
    if patch:
        values.update({"patch_layer": unreal.Name(patch[0]), "patch_amount": float(patch[1]), "patch_tile_size": float(patch[2])})
    if spec.get("glow"):
        values.update({
            "glow": glow(spec["glow"]),
            "glow_mask": texture(spec.get("glow_mask"), "%s glow mask" % layer),
            "glow_tile_size": float(spec.get("glow_tile", 6.0)),
            "glow_colour": colour(spec.get("glow_colour", (1.0, 0.35, 0.08))),
            "glow_strength": float(spec.get("glow_strength", 1.0)),
            "glow_sharpness": float(spec.get("glow_sharpness", 6.0)),
        })
    set_props(look, values, "%s look" % layer)
    if values["base_color"] is None:
        warn("%s: no base colour found, it keeps its greybox colour" % layer)
    return look


def build_settings(mask):
    settings = unreal.BeyondGroundSettings()
    s = content.SETTINGS
    trampled = s["trampled"]
    quads = (LAYOUT.get_editor_property("components_per_side") * LAYOUT.get_editor_property("sections_per_component")
             * LAYOUT.get_editor_property("quads_per_section"))
    values = {
        "region_mask": mask,
        "region_mask_size": float(quads),
        "trampled_snow_base_color": texture(trampled["base"], "trampled snow base colour"),
        "trampled_snow_normal": texture(trampled["normal"], "trampled snow normal"),
        "trampled_snow_roughness": texture(trampled["roughness"], "trampled snow roughness"),
        "trampled_snow_roughness_channel": channel(trampled["roughness_channel"]),
        "trampled_snow_tile_size": float(trampled["tile"]),
        "snow_layer": unreal.Name(s["snow_layer"]),
        "rock_snow_start": float(s["rock_snow_start"]),
        "rock_snow_end": float(s["rock_snow_end"]),
        "gap_snow_level": float(s["gap_snow_level"]),
        "macro_noise": texture(s["macro_noise"], "macro noise"),
        "macro_tile_size": float(s["macro_tile"]),
        "macro_tile_size2": float(s["macro_tile2"]),
        "macro_strength": float(s["macro_strength"]),
        "variation_tint_a": colour(s["variation_a"]),
        "variation_tint_b": colour(s["variation_b"]),
        "far_tile_scale": float(s["far_tile_scale"]),
        "far_mix": float(s["far_mix"]),
        "glow_far_fade": float(s["glow_far_fade"]),
        "far_blend_start": float(s["far_blend_start"]),
        "far_blend_end": float(s["far_blend_end"]),
        "far_normal_fade": float(s["far_normal_fade"]),
    }
    for region, grade_values in content.GRADES.items():
        grade = unreal.BeyondGroundRegionGrade()
        set_props(grade, {"tint": colour(grade_values["tint"]), "saturation": float(grade_values["saturation"]),
                          "brightness": float(grade_values["brightness"])}, "%s grade" % region)
        values[region.lower()] = grade
    set_props(settings, values, "ground settings")
    return settings


def ensure_instanced_usage_of(interface):
    """Grass draws instanced: a material's base must allow it (saved when changed)."""
    base = interface.get_base_material() if interface is not None else None
    if base is None or base.get_editor_property("used_with_instanced_static_meshes"):
        return
    base.set_editor_property("used_with_instanced_static_meshes", True)
    MEL.recompile_material(base)
    save(base, base.get_path_name().split(".")[0])
    log("instanced usage switched on: %s" % base.get_path_name().split(".")[0])


def ensure_instanced_usage(mesh):
    for slot in mesh.get_editor_property("static_materials"):
        ensure_instanced_usage_of(slot.get_editor_property("material_interface"))


OVERRIDES = {}


def override_material(name):
    """An instance from ground_content.MATERIAL_OVERRIDES (in the Grass folder; remade with BEYOND_REBUILD_GROUND=1)."""
    if name in OVERRIDES:
        return OVERRIDES[name]
    spec = content.MATERIAL_OVERRIDES.get(name)
    path = "%s/%s" % (GRASS_DIR, name)
    instance = None
    if spec is None:
        warn("no material override %s in ground_content.py" % name)
    elif EAL.does_asset_exist(path) and not REBUILD_GROUND:
        instance = load(path)
    else:
        parent = first_asset([spec["parent"]], "%s parent" % name, unreal.MaterialInterface)
        if parent is not None:
            instance = load(path) if EAL.does_asset_exist(path) else ASSET_TOOLS.create_asset(
                name, GRASS_DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
            MEL.set_material_instance_parent(instance, parent)
            for parameter, value in spec.get("vectors", {}).items():
                MEL.set_material_instance_vector_parameter_value(instance, parameter, colour(value))
            MEL.update_material_instance(instance)
            save(instance, path)
            log("made %s (%s)" % (path, ", ".join("%s %s" % kv for kv in spec.get("vectors", {}).items())))
    OVERRIDES[name] = instance
    return instance


def grass_varieties(name, spec):
    varieties = []
    for entry in spec["varieties"]:
        candidates, density, scale, cull, shadow = entry[:5]
        override = override_material(entry[5]) if len(entry) > 5 else None
        for path in candidates:
            if not EAL.does_asset_exist(path):
                warn("grass %s: %s not found" % (name, path))
                continue
            mesh = EAL.load_asset(path)
            if not isinstance(mesh, unreal.StaticMesh):
                warn("grass %s: %s isn't a static mesh (grass can't use it; it's for the trees-and-rocks pass)" % (name, path))
                continue
            bounds = mesh.get_bounds()
            height = bounds.box_extent.z * 2.0 * scale[1] / 100.0
            if height > content.MAX_COVER_HEIGHT:
                warn("grass %s: %s is %.1f m tall at its largest, kept for the trees-and-rocks pass" % (name, path, height))
                continue
            ensure_instanced_usage(mesh)
            if override is not None:
                ensure_instanced_usage_of(override)
            variety = unreal.BeyondGrassVarietySpec()
            set_props(variety, {
                "mesh": mesh,
                "density": float(density),
                "scale_min": float(scale[0]),
                "scale_max": float(scale[1]),
                "start_cull_distance": int(cull[0] * 100),
                "end_cull_distance": int(cull[1] * 100),
                "align_to_surface": True,
                "cast_shadow": bool(shadow),
                "override_materials": [override] * len(mesh.get_editor_property("static_materials")) if override is not None else [],
            }, "grass %s" % name)
            varieties.append(variety)
    return varieties


def step_grass():
    inputs = []
    for name, spec in content.GRASS.items():
        path = "%s/LGT_%s" % (GRASS_DIR, name)
        if EAL.does_asset_exist(path) and not REBUILD_GROUND:
            grass = load(path)
        else:
            varieties = grass_varieties(name, spec)
            if not varieties:
                warn("grass %s: no meshes found, not grown" % name)
                continue
            grass = GLIB.create_grass_type(path, varieties)
            save_if_dirty(grass)
            log("grass type %s: %d meshes on %s" % (path, len(varieties), spec["layer"]))
        if grass is None:
            continue
        entry = unreal.BeyondGroundGrassInput()
        regions = spec.get("regions", (1.0, 1.0, 1.0, 1.0))
        set_props(entry, {"name": unreal.Name(name), "grass_type": grass, "layer": unreal.Name(spec["layer"]),
                          "regions": unreal.LinearColor(*[float(v) for v in regions])}, "grass input %s" % name)
        inputs.append(entry)
    return inputs


def cap_textures():
    if FULL_RES:
        log("ground textures left at full size (BEYOND_GROUND_FULL_RES=1)")
        return
    capped = 0
    for path, tex in sorted(USED_TEXTURES.items()):
        # The source size from the asset registry (a headless run only sees 32 px placeholders)
        dimensions = str(EAL.find_asset_data(path).get_tag_value("Dimensions") or "")
        try:
            size = max(int(v) for v in dimensions.lower().replace(" ", "").split("x")[:2])
            limit = tex.get_editor_property("max_texture_size")
        except (ValueError, Exception) as error:  # noqa: BLE001
            warn("%s: could not read its size (%r, %s)" % (path, dimensions, error))
            continue
        if size > MAX_TEXTURE and (limit == 0 or limit > MAX_TEXTURE):
            tex.set_editor_property("max_texture_size", MAX_TEXTURE)
            save(tex, path)
            capped += 1
    log("ground textures: %d used, %d capped at %d px" % (len(USED_TEXTURES), capped, MAX_TEXTURE))


def step_ground():
    # The region mask (only with the material: it is what the material was laid out for)
    mask = load(REGION_MASK) if EAL.does_asset_exist(REGION_MASK) and not REBUILD_GROUND else None
    if mask is None:
        mask = GLIB.bake_region_mask(LAYOUT, REGION_MASK, 512)
        save_if_dirty(mask)
        log("baked %s" % REGION_MASK)
    looks = [build_look(str(layer), content.LAYERS[str(layer)]) for layer in GLIB.get_layer_names() if str(layer) in content.LAYERS]
    missing = [str(layer) for layer in GLIB.get_layer_names() if str(layer) not in content.LAYERS]
    if missing:
        warn("no look for %s in ground_content.py (greybox colour)" % ", ".join(missing))
    settings = build_settings(mask)
    grass = step_grass()
    cap_textures()

    existed = EAL.does_asset_exist(GROUND_MATERIAL)
    material = GLIB.create_ground_material(GROUND_MATERIAL, looks, settings, grass, REBUILD_GROUND)
    if material is None:
        warn("could not build %s" % GROUND_MATERIAL)
        return None
    save_if_dirty(material)
    if existed and not REBUILD_GROUND:
        log("%s exists (BEYOND_REBUILD_GROUND=1 builds it again from ground_content.py)" % GROUND_MATERIAL)
    else:
        log("built %s: %d layer looks, %d grass types" % (GROUND_MATERIAL, len(looks), len(grass)))
    instance = GLIB.create_ground_material_instance(GROUND_INSTANCE, material, RESET_INSTANCE)
    save_if_dirty(instance)
    return instance


# ---------------------------------------------------------------- 2. water, ice, lava materials

def sampler_type(tex):
    """The sampler a texture parameter needs for this texture (a mismatch is a compile error)."""
    types = unreal.MaterialSamplerType
    try:
        compression = tex.get_editor_property("compression_settings").name
        srgb = tex.get_editor_property("srgb")
        virtual = tex.get_editor_property("virtual_texture_streaming")
    except Exception:  # noqa: BLE001
        return types.SAMPLERTYPE_COLOR
    if compression == "TC_NORMALMAP":
        return types.SAMPLERTYPE_VIRTUAL_NORMAL if virtual else types.SAMPLERTYPE_NORMAL
    if compression in ("TC_GRAYSCALE", "TC_DISPLACEMENTMAP"):
        if virtual:
            return types.SAMPLERTYPE_VIRTUAL_GRAYSCALE if srgb else types.SAMPLERTYPE_VIRTUAL_LINEAR_GRAYSCALE
        return types.SAMPLERTYPE_GRAYSCALE if srgb else types.SAMPLERTYPE_LINEAR_GRAYSCALE
    if compression == "TC_MASKS":
        return types.SAMPLERTYPE_VIRTUAL_MASKS if virtual else types.SAMPLERTYPE_MASKS
    if compression == "TC_ALPHA":
        return types.SAMPLERTYPE_VIRTUAL_ALPHA if virtual else types.SAMPLERTYPE_ALPHA
    if virtual:
        return types.SAMPLERTYPE_VIRTUAL_COLOR if srgb else types.SAMPLERTYPE_VIRTUAL_LINEAR_COLOR
    return types.SAMPLERTYPE_COLOR if srgb else types.SAMPLERTYPE_LINEAR_COLOR


class Graph:
    """A material built node by node from the script (water, ice, lava)."""

    def __init__(self, material):
        self.material = material
        self.count = 0

    def node(self, cls, **props):
        x = -1800 + (self.count % 6) * 300
        y = -600 + (self.count // 6) * 160
        self.count += 1
        expression = MEL.create_material_expression(self.material, cls, x, y)
        for key, value in props.items():
            expression.set_editor_property(key, value)
        return expression

    def link(self, source, target, target_input, source_output=""):
        if not MEL.connect_material_expressions(source, source_output, target, target_input):
            warn("%s: could not connect %s -> %s.%s" % (self.material.get_name(), source.get_name(), target.get_name(), target_input))

    def output(self, source, prop, source_output=""):
        if not MEL.connect_material_property(source, source_output, prop):
            warn("%s: could not connect %s" % (self.material.get_name(), prop))

    def scalar(self, name, value, group=""):
        return self.node(unreal.MaterialExpressionScalarParameter, parameter_name=name, default_value=float(value), group=group)

    def vector(self, name, value, group=""):
        return self.node(unreal.MaterialExpressionVectorParameter, parameter_name=name, default_value=colour(value), group=group)

    def texture(self, name, tex, uv, group=""):
        sample = self.node(unreal.MaterialExpressionTextureSampleParameter2D, parameter_name=name, group=group)
        if tex is not None:
            sample.set_editor_property("texture", tex)
            sample.set_editor_property("sampler_type", sampler_type(tex))
        self.link(uv, sample, "UVs")
        return sample

    def custom(self, description, code, inputs, output_type):
        """inputs: (name, expression, output name)"""
        node = self.node(unreal.MaterialExpressionCustom, code=code, description=description, output_type=output_type)
        slots = []
        for input_name, _source, _output in inputs:
            slot = unreal.CustomInput()
            slot.set_editor_property("input_name", input_name)
            slots.append(slot)
        node.set_editor_property("inputs", slots)
        for input_name, source, source_output in inputs:
            self.link(source, node, input_name, source_output)
        return node


def new_material(path):
    folder, name = path.rsplit("/", 1)
    if EAL.does_asset_exist(path):
        material = load(path)
        if material is not None:
            # Every node, outputs too (MaterialEditingLibrary's delete-all leaves every other node behind)
            GLIB.clear_material_graph(material)
        return material, "rebuilt"
    return ASSET_TOOLS.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew()), "created"


FLOAT1 = unreal.CustomMaterialOutputType.CMOT_FLOAT1
FLOAT2 = unreal.CustomMaterialOutputType.CMOT_FLOAT2
FLOAT3 = unreal.CustomMaterialOutputType.CMOT_FLOAT3
PROP = unreal.MaterialProperty


def build_water_material():
    material, action = new_material(WATER_MATERIAL)
    if material is None:
        warn("could not create %s" % WATER_MATERIAL)
        return None
    slw = hasattr(unreal, "MaterialExpressionSingleLayerWaterMaterialOutput")
    if slw:
        material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_SINGLE_LAYER_WATER)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    g = Graph(material)
    position = g.node(unreal.MaterialExpressionWorldPosition)
    time = g.node(unreal.MaterialExpressionTime)
    tile = g.scalar("Wave Tile Size", 13.0, "Waves")
    speed = g.scalar("Wave Speed", 0.03, "Waves")
    strength = g.scalar("Wave Strength", 0.45, "Waves")
    uv_a = g.custom("Wave UV A", "return P.xy / (Tile * 100.0) + float2(0.6, 0.8) * T * Speed;",
                    [("P", position, ""), ("T", time, ""), ("Tile", tile, ""), ("Speed", speed, "")], FLOAT2)
    # The second set turned and larger, so the two never line up into a grid
    uv_b = g.custom("Wave UV B", "float2 R = float2(P.x * 0.8 - P.y * 0.6, P.x * 0.6 + P.y * 0.8);\n"
                    "return R / (Tile * 100.0) * 0.37 + float2(-0.7, 0.3) * T * Speed * 0.6;",
                    [("P", position, ""), ("T", time, ""), ("Tile", tile, ""), ("Speed", speed, "")], FLOAT2)
    normal_tex = first_asset([content.WATER_NORMAL], "water normal", unreal.Texture)
    wave_a = g.texture("Wave Normal", normal_tex, uv_a, "Waves")
    wave_b = g.texture("Wave Normal", normal_tex, uv_b, "Waves")
    normal = g.custom("Waves", "float3 N = float3(A.xy + B.xy, A.z * B.z);\nN.xy *= Strength;\nreturn normalize(N);",
                      [("A", wave_a, "RGB"), ("B", wave_b, "RGB"), ("Strength", strength, "")], FLOAT3)
    g.output(normal, PROP.MP_NORMAL)
    g.output(g.scalar("Roughness", 0.03, "Surface"), PROP.MP_ROUGHNESS)
    g.output(g.vector("Surface Colour", (0.02, 0.03, 0.03), "Surface"), PROP.MP_BASE_COLOR)
    if slw:
        # Opacity 0: all water (1 would be an opaque surface layer such as foam)
        g.output(g.scalar("Surface Opacity", 0.0, "Surface"), PROP.MP_OPACITY)
        out = g.node(unreal.MaterialExpressionSingleLayerWaterMaterialOutput)
        g.link(g.vector("Scattering", (0.01, 0.04, 0.035), "Water"), out, "ScatteringCoefficients")
        g.link(g.vector("Absorption", (0.35, 0.07, 0.06), "Water"), out, "AbsorptionCoefficients")
        g.link(g.scalar("Phase G", 0.1, "Water"), out, "PhaseG")
    else:
        warn("Single Layer Water isn't scriptable here: %s is a plain glossy surface" % WATER_MATERIAL)
    MEL.recompile_material(material)
    save(material, WATER_MATERIAL)
    log("%s %s" % (action, WATER_MATERIAL))
    return material


def build_water_instance(name, look, parent):
    path = "%s/%s" % (WATER_DIR, name)
    if EAL.does_asset_exist(path) and not REBUILD_WATER:
        return load(path)
    instance = load(path) if EAL.does_asset_exist(path) else ASSET_TOOLS.create_asset(
        name, WATER_DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    if instance is None:
        warn("could not create %s" % path)
        return None
    MEL.set_material_instance_parent(instance, parent)
    MEL.set_material_instance_vector_parameter_value(instance, "Scattering", colour(look["scattering"]))
    MEL.set_material_instance_vector_parameter_value(instance, "Absorption", colour(look["absorption"]))
    MEL.set_material_instance_scalar_parameter_value(instance, "Roughness", float(look["roughness"]))
    MEL.update_material_instance(instance)
    save(instance, path)
    log("made %s" % path)
    return instance


def build_ice_material():
    material, action = new_material(ICE_MATERIAL)
    if material is None:
        warn("could not create %s" % ICE_MATERIAL)
        return None
    g = Graph(material)
    position = g.node(unreal.MaterialExpressionWorldPosition)

    def uv(name, tile):
        return g.custom(name, "return P.xy / (Tile * 100.0);", [("P", position, ""), ("Tile", g.scalar(name, tile, "Tiling"), "")], FLOAT2)

    crack = g.texture("Crack Normal", first_asset([content.CRACKLE_NORMAL], "ice cracks", unreal.Texture), uv("Crack Tile Size", 3.0), "Ice")
    swell = g.texture("Swell Normal", first_asset([content.WATER_NORMAL], "ice swell", unreal.Texture), uv("Swell Tile Size", 11.0), "Ice")
    snow = g.texture("Snow Base Colour", first_asset([content.FAB_SNOW + "B"], "ice snow dusting", unreal.Texture), uv("Snow Tile Size", 4.0), "Snow")
    noise = g.texture("Dust Noise", first_asset([content.NOISE_GROUND, content.NOISE_SOFT], "ice dust noise", unreal.Texture),
                      uv("Dust Tile Size", 25.0), "Snow")
    level = g.scalar("Dust Level", content.ICE_LOOK["dust_level"], "Snow")
    ice_colour = g.vector("Ice Colour", content.ICE_LOOK["colour"], "Ice")
    base = g.custom("Ice colour", "float Dust = saturate((N - Level) * 4.0);\nreturn lerp(Ice, Snow, Dust);",
                    [("N", noise, "R"), ("Level", level, ""), ("Ice", ice_colour, ""), ("Snow", snow, "RGB")], FLOAT3)
    normal = g.custom("Ice normal",
                      "float Dust = saturate((N - Level) * 4.0);\n"
                      "float3 C = float3(Crack.xy * CrackStrength, 1.0);\n"
                      "float3 M = normalize(float3(C.xy + Swell.xy * 0.3, C.z));\n"
                      "return normalize(lerp(M, float3(0.0, 0.0, 1.0), Dust * 0.7));",
                      [("N", noise, "R"), ("Level", level, ""), ("Crack", crack, "RGB"), ("Swell", swell, "RGB"),
                       ("CrackStrength", g.scalar("Crack Strength", 0.8, "Ice"), "")], FLOAT3)
    rough = g.custom("Ice roughness", "float Dust = saturate((N - Level) * 4.0);\nreturn lerp(Ice, 0.7, Dust);",
                     [("N", noise, "R"), ("Level", level, ""), ("Ice", g.scalar("Ice Roughness", content.ICE_LOOK["roughness"], "Ice"), "")], FLOAT1)
    g.output(base, PROP.MP_BASE_COLOR)
    g.output(normal, PROP.MP_NORMAL)
    g.output(rough, PROP.MP_ROUGHNESS)
    g.output(g.scalar("Specular", 0.6, "Ice"), PROP.MP_SPECULAR)
    MEL.recompile_material(material)
    save(material, ICE_MATERIAL)
    log("%s %s" % (action, ICE_MATERIAL))
    return material


def build_lava_material():
    material, action = new_material(LAVA_MATERIAL)
    if material is None:
        warn("could not create %s" % LAVA_MATERIAL)
        return None
    g = Graph(material)
    position = g.node(unreal.MaterialExpressionWorldPosition)
    time = g.node(unreal.MaterialExpressionTime)
    tile = g.scalar("Flow Tile Size", 12.0, "Flow")
    speed = g.scalar("Flow Speed", 0.012, "Flow")
    uv_a = g.custom("Flow UV A", "return P.xy / (Tile * 100.0) + float2(0.3, 0.1) * T * Speed;",
                    [("P", position, ""), ("T", time, ""), ("Tile", tile, ""), ("Speed", speed, "")], FLOAT2)
    uv_b = g.custom("Flow UV B", "return P.xy / (Tile * 100.0) * 0.61 + float2(-0.2, 0.25) * T * Speed * 0.7;",
                    [("P", position, ""), ("T", time, ""), ("Tile", tile, ""), ("Speed", speed, "")], FLOAT2)
    lava_tex = first_asset([content.LAVA_FLOOR], "lava", unreal.Texture)
    flow_a = g.texture("Lava", lava_tex, uv_a, "Flow")
    flow_b = g.texture("Lava", lava_tex, uv_b, "Flow")
    crust_uv = g.custom("Crust UV", "return P.xy / (Tile * 100.0);",
                        [("P", position, ""), ("Tile", g.scalar("Crust Tile Size", 7.0, "Crust"), "")], FLOAT2)
    crust = g.texture("Crust Noise", first_asset([content.NOISE_GROUND, content.NOISE_SOFT], "lava crust", unreal.Texture), crust_uv, "Crust")
    heat = g.custom("Glow",
                    "float H = saturate(dot(A, 0.333) * dot(B, 0.333) * Boost);\n"
                    "float C = saturate((N - Level) * 6.0);\n"
                    "return H * (1.0 - C);",
                    [("A", flow_a, "RGB"), ("B", flow_b, "RGB"), ("N", crust, "R"),
                     ("Boost", g.scalar("Heat Boost", content.LAVA_LOOK["boost"], "Flow"), ""),
                     ("Level", g.scalar("Crust Level", content.LAVA_LOOK["crust_level"], "Crust"), "")], FLOAT1)
    hot = g.vector("Hot Colour", content.LAVA_LOOK["hot"], "Colour")
    emissive = g.custom("Lava light", "return (Hot * G + float3(1.0, 0.6, 0.25) * pow(G, 4.0)) * Strength;",
                        [("G", heat, ""), ("Hot", hot, ""), ("Strength", g.scalar("Glow Strength", content.LAVA_LOOK["strength"], "Colour"), "")], FLOAT3)
    base = g.custom("Lava colour", "return lerp(Crust, Hot * 0.3, G);",
                    [("G", heat, ""), ("Hot", hot, ""), ("Crust", g.vector("Crust Colour", (0.035, 0.03, 0.03), "Colour"), "")], FLOAT3)
    rough = g.custom("Lava roughness", "return lerp(0.85, 0.35, G);", [("G", heat, "")], FLOAT1)
    g.output(base, PROP.MP_BASE_COLOR)
    g.output(emissive, PROP.MP_EMISSIVE_COLOR)
    g.output(rough, PROP.MP_ROUGHNESS)
    MEL.recompile_material(material)
    save(material, LAVA_MATERIAL)
    log("%s %s" % (action, LAVA_MATERIAL))
    return material


def step_water_materials():
    def get(path, builder):
        if EAL.does_asset_exist(path) and not REBUILD_WATER:
            return load(path)
        return builder()

    water = get(WATER_MATERIAL, build_water_material)
    materials = {"ice": get(ICE_MATERIAL, build_ice_material), "lava": get(LAVA_MATERIAL, build_lava_material)}
    for name, look in content.WATER_LOOKS.items():
        materials[name] = build_water_instance(name, look, water) if water is not None else None
    return materials


# ---------------------------------------------------------------- 2b. surfaces and hazards in Dominion

def existing_actors():
    return {a.get_actor_label(): a for a in ACTORS.get_all_level_actors()}


def clear_label(existing, label):
    """True when the actor should be (re)placed: missing, or rebuilt."""
    actor = existing.get(label)
    if actor is None:
        return True
    if not REBUILD_WATER:
        return False
    WLIB.destroy_actors([actor])
    return True


def place_disc(existing, label, centre, top_z, radius_cm, thickness_cm, material, walkable):
    if not clear_label(existing, label):
        return None
    mesh = unreal.load_asset(CYLINDER)
    location = unreal.Vector(centre[0], centre[1], top_z - thickness_cm * 0.5)
    actor = ACTORS.spawn_actor_from_class(unreal.StaticMeshActor, location, unreal.Rotator(0.0, 0.0, 0.0))
    if actor is None:
        warn("could not place %s" % label)
        return None
    component = actor.static_mesh_component
    component.set_static_mesh(mesh)
    # The engine cylinder: radius 50, 100 tall, pivot in the middle
    actor.set_actor_scale3d(unreal.Vector(radius_cm / 50.0, radius_cm / 50.0, thickness_cm / 100.0))
    if material is not None:
        component.set_material(0, material)
    component.set_editor_property("cast_shadow", False)
    if walkable:
        component.set_collision_profile_name("BlockAll")
    else:
        component.set_collision_profile_name("NoCollision")
        component.set_editor_property("can_ever_affect_navigation", False)
    actor.set_actor_label(label)
    actor.set_folder_path("Ground")
    # Always loaded (a handful of actors): the ice must be there when the party lands on it
    WLIB.set_spatially_loaded(actor, False)
    return actor


def place_hazard(existing, label, kind, centre, top_z, bottom_z, radius_cm, burn=None):
    if not clear_label(existing, label):
        return None
    half = max((top_z - bottom_z) * 0.5, 50.0)
    actor = ACTORS.spawn_actor_from_class(unreal.BeyondHazardVolume, unreal.Vector(centre[0], centre[1], top_z - half),
                                          unreal.Rotator(0.0, 0.0, 0.0))
    if actor is None:
        warn("could not place %s" % label)
        return None
    actor.set_editor_property("kind", kind)
    actor.set_editor_property("radius", float(radius_cm))
    if burn is not None:
        actor.set_editor_property("burn_damage_per_second", float(burn))
    actor.get_editor_property("box").set_box_extent(unreal.Vector(radius_cm, radius_cm, half), False)
    actor.set_actor_label(label)
    actor.set_folder_path("Ground")
    WLIB.set_spatially_loaded(actor, False)
    return actor


def step_surfaces(materials):
    existing = existing_actors()
    placed = 0
    for lake_id, centre, radius, depth, _frozen in world_content.LAYOUT["lakes"]:
        kind = content.WATER.get(lake_id)
        level = lake_level(lake_id)
        if kind is None or level is None:
            warn("%s: no water (no entry in ground_content.WATER or no level in the layout)" % lake_id)
            continue
        x, y = m2ue(*centre)
        disc = (radius + content.WATER_MARGIN) * 100.0
        if kind == "ice":
            placed += place_disc(existing, "WB_Ice_%s" % lake_id, (x, y), level, disc, 40.0, materials.get("ice"), True) is not None
            continue
        placed += place_disc(existing, "WB_Water_%s" % lake_id, (x, y), level, disc, 5.0, materials.get(kind), False) is not None
        top = level - content.DEEP_BELOW_SURFACE * 100.0
        placed += place_hazard(existing, "WB_Hazard_%s" % lake_id, unreal.BeyondHazardKind.DEEP_WATER, (x, y), top,
                               level - (depth + 6.0) * 100.0, radius * 100.0) is not None
        log("%s: water at %.1f m, deep water from %.1f m down within %.0f m" % (lake_id, level / 100.0, content.DEEP_BELOW_SURFACE, radius))

    # The caldera
    lava = content.LAVA
    volcano = world_content.LAYOUT["volcano"]
    arena = next((a for a in world_content.ARENAS if a[0] == lava["arena"]), None)
    if arena is None:
        warn("no arena %s for the lava's level" % lava["arena"])
        return placed
    ax, ay = m2ue(*arena[2])
    level = ground(ax, ay) - lava["below_arena"] * 100.0
    vx, vy = m2ue(*volcano["centre"])
    disc = volcano["rim_radius"] * lava["radius_share"] * 100.0
    # How much of the floor the lava shows on (the floor's low spots)
    samples = covered = 0
    step = 1000.0
    steps = int(disc // step)
    for i in range(-steps, steps + 1):
        for j in range(-steps, steps + 1):
            if (i * step) ** 2 + (j * step) ** 2 <= disc * disc:
                samples += 1
                covered += ground(vx + i * step, vy + j * step) < level
    placed += place_disc(existing, "WB_Lava_%s" % lava["arena"], (vx, vy), level, disc, 5.0, materials.get("lava"), False) is not None
    placed += place_hazard(existing, "WB_Hazard_%s" % lava["arena"], unreal.BeyondHazardKind.BURN, (vx, vy),
                           level + lava["burn_above"] * 100.0, level - 800.0, disc, lava["burn_per_second"]) is not None
    log("caldera: lava at %.1f m (%.1f m under Kael'thar's floor), %.0f m across, showing on %d%% of the floor" % (
        level / 100.0, lava["below_arena"], disc * 2.0 / 100.0, 100 * covered // max(samples, 1)))
    if not placed:
        log("water, ice and lava: all there (BEYOND_REBUILD_WATER=1 places them again)")
    return placed


# ---------------------------------------------------------------- 3. pictures

def look_at(eye, target):
    dx, dy, dz = target.x - eye.x, target.y - eye.y, target.z - eye.z
    yaw = math.degrees(math.atan2(dy, dx))
    pitch = math.degrees(math.atan2(dz, math.sqrt(dx * dx + dy * dy)))
    return unreal.Rotator(roll=0.0, pitch=pitch, yaw=yaw)


def view_target(spec, direction):
    """A view's target (Unreal cm, on the ground)."""
    if isinstance(spec, str):
        x, y = world_content.REGIONS[spec]["centre"]
    elif spec[0] == "lake":
        lake = next(l for l in world_content.LAYOUT["lakes"] if l[0] == spec[1])
        length = math.hypot(*direction) or 1.0
        # On the water 15 m off the near shore (the eye stands back on the bank)
        x = lake[1][0] + direction[0] / length * (lake[2] - 15.0)
        y = lake[1][1] + direction[1] / length * (lake[2] - 15.0)
    elif spec[0] == "volcano":
        x, y = world_content.LAYOUT["volcano"]["centre"]
    else:
        x, y = spec
    ux, uy = m2ue(x, y)
    return unreal.Vector(ux, uy, ground(ux, uy))


def shots():
    """(file name, camera location, rotation)"""
    out = []
    for name, spec, direction in content.VIEWS:
        target = view_target(spec, direction)
        length = math.hypot(*direction) or 1.0
        # Unreal: x east, y south
        dx, dy = direction[0] / length * 100.0, -direction[1] / length * 100.0
        back = 40.0 if isinstance(spec, tuple) and spec[0] == "lake" else 14.0
        eye_xy = (target.x + dx * back, target.y + dy * back)
        eye = unreal.Vector(eye_xy[0], eye_xy[1], ground(*eye_xy) + 170.0)
        out.append(("%s_eye" % name, eye, look_at(eye, unreal.Vector(target.x, target.y, target.z + 60.0))))
        high = unreal.Vector(target.x + dx * 55.0, target.y + dy * 55.0, target.z + 3500.0)
        out.append(("%s_aerial" % name, high, look_at(high, target)))
    # Two from high up: tiling and variation at a distance
    for name, (x, y), height in (("overview_elderwood", (-400, -900), 350.0), ("overview_north", (0, 1000), 450.0)):
        ux, uy = m2ue(x, y)
        target = unreal.Vector(ux, uy, ground(ux, uy))
        eye = unreal.Vector(ux - 30000.0, uy + 30000.0, target.z + height * 100.0)
        out.append((name, eye, look_at(eye, target)))
    return out


def step_pictures():
    if SKIP_PICTURES or not rendering():
        log("pictures skipped (%s)" % ("BEYOND_SKIP_PICTURES=1" if SKIP_PICTURES else "no rendering"))
        return
    # The save let the proxies go: load them again (nothing is saved after this)
    WLIB.load_landscape_proxies(WORLD)
    WLIB.load_actors_with_label_prefix(WORLD, "WB_")
    unreal.SystemLibrary.execute_console_command(WORLD, "r.Streaming.FullyLoadUsedTextures 1")
    planned = shots()
    # The instances fill in while the pictures render (the count is what was ready at once)
    instances = GLIB.grow_grass_at(WORLD, [location for _name, location, _rotation in planned])
    log("grass grown around %d cameras (%d instances ready at once)" % (len(planned), instances))
    os.makedirs(SAVED, exist_ok=True)
    taken = 0
    for name, location, rotation in planned:
        taken += VLIB.capture_view(WORLD, location, rotation, os.path.join(SAVED, "%s.png" % name), 70.0, 1280, 720, 0.5, 0.0, 24)
    VLIB.release_capture()
    log("pictures: %d of %d in %s" % (taken, len(planned), SAVED))


# ---------------------------------------------------------------- main

def main():
    global GLIB, WLIB, VLIB, ACTORS, LAYOUT, WORLD
    log("pass 18")
    missing = [name for name in ("BeyondGroundLibrary", "BeyondWorldBuilderLibrary", "BeyondVillageBuilderLibrary", "BeyondGroundLayerLook")
               if not hasattr(unreal, name)]
    if not missing:
        try:
            unreal.get_default_object(unreal.BeyondHazardVolume).get_editor_property("radius")
        except Exception:  # noqa: BLE001 - the hazard's Radius came with this pass
            missing.append("BeyondHazardVolume.Radius")
    if missing:
        warn("the editor is running an old build (%s missing). Close the editor, build WorldBeyondEditor (Development Editor, "
             "Win64) until it says Build succeeded, then run this script again. Nothing was changed." % ", ".join(missing))
        write_report("last_run_pass18.txt")
        raise RuntimeError("C++ not rebuilt: %s missing" % ", ".join(missing))
    GLIB = unreal.BeyondGroundLibrary
    WLIB = unreal.BeyondWorldBuilderLibrary
    VLIB = unreal.BeyondVillageBuilderLibrary
    ACTORS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    LAYOUT = load(LAYOUT_ASSET)
    if LAYOUT is None or not EAL.does_asset_exist(MAP):
        warn("Dominion or its layout missing: run migrate_pass15.py and migrate_pass16.py first")
        write_report("last_run_pass18.txt")
        return

    instance = step_ground()
    materials = step_water_materials()

    if not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(MAP):
        warn("could not open %s" % MAP)
        write_report("last_run_pass18.txt")
        return
    WORLD = editor_world()
    WLIB.load_landscape_proxies(WORLD)
    WLIB.load_actors_with_label_prefix(WORLD, "WB_")

    if instance is not None:
        current = GLIB.get_landscape_material(WORLD)
        if current is not None and current.get_path_name() == instance.get_path_name():
            log("the landscape already wears %s" % GROUND_INSTANCE)
        else:
            proxies = GLIB.set_landscape_material(WORLD, instance)
            log("the landscape wears %s now (%d streaming proxies)" % (GROUND_INSTANCE, proxies))
    step_surfaces(materials)

    if WLIB.save_world(WORLD):
        log("saved %s" % MAP)
    else:
        warn("some packages of %s could not be saved" % MAP)
    step_pictures()
    write_report("last_run_pass18.txt")


main()
