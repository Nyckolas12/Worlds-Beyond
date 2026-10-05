"""
Worlds Beyond - pass 5: fixes from the fifth playtest.

Run with the editor closed, after building the C++:
    UnrealEditor-Cmd.exe <path>/WorldBeyond.uproject -run=pythonscript -script=<path>/Scripts/Migration/migrate_pass5.py -unattended -nosplash -NullRHI

- Angel aims properly: with his staff out the camera moves over his right shoulder and a crosshair shows where his
  spells go; the enemy under it glows purple. Hold the right mouse button to aim (zoom, turn with the camera).
  Casting turns him to the crosshair. (IA_Aim on RMB, M_Beyond_AimHighlight, BP_Angel Aim Settings.)
- Angel's E (GA_Angel_LightningStrike) bursts blue / purple crystal spikes out of the ground instead of the yellow
  lightning; its aiming circle is purple. (M_Beyond_ArcaneCrystal, SM_Beyond_CrystalSpike, BP_Beyond_ArcaneSpikes.)

The materials and the spike mesh are only created when missing, so tweaks made in the editor survive a re-run.
Every asset saved is copied to Saved/MigrationBackups/<timestamp>/ first. Running it twice is safe.
"""
import os
import sys

import unreal

sys.path.insert(0, os.path.join(os.path.abspath(unreal.Paths.project_dir()), "Scripts", "Migration"))
from migration_common import (ASSET_TOOLS, EAL, bp_class, cdo, ensure_blueprint, load, log, map_keys, save,  # noqa: E402
                              set_props, warn, write_report)

MEL = unreal.MaterialEditingLibrary

ABILITIES = "/Game/WorldsBeyond/Abilities/"
SPIKES_DIR = "/Game/WorldsBeyond/VFX/ArcaneSpikes"
STAFF_ATTACKS = "/Game/EssentialAnimation/MagicStaff/Animation/UE5/Sequence/Attack/"
ANGEL = "/Game/WorldsBeyond/Characters/Angel/BP_Angel"

BLUE = unreal.LinearColor(0.10, 0.35, 1.0, 1.0)
PURPLE = unreal.LinearColor(0.60, 0.15, 1.0, 1.0)
RETICLE_PURPLE = unreal.LinearColor(0.55, 0.2, 1.0, 1.0)


# ---------------------------------------------------------------- material helpers

def _new_material(folder, name):
    """(material, created): the existing material, or a new empty one."""
    path = "%s/%s" % (folder, name)
    if EAL.does_asset_exist(path):
        return load(path), False
    material = ASSET_TOOLS.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())
    if material is None:
        warn("could not create %s" % path)
    return material, True


def _node(material, cls, x, y, **props):
    expression = MEL.create_material_expression(material, cls, x, y)
    for key, value in props.items():
        expression.set_editor_property(key, value)
    return expression


def _link(source, target, target_input, source_output=""):
    if not MEL.connect_material_expressions(source, source_output, target, target_input):
        warn("material: could not connect %s -> %s.%s" % (source.get_name(), target.get_name(), target_input))


def _output(material, source, prop):
    if not MEL.connect_material_property(source, "", prop):
        warn("material %s: could not connect %s" % (material.get_name(), prop))


def make_crystal_material():
    """Opaque crystal: dark tinted body, emissive Color x Glow, brighter at grazing angles."""
    material, created = _new_material(SPIKES_DIR, "M_Beyond_ArcaneCrystal")
    if material is None or not created:
        if material:
            log("kept existing %s" % material.get_path_name())
        return material

    color = _node(material, unreal.MaterialExpressionVectorParameter, -900, 0, parameter_name="Color", default_value=PURPLE)
    glow = _node(material, unreal.MaterialExpressionScalarParameter, -900, 200, parameter_name="Glow", default_value=6.0)
    fresnel = _node(material, unreal.MaterialExpressionFresnel, -900, 350, exponent=3.0)
    rim = _node(material, unreal.MaterialExpressionAdd, -650, 350, const_b=0.4)
    _link(fresnel, rim, "A")
    color_glow = _node(material, unreal.MaterialExpressionMultiply, -650, 100)
    _link(color, color_glow, "A")
    _link(glow, color_glow, "B")
    emissive = _node(material, unreal.MaterialExpressionMultiply, -400, 200)
    _link(color_glow, emissive, "A")
    _link(rim, emissive, "B")
    base = _node(material, unreal.MaterialExpressionMultiply, -400, -100, const_b=0.15)
    _link(color, base, "A")
    roughness = _node(material, unreal.MaterialExpressionConstant, -400, 450, r=0.2)

    _output(material, base, unreal.MaterialProperty.MP_BASE_COLOR)
    _output(material, emissive, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    _output(material, roughness, unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.recompile_material(material)
    save(material, material.get_path_name().split(".")[0])
    log("created %s (Color + Glow parameters)" % material.get_path_name())
    return material


def make_highlight_material():
    """Translucent unlit purple rim for the enemy under the crosshair (used as a mesh Overlay Material)."""
    material, created = _new_material(SPIKES_DIR.rsplit("/", 1)[0] + "/Aim", "M_Beyond_AimHighlight")
    if material is None or not created:
        if material:
            log("kept existing %s" % material.get_path_name())
        return material

    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("used_with_skeletal_mesh", True)

    color = _node(material, unreal.MaterialExpressionVectorParameter, -900, 0, parameter_name="Color",
                  default_value=unreal.LinearColor(0.75, 0.3, 1.0, 1.0))
    intensity = _node(material, unreal.MaterialExpressionScalarParameter, -900, 200, parameter_name="Intensity", default_value=4.0)
    fresnel = _node(material, unreal.MaterialExpressionFresnel, -900, 350, exponent=2.5)
    color_intensity = _node(material, unreal.MaterialExpressionMultiply, -650, 100)
    _link(color, color_intensity, "A")
    _link(intensity, color_intensity, "B")
    emissive = _node(material, unreal.MaterialExpressionMultiply, -400, 150)
    _link(color_intensity, emissive, "A")
    _link(fresnel, emissive, "B")
    rim_opacity = _node(material, unreal.MaterialExpressionMultiply, -650, 400, const_b=0.85)
    _link(fresnel, rim_opacity, "A")
    opacity = _node(material, unreal.MaterialExpressionAdd, -400, 400, const_b=0.08)
    _link(rim_opacity, opacity, "A")

    _output(material, emissive, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    _output(material, opacity, unreal.MaterialProperty.MP_OPACITY)
    MEL.recompile_material(material)
    save(material, material.get_path_name().split(".")[0])
    log("created %s" % material.get_path_name())
    return material


# ---------------------------------------------------------------- spike mesh

def _geometry_function(name):
    for library_name in dir(unreal):
        if library_name.startswith("GeometryScript"):
            library = getattr(unreal, library_name)
            if hasattr(library, name):
                return getattr(library, name)
    return None


def _pick(result, kind):
    if isinstance(result, tuple):
        return next((r for r in result if isinstance(r, kind)), None)
    return result if isinstance(result, kind) else None


def make_crystal_mesh(material):
    """A faceted five-sided spike, base at the origin, 100 cm tall (Geometry Script). None: use the engine cone."""
    path = SPIKES_DIR + "/SM_Beyond_CrystalSpike"
    if EAL.does_asset_exist(path):
        log("kept existing %s" % path)
        return load(path)

    append_cone = _geometry_function("append_cone")
    create_asset = _geometry_function("create_new_static_mesh_asset_from_mesh")
    if not append_cone or not create_asset:
        warn("Geometry Script not available (is the GeometryScripting plugin enabled?): the spikes use the engine cone")
        return None

    try:
        mesh = unreal.new_object(unreal.DynamicMesh)
        options = unreal.GeometryScriptPrimitiveOptions()
        try:
            options.set_editor_property("polygroup_mode", unreal.GeometryScriptPrimitivePolygroupMode.PER_FACE)
        except Exception:
            pass
        append_cone(mesh, options, unreal.Transform(), 30.0, 1.0, 100.0, 5, 1, True,
                    unreal.GeometryScriptPrimitiveOriginMode.BASE)
        per_face = _geometry_function("set_per_face_normals")
        if per_face:
            per_face(mesh)

        asset_options = unreal.GeometryScriptCreateNewStaticMeshAssetOptions()
        for key, value in (("enable_recompute_normals", False), ("enable_collision", False), ("enable_nanite", False)):
            try:
                asset_options.set_editor_property(key, value)
            except Exception:
                pass
        static_mesh = _pick(create_asset(mesh, path, asset_options), unreal.StaticMesh)
    except Exception as e:
        warn("could not build the crystal spike mesh (%s): the spikes use the engine cone" % e)
        return None

    if static_mesh is None:
        warn("Geometry Script made no static mesh at %s: the spikes use the engine cone" % path)
        return None
    if material:
        try:
            static_mesh.set_material(0, material)
        except Exception:
            pass
    save(static_mesh, path)
    log("created %s (5-sided faceted spike, 100 cm)" % path)
    return static_mesh


# ---------------------------------------------------------------- Angel's E

def step_arcane_spikes():
    """GA_Angel_LightningStrike: blue / purple crystal spikes instead of the yellow lightning cue, purple reticle."""
    material = make_crystal_material()
    mesh = make_crystal_mesh(material)

    spikes_path = SPIKES_DIR + "/BP_Beyond_ArcaneSpikes"
    spikes = ensure_blueprint(spikes_path, unreal.BeyondSpikeBurst)
    props = dict(color_a=BLUE, color_b=PURPLE)
    if material:
        props["spike_material"] = material
    if mesh:
        props["spike_mesh"] = mesh
    set_props(spikes, spikes_path, **props)
    log("configured %s (%s, %s)" % (spikes_path, mesh.get_name() if mesh else "engine cone",
                                    material.get_name() if material else "engine shape material"))

    path = ABILITIES + "Angel/GA_Angel_LightningStrike"
    strike = load(path)
    if strike is None:
        return
    set_props(strike, path,
              spike_burst_class=spikes.generated_class(),
              strike_cue_tag=unreal.GameplayTag(),      # no more yellow lightning bolt
              impact_delay=0.1,                          # damage as the first spikes break the surface
              override_target_decal_color=True,
              target_decal_color=RETICLE_PURPLE,
              target_decal_color_parameter="Color")
    log("%s: spike burst, lightning cue removed, purple reticle (damage / radius / cooldown unchanged)" % path)


# ---------------------------------------------------------------- Angel's aiming

def step_angel_aim():
    """IA_Aim on the right mouse button, the target highlight and BP_Angel's Aim Settings."""
    actions = map_keys("/Game/Input/IMC_Default", {"IA_Aim": "RightMouseButton"})
    action = actions["IA_Aim"]
    # IA_TargetCancel shares the right mouse button (cancels E's reticle); both must see it
    if action.get_editor_property("consume_input"):
        action.set_editor_property("consume_input", False)
        save(action, "/Game/Input/Actions/IA_Aim")
        log("IA_Aim lets the right mouse button through to IA_TargetCancel")

    highlight = make_highlight_material()

    angel = load(ANGEL)
    if angel is None:
        return
    casts = [load(STAFF_ATTACKS + "UE5_WZ_Attack_02_Seq_Montage"),   # LMB spell, E
             load(STAFF_ATTACKS + "UE5_BM_Attack_08_Seq_Montage")]   # Arcane Bolt
    settings = cdo(angel).get_editor_property("aim_settings")
    settings.set_editor_property("enabled", True)
    settings.set_editor_property("aim_action", action)
    if highlight:
        settings.set_editor_property("aim_highlight_material", highlight)
    settings.set_editor_property("face_aim_montages", [m for m in casts if m])
    set_props(angel, ANGEL, aim_settings=settings)
    log("%s: aiming on (crosshair with the staff out, hold RMB to aim, casts face the crosshair)" % ANGEL)


def main():
    log("pass 5")
    step_arcane_spikes()
    step_angel_aim()
    write_report("last_run_pass5.txt")


main()
