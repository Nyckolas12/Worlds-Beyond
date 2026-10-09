"""
Worlds Beyond - pass 10: enemies (Plan 3A).

Run with the editor closed, after building the C++:
    UnrealEditor-Cmd.exe <path>/WorldBeyond.uproject -run=pythonscript -script=<path>/Scripts/Migration/migrate_pass10.py -unattended -nosplash -NullRHI

- Montages for the roster's Paragon animations (/Game/WorldsBeyond/Enemies/Montages/<group>/), death montages hold
  their last frame.
- M_Beyond_Telegraph (the ground marker of telegraphed attacks: circle, ring, cone, line, filling over the wind-up) and
  M_Beyond_EnemyTint (fresnel overlay for region / affix tints) in /Game/WorldsBeyond/Enemies/Materials/. Rebuilt on
  every run like pass 6's materials.
- Two projectiles (ABeyondProjectile children), the enemies' abilities (GA_*), ability sets, eight elite affixes
  (DA_Affix_*), the seven roster enemies (DA_Enemy_*) and DA_EnemyRoster (Project Settings -> Worlds Beyond Enemies
  points at it):
    forest: Timber Wolf, Raider, Raider Slinger, Treant; corrupted woods: Hollow Wraith, Cursed Knight, Corrupted Beast.
- The test arena /Game/WorldsBeyond/Maps/TestArena: open ground with a navmesh, both demigods, a checkpoint and a
  forest and a corrupted-woods camp (ABeyondEnemySpawner).

Data is only created when missing, so editor tuning survives a re-run; BEYOND_REBUILD_ENEMIES=1 rebuilds the
abilities, affixes and enemies (asset names stay), BEYOND_REBUILD_ARENA=1 rebuilds the map.
Every asset saved is copied to Saved/MigrationBackups/<timestamp>/ first. Report: last_run_pass10.txt.
"""
import os
import sys

import unreal

sys.path.insert(0, os.path.join(os.path.abspath(unreal.Paths.project_dir()), "Scripts", "Migration"))
from migration_common import (ASSET_TOOLS, EAL, bp_class, ensure_blueprint, ensure_montage, fx, load, log, save,  # noqa: E402
                              set_props, tag, tag_container, warn, write_report)

ROOT = "/Game/WorldsBeyond/Enemies"
MONTAGES = ROOT + "/Montages"
ABILITIES = ROOT + "/Abilities"
SETS = ROOT + "/AbilitySets"
AFFIXES = ROOT + "/Affixes"
ROSTER = ROOT + "/Roster"
MATERIALS = ROOT + "/Materials"
PROJECTILES = ROOT + "/Projectiles"
ROSTER_ASSET = ROOT + "/DA_EnemyRoster"
ARENA = "/Game/WorldsBeyond/Maps/TestArena"
JIWOONG = "/Game/WorldsBeyond/Characters/Ji-Woong/BP_Ji-Woong"

PM = "/Game/ParagonMinions/Characters/"
BLACK = PM + "Buff/Buff_Black/"
RED = PM + "Buff/Buff_Red/"
BLUE = PM + "Buff/Buff_Blue/"
MELEE = PM + "Minions/Down_Minions/Animations/Melee/"
RANGED = PM + "Minions/Down_Minions/Animations/Ranged/"
SUPER = PM + "Minions/Down_Minions/Animations/Super/"
DAWN_MESHES = PM + "Minions/Down_Minions/Meshes/"
DUSK_MESHES = PM + "Minions/Dusk_Minions/Meshes/"
PFX = "/Game/ParagonMinions/FX/Particles/"
FXV = "/Game/FXVarietyPack/Particles/"

REBUILD = os.environ.get("BEYOND_REBUILD_ENEMIES", "") == "1"
REBUILD_ARENA = os.environ.get("BEYOND_REBUILD_ARENA", "") == "1"

MEL = unreal.MaterialEditingLibrary
SHAPE = unreal.BeyondStrikeShape
AIM = unreal.BeyondStrikeAim
RANK = unreal.BeyondEnemyRank

LIGHT = "Event.Hit.Light"
STAGGER = "Event.Hit.Stagger"
STUN = "Event.Hit.Stun"
KNOCK = "Event.Hit.KnockBack"


# ---------------------------------------------------------------- small helpers

def timed_fx(lifetime, **kwargs):
    value = fx(**kwargs)
    value.set_editor_property("max_lifetime", float(lifetime))
    return value


def struct(cls, **props):
    value = cls()
    for key, item in props.items():
        value.set_editor_property(key, item)
    return value


def growth(max_health=0.0, max_stamina=0.0, strength=0.0, arcana=0.0, defense=0.0):
    return struct(unreal.BeyondStatGrowth, max_health=float(max_health), max_stamina=float(max_stamina),
                  strength=float(strength), arcana=float(arcana), defense=float(defense))


def strike(**props):
    for key in ("damage_type", "hit_response", "linger_damage_type"):
        if isinstance(props.get(key), str):
            props[key] = tag(props[key])
    if "color" in props and isinstance(props["color"], tuple):
        props["color"] = unreal.LinearColor(*props["color"])
    for key in ("radius", "inner_radius", "cone_angle", "length", "width", "wind_up", "damage", "linger_duration",
                "linger_damage_per_second", "falling_mesh_scale", "fall_height"):
        if key in props:
            props[key] = float(props[key])
    return struct(unreal.BeyondStrikeSettings, **props)


def data_asset(folder, name, cls, rebuild=REBUILD):
    """(asset, fill): existing (fill only when rebuilding) or new (fill)."""
    path = "%s/%s" % (folder, name)
    exists = EAL.does_asset_exist(path)
    if exists and not rebuild:
        return load(path), False
    asset = load(path) if exists else ASSET_TOOLS.create_asset(name, folder, cls, unreal.DataAssetFactory())
    if asset is None:
        warn("could not create %s" % path)
        return None, False
    return asset, True


def seq_path(folder, name):
    return folder + name


_montages = {}


def montage(group, sequence, hold=False):
    """AM_<group>_<name> from a Paragon sequence; hold keeps the last frame (deaths)."""
    key = (group, sequence)
    if key in _montages:
        return _montages[key]
    name = sequence.rsplit("/", 1)[1]
    path = "%s/%s/AM_%s_%s" % (MONTAGES, group, group, name)
    created = not EAL.does_asset_exist(path)
    result = ensure_montage(sequence, path)
    if result is not None and created:
        try:
            source = load(sequence)
            if source is not None and source.get_editor_property("enable_root_motion"):
                log("  %s has root motion" % sequence)
        except Exception:
            pass
        if hold:
            result.set_editor_property("enable_auto_blend_out", False)
            save(result, path)
    _montages[key] = result
    return result


def combo_step(anim, damage, response=LIGHT, rate=1.0):
    return struct(unreal.BeyondComboStep, montage=anim, damage=float(damage), hit_response=tag(response), play_rate=float(rate))


def ability(folder, name, parent, **props):
    """A Blueprint child of a C++ ability with its defaults set (only when new or rebuilding)."""
    path = "%s/%s/%s" % (ABILITIES, folder, name)
    exists = EAL.does_asset_exist(path)
    bp = ensure_blueprint(path, parent)
    if bp is None:
        warn("could not create %s" % path)
        return None
    if not exists or REBUILD:
        if "cooldown_tag" in props:
            props["cooldown_tags"] = tag_container(props.pop("cooldown_tag"))
        for key in ("ai_min_range", "ai_max_range", "ai_weight", "cooldown_duration"):
            if key in props:
                props[key] = float(props[key])
        set_props(bp, path, **props)
        log("built %s" % path)
    return bp_class(path)


def melee(folder, name, steps, reach=170.0, radius=60.0, tokens=1, **props):
    return ability(folder, name, unreal.BeyondGA_MeleeCombo, combo_steps=steps, unarmed_reach=float(reach),
                   unarmed_radius=float(radius), legs_follow_movement=False, stop_if_combo_window_missed=False,
                   ai_chains_combo=True, ai_attack_token_cost=tokens, **props)


def area(folder, name, anim, aim, settings, tokens=0, **props):
    return ability(folder, name, unreal.BeyondGA_AreaAttack, montage=anim, aim=aim, strike=settings,
                   ai_attack_token_cost=tokens, **props)


def ability_set(name, classes):
    data, fill = data_asset(SETS, name, unreal.BeyondAbilitySet)
    if data is None or not fill:
        return data
    entries = []
    for cls in classes:
        if cls is None:
            continue
        entry = unreal.BeyondAbilitySet_Ability()
        entry.set_editor_property("ability", cls)
        entry.set_editor_property("level", 1)
        entries.append(entry)
    data.set_editor_property("abilities", entries)
    save(data, "%s/%s" % (SETS, name))
    log("built %s/%s (%d abilities)" % (SETS, name, len(entries)))
    return data


def projectile(name, trail, impact, speed=2200.0, radius=22.0):
    path = "%s/%s" % (PROJECTILES, name)
    exists = EAL.does_asset_exist(path)
    bp = ensure_blueprint(path, unreal.BeyondProjectile)
    if bp is None:
        return None
    if not exists or REBUILD:
        set_props(bp, path, speed=float(speed), collision_radius=float(radius), trail_fx=trail, impact_fx=impact)
        log("built %s" % path)
    return bp_class(path)


def mesh_fit(mesh, radius=None, half_height=None):
    """Capsule radius / half height and the mesh offset that puts the feet on the capsule's bottom, from the bounds."""
    try:
        bounds = mesh.get_bounds()
        origin, extent = bounds.origin, bounds.box_extent
        fit_half = max(extent.z, 40.0)
        # Between the narrow and the long side (quadrupeds are long), never thinner than a human
        fit_radius = min(max((extent.x * extent.y) ** 0.5 * 0.55, 34.0), fit_half)
        bottom = origin.z - extent.z
        log("  %s bounds: origin %.0f %.0f %.0f, extent %.0f %.0f %.0f" % (mesh.get_name(), origin.x, origin.y, origin.z,
                                                                       extent.x, extent.y, extent.z))
        return (radius or fit_radius, half_height or fit_half, unreal.Vector(0.0, 0.0, -bottom))
    except Exception as e:  # keep the explicit values
        warn("%s: no bounds (%s); using the given capsule" % (mesh.get_name(), e))
        return (radius or 40.0, half_height or 90.0, unreal.Vector(0.0, 0.0, 0.0))


def tag_map(entries, value_type):
    result = unreal.Map(unreal.GameplayTag, value_type)
    for key, value in entries.items():
        if value is not None:
            result[tag(key)] = value
    return result


# ---------------------------------------------------------------- materials

TELEGRAPH_CODE = r"""
float2 p = (UV - 0.5) * 2.0;
float2 q = float2(p.x * ForwardSign, -p.y);
float r = length(q);
float aa = 0.02;
float shape = 0.0;
float edge = 0.0;
float prog = 0.0;
if (Shape < 0.5)
{
    shape = 1.0 - smoothstep(1.0 - aa, 1.0, r);
    edge = smoothstep(0.9, 0.97, r) * shape;
    prog = 1.0 - smoothstep(Fill - aa, Fill, r);
}
else if (Shape < 1.5)
{
    shape = (1.0 - smoothstep(1.0 - aa, 1.0, r)) * smoothstep(Inner - aa, Inner, r);
    edge = saturate(smoothstep(0.9, 0.97, r) + 1.0 - smoothstep(Inner, Inner + 0.07, r)) * shape;
    prog = 1.0 - smoothstep(Fill - aa, Fill, (r - Inner) / max(1.0 - Inner, 0.001));
}
else if (Shape < 2.5)
{
    float ang = abs(atan2(q.y, q.x));
    float inAng = 1.0 - smoothstep(HalfAngle - 0.02, HalfAngle, ang);
    shape = (1.0 - smoothstep(1.0 - aa, 1.0, r)) * inAng;
    edge = max(smoothstep(0.9, 0.97, r), smoothstep(HalfAngle - 0.07, HalfAngle - 0.01, ang)) * shape;
    prog = 1.0 - smoothstep(Fill - aa, Fill, r);
}
else
{
    shape = (1.0 - smoothstep(1.0 - aa, 1.0, abs(q.x))) * (1.0 - smoothstep(1.0 - aa * 2.0, 1.0, abs(q.y)));
    edge = max(smoothstep(0.9, 0.97, abs(q.y)), smoothstep(0.95, 0.99, abs(q.x))) * shape;
    prog = 1.0 - smoothstep(Fill - aa, Fill, q.x * 0.5 + 0.5);
}
float pulse = 0.5 + 0.5 * sin(Time * 6.0);
float a = saturate(shape * 0.16 + prog * shape * (0.35 + 0.3 * Fill) + edge * 0.9);
a = lerp(a, shape * (0.4 + 0.2 * pulse) + edge * 0.5, Linger);
float3 rgb = Color.rgb * (1.0 + edge * 1.5 + prog * 0.6 + Linger * pulse * 0.5);
return float4(rgb, saturate(a) * Opacity * Color.a);
"""

TINT_CODE = r"""
float f = pow(saturate(1.0 - dot(normalize(N), normalize(V))), 2.5);
float a = saturate(f * Color.a * 1.1 + Color.a * 0.1);
return float4(Color.rgb * (f * 3.0 + 0.35), a);
"""


def _node(material, cls, x, y, **props):
    expression = MEL.create_material_expression(material, cls, x, y)
    for key, value in props.items():
        expression.set_editor_property(key, value)
    return expression


def _link(source, target, target_input, source_output=""):
    if not MEL.connect_material_expressions(source, source_output, target, target_input):
        warn("could not connect %s -> %s.%s" % (source.get_name(), target.get_name(), target_input or "Input"))
        return False
    return True


def _custom_material(name, code, inputs, domain, shading=None, description=""):
    """A material whose colour and opacity come from one Custom node. inputs: (name, kind, default)."""
    path = "%s/%s" % (MATERIALS, name)
    if EAL.does_asset_exist(path):
        material = load(path)
        if material is None:
            return None
        MEL.delete_all_material_expressions(material)
        action = "rebuilt"
    else:
        material = ASSET_TOOLS.create_asset(name, MATERIALS, unreal.Material, unreal.MaterialFactoryNew())
        if material is None:
            warn("could not create %s" % path)
            return None
        action = "created"

    material.set_editor_property("material_domain", domain)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    if shading is not None:
        material.set_editor_property("shading_model", shading)

    custom = _node(material, unreal.MaterialExpressionCustom, -350, 0)
    custom.set_editor_property("code", code)
    custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    custom.set_editor_property("description", description)
    custom_inputs = []
    for input_name, _kind, _default in inputs:
        custom_input = unreal.CustomInput()
        custom_input.set_editor_property("input_name", input_name)
        custom_inputs.append(custom_input)
    custom.set_editor_property("inputs", custom_inputs)

    y = -400
    for input_name, kind, default in inputs:
        if kind == "uv":
            source = _node(material, unreal.MaterialExpressionTextureCoordinate, -800, y)
        elif kind == "time":
            source = _node(material, unreal.MaterialExpressionTime, -800, y)
        elif kind == "normal":
            source = _node(material, unreal.MaterialExpressionVertexNormalWS, -800, y)
        elif kind == "camera":
            source = _node(material, unreal.MaterialExpressionCameraVectorWS, -800, y)
        elif kind == "scalar":
            source = _node(material, unreal.MaterialExpressionScalarParameter, -800, y,
                           parameter_name=input_name, default_value=float(default))
        elif kind == "vector":
            source = _node(material, unreal.MaterialExpressionVectorParameter, -800, y,
                           parameter_name=input_name, default_value=unreal.LinearColor(*default))
        else:
            raise ValueError(kind)
        _link(source, custom, input_name)
        y += 110

    color = _node(material, unreal.MaterialExpressionComponentMask, -120, -60, r=True, g=True, b=True, a=False)
    alpha = _node(material, unreal.MaterialExpressionComponentMask, -120, 60, r=False, g=False, b=False, a=True)
    for mask in (color, alpha):
        if not MEL.connect_material_expressions(custom, "", mask, "") and not _link(custom, mask, "Input"):
            warn("%s: the Custom node isn't connected to its masks" % name)
    if not MEL.connect_material_property(color, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR):
        warn("%s: could not connect Emissive" % name)
    if domain == unreal.MaterialDomain.MD_DEFERRED_DECAL:
        MEL.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    if not MEL.connect_material_property(alpha, "", unreal.MaterialProperty.MP_OPACITY):
        warn("%s: could not connect Opacity" % name)

    MEL.recompile_material(material)
    save(material, path)
    log("%s %s" % (action, path))
    return material


def step_materials():
    _custom_material(
        "M_Beyond_Telegraph", TELEGRAPH_CODE,
        [("UV", "uv", None), ("Time", "time", None), ("Shape", "scalar", 0.0), ("Fill", "scalar", 0.0),
         ("Inner", "scalar", 0.0), ("HalfAngle", "scalar", 0.785), ("Opacity", "scalar", 1.0), ("Linger", "scalar", 0.0),
         ("ForwardSign", "scalar", 1.0), ("Color", "vector", (1.0, 0.28, 0.08, 1.0))],
        unreal.MaterialDomain.MD_DEFERRED_DECAL,
        description="Telegraph: Shape 0 circle, 1 ring, 2 cone, 3 line; Fill 0..1 over the wind-up; Linger = hazard look. "
                    "ForwardSign -1 flips cones / lines if they ever point backwards.")
    _custom_material(
        "M_Beyond_EnemyTint", TINT_CODE,
        [("N", "normal", None), ("V", "camera", None), ("Color", "vector", (1.0, 0.4, 0.1, 0.6))],
        unreal.MaterialDomain.MD_SURFACE, shading=unreal.MaterialShadingModel.MSM_UNLIT,
        description="Fresnel overlay for enemy tints: Color.rgb, alpha = strength")


# ---------------------------------------------------------------- abilities

def step_abilities():
    """Every roster enemy's abilities and ability set; returns {enemy key: ability set}."""
    sets = {}
    slinger_shot = projectile(
        "BP_Beyond_SlingerShot",
        trail=timed_fx(0, system=FXV + "P_ky_fireBall", scale=0.25),
        impact=timed_fx(2, system=FXV + "P_ky_hit1", scale=0.5),
        speed=2300.0, radius=20.0)
    wraith_bolt = projectile(
        "BP_Beyond_WraithBolt",
        trail=timed_fx(0, system=PFX + "Buffs/Buff_Blue/FX/P_Spit_Attack_Projectile", scale=1.0),
        impact=timed_fx(2, system=PFX + "Buffs/Buff_Blue/FX/P_Spit_Attack_Impact"),
        speed=1700.0, radius=26.0)

    # Timber Wolf: bites, pounces
    bite = melee("Wolf", "GA_Wolf_Bite", [
        combo_step(montage("Wolf", BLACK + "Animations/BiteAttack_A"), 12),
        combo_step(montage("Wolf", BLACK + "Animations/BiteAttack_B"), 14)],
        reach=150.0, radius=70.0, ai_max_range=200, ai_weight=3)
    pounce = ability("Wolf", "GA_Wolf_Pounce", unreal.BeyondGA_Dash,
                     dash_distance=520.0, dash_duration=0.35, exit_speed=150.0, invincible_while_dashing=False,
                     pass_through_pawns=False, dash_montage=montage("Wolf", BLACK + "Animations/BiteAttack_Fast"),
                     path_damage=16.0, path_radius=110.0, detonate_delay=0.05, path_damage_type=tag("DamageType.Melee"),
                     path_hit_response=tag(STAGGER), ai_min_range=320, ai_max_range=650, ai_weight=2,
                     cooldown_duration=7, cooldown_tag="Cooldown.Enemy.Mobility", ai_attack_token_cost=1)
    sets["wolf"] = ability_set("DA_AbilitySet_Enemy_Wolf", [bite, pounce])

    # Raider: three-hit combo, cleave
    combo = melee("Raider", "GA_Raider_Combo", [
        combo_step(montage("Raider", MELEE + "Attack_A"), 14),
        combo_step(montage("Raider", MELEE + "Attack_B"), 14),
        combo_step(montage("Raider", MELEE + "Attack_C"), 20, STAGGER)],
        reach=175.0, radius=70.0, ai_max_range=220, ai_weight=3)
    cleave = area("Raider", "GA_Raider_Cleave", montage("Raider", MELEE + "Attack_D"), AIM.FROM_SELF_TOWARD_TARGET,
                  strike(shape=SHAPE.CONE, radius=330, cone_angle=100, wind_up=0.9, damage=28, damage_type="DamageType.Melee",
                         hit_response=STAGGER, impact_fx=timed_fx(2, system=PFX + "Minions/Shared/P_Minion_Melee_Impact")),
                  tokens=1, ai_max_range=280, ai_weight=1.5, cooldown_duration=7, cooldown_tag="Cooldown.Enemy.Special")
    sets["raider"] = ability_set("DA_AbilitySet_Enemy_Raider", [combo, cleave])

    # Raider Slinger: shoots from range, shoves when crowded
    shot = ability("Slinger", "GA_Slinger_Shot", unreal.BeyondGA_Projectile,
                   projectile_class=slinger_shot, cast_montage=montage("Slinger", RANGED + "Fire_A"), damage=13.0,
                   hit_response=tag(LIGHT), projectile_speed=2300.0, max_range=2400.0, fallback_fire_delay=0.3,
                   ai_min_range=250, ai_max_range=1600, ai_weight=3, cooldown_duration=1.8, cooldown_tag="Cooldown.Enemy.Primary")
    bash = melee("Slinger", "GA_Slinger_Bash", [combo_step(montage("Slinger", RANGED + "Melee_Combo_A"), 10, STAGGER)],
                 reach=150.0, ai_max_range=180, ai_weight=1)
    sets["slinger"] = ability_set("DA_AbilitySet_Enemy_Slinger", [shot, bash])

    # Treant: heavy fists, ground smash, uppercut
    pummel = melee("Treant", "GA_Treant_Pummel", [
        combo_step(montage("Treant", RED + "Animations/Attack_Punch_01"), 20),
        combo_step(montage("Treant", RED + "Animations/Attack_Punch_02"), 24, STAGGER)],
        reach=230.0, radius=95.0, ai_max_range=260, ai_weight=2)
    smash = area("Treant", "GA_Treant_Smash", montage("Treant", RED + "Animations/Attack_BigSmash"), AIM.AT_SELF,
                 strike(shape=SHAPE.CIRCLE, radius=430, wind_up=1.25, damage=38, damage_type="DamageType.Melee",
                        hit_response=KNOCK, impact_fx=timed_fx(2, system=PFX + "Buffs/Buff_Red/FX/P_Buff_Red_BigSmash_Impact")),
                 tokens=1, ai_max_range=380, ai_weight=1.5, cooldown_duration=9, cooldown_tag="Cooldown.Enemy.Special")
    uppercut = area("Treant", "GA_Treant_Uppercut", montage("Treant", RED + "Animations/Attack_Uppercut"),
                    AIM.FROM_SELF_TOWARD_TARGET,
                    strike(shape=SHAPE.CONE, radius=300, cone_angle=70, wind_up=0.75, damage=26, damage_type="DamageType.Melee",
                           hit_response=STAGGER, impact_fx=timed_fx(2, system=PFX + "Buffs/Buff_Red/FX/P_Buff_Red_Melee_Impact")),
                    tokens=1, ai_max_range=280, ai_weight=1, cooldown_duration=6, cooldown_tag="Cooldown.Enemy.Secondary")
    sets["treant"] = ability_set("DA_AbilitySet_Enemy_Treant", [pummel, smash, uppercut])

    # Hollow Wraith: dark bolts, soul burst under the target
    bolt = ability("Wraith", "GA_Wraith_Bolt", unreal.BeyondGA_Projectile,
                   projectile_class=wraith_bolt, cast_montage=montage("Wraith", BLUE + "Animations/Fire_A"), damage=15.0,
                   hit_response=tag(LIGHT), projectile_speed=1700.0, max_range=2200.0, fallback_fire_delay=0.35,
                   ai_min_range=200, ai_max_range=1500, ai_weight=3, cooldown_duration=2.2, cooldown_tag="Cooldown.Enemy.Primary")
    burst = area("Wraith", "GA_Wraith_SoulBurst", montage("Wraith", BLUE + "Animations/Fire_B"), AIM.AT_TARGET,
                 strike(shape=SHAPE.CIRCLE, radius=260, wind_up=1.3, damage=28, damage_type="DamageType.Explosion",
                        hit_response=LIGHT, color=(0.55, 0.2, 1.0, 1.0),
                        impact_fx=timed_fx(2.5, system=FXV + "P_ky_darkStorm", scale=0.45)),
                 ai_max_range=1300, ai_weight=1.5, cooldown_duration=8, cooldown_tag="Cooldown.Enemy.Special")
    sets["wraith"] = ability_set("DA_AbilitySet_Enemy_Wraith", [bolt, burst])

    # Cursed Knight: combo, wide cleave, overhead line
    knight_combo = melee("Knight", "GA_Knight_Combo", [
        combo_step(montage("Knight", SUPER + "Attack_A_Dusk"), 20),
        combo_step(montage("Knight", SUPER + "Attack_B_Dusk"), 22),
        combo_step(montage("Knight", SUPER + "Attack_C_Dusk"), 30, STAGGER)],
        reach=220.0, radius=90.0, ai_max_range=260, ai_weight=3)
    knight_cleave = area("Knight", "GA_Knight_Cleave", montage("Knight", SUPER + "Attack_D_Dusk"), AIM.FROM_SELF_TOWARD_TARGET,
                         strike(shape=SHAPE.CONE, radius=400, cone_angle=120, wind_up=1.0, damage=42,
                                damage_type="DamageType.Melee", hit_response=STAGGER,
                                impact_fx=timed_fx(2, system=PFX + "Minions/Shared/P_Minion_Melee_Impact", scale=1.5)),
                         tokens=1, ai_max_range=350, ai_weight=1.5, cooldown_duration=8, cooldown_tag="Cooldown.Enemy.Special")
    overhead = area("Knight", "GA_Knight_Overhead", montage("Knight", SUPER + "Attack_F_Dusk"), AIM.FROM_SELF_TOWARD_TARGET,
                    strike(shape=SHAPE.LINE, length=700, width=200, wind_up=1.15, damage=50, damage_type="DamageType.Melee",
                           hit_response=KNOCK, color=(0.6, 0.2, 1.0, 1.0),
                           impact_fx=timed_fx(2, system=PFX + "Minions/Prime_Helix/Abilities/SpecialAttack3/FX/P_Prime_Ground_Fist_Impact")),
                    tokens=1, ai_min_range=150, ai_max_range=650, ai_weight=1, cooldown_duration=12,
                    cooldown_tag="Cooldown.Enemy.Ultimate")
    sets["knight"] = ability_set("DA_AbilitySet_Enemy_Knight", [knight_combo, knight_cleave, overhead])

    # Corrupted Beast: bites, poison spit that leaves a pool
    beast_bite = melee("Beast", "GA_Beast_Bite", [
        combo_step(montage("Beast", BLACK + "Animations/BiteAttack_A"), 16),
        combo_step(montage("Beast", BLACK + "Animations/BiteAttack_B"), 18, STAGGER)],
        reach=160.0, radius=75.0, ai_max_range=210, ai_weight=2.5)
    spit = area("Beast", "GA_Beast_Spit", montage("Beast", BLACK + "Animations/Spit_Attack"), AIM.AT_TARGET,
                strike(shape=SHAPE.CIRCLE, radius=230, wind_up=1.0, damage=12, damage_type="DamageType.Explosion",
                       hit_response=LIGHT, linger_duration=3.5, linger_damage_per_second=9,
                       linger_damage_type="DamageType.Proc.Poison", color=(0.35, 0.95, 0.2, 1.0),
                       impact_fx=timed_fx(2, system=PFX + "Buffs/Buff_Black/Abilities/RangedAttack/FX/P_Spit_Attack_Impact"),
                       linger_fx=timed_fx(0, system=PFX + "SharedGameplay/States/Poison/P_Poison", scale=1.5)),
                ai_min_range=300, ai_max_range=1100, ai_weight=1.5, cooldown_duration=7, cooldown_tag="Cooldown.Enemy.Special")
    sets["beast"] = ability_set("DA_AbilitySet_Enemy_Beast", [beast_bite, spit])

    # Affix extras
    venom_spit = area("Affix", "GA_Affix_VenomSpit", None, AIM.AT_TARGET,
                      strike(shape=SHAPE.CIRCLE, radius=200, wind_up=1.0, damage=8, damage_type="DamageType.Explosion",
                             linger_duration=3, linger_damage_per_second=8, linger_damage_type="DamageType.Proc.Poison",
                             color=(0.35, 0.95, 0.2, 1.0),
                             impact_fx=timed_fx(2, system=PFX + "Buffs/Buff_Black/Abilities/RangedAttack/FX/P_Spit_Attack_Impact"),
                             linger_fx=timed_fx(0, system=PFX + "SharedGameplay/States/Poison/P_Poison")),
                      min_duration=0.4, ai_min_range=250, ai_max_range=1000, ai_weight=1, cooldown_duration=9,
                      cooldown_tag="Cooldown.Enemy.Buff")
    lunge = ability("Affix", "GA_Affix_Lunge", unreal.BeyondGA_Dash,
                    dash_distance=450.0, dash_duration=0.25, exit_speed=150.0, invincible_while_dashing=False,
                    pass_through_pawns=False, path_damage=14.0, path_radius=100.0, detonate_delay=0.05,
                    path_damage_type=tag("DamageType.Melee"), path_hit_response=tag(STAGGER),
                    ai_min_range=300, ai_max_range=650, ai_weight=1.5, cooldown_duration=7,
                    cooldown_tag="Cooldown.Enemy.Mobility", ai_attack_token_cost=1)
    sets["affix_venom"] = ability_set("DA_AbilitySet_Affix_Venomous", [venom_spit])
    sets["affix_swift"] = ability_set("DA_AbilitySet_Affix_Swift", [lunge])
    return sets


# ---------------------------------------------------------------- affixes

def make_affix(name, **props):
    path = "%s/%s" % (AFFIXES, name)
    affix, fill = data_asset(AFFIXES, name, unreal.BeyondAffixDefinition)
    if affix is None or not fill:
        if affix is not None:
            log("kept existing %s (BEYOND_REBUILD_ENEMIES=1 rebuilds it)" % path)
        return affix
    props["affix_id"] = unreal.Name(props["affix_id"])
    props["prefix"] = unreal.Text(props["prefix"])
    props["description"] = unreal.Text(props["description"])
    props["affix_tag"] = tag(props["affix_tag"])
    props["tint"] = unreal.LinearColor(*props["tint"])
    for key in ("on_hit_damage_type", "ward_break_damage_type", "pulse_damage_type"):
        if key in props:
            props[key] = tag(props[key])
    if "granted_tags" in props:
        props["granted_tags"] = tag_container(*props["granted_tags"])
    if "damage_taken_multipliers" in props:
        props["damage_taken_multipliers"] = tag_map(props["damage_taken_multipliers"], float)
    for key, value in props.items():
        if isinstance(value, int) and not isinstance(value, bool) and key not in ("split_count",):
            value = float(value)
        try:
            affix.set_editor_property(key, value)
        except Exception as e:
            warn("%s: could not set %s (%s)" % (path, key, e))
    save(affix, path)
    log("built %s" % path)
    return affix


def step_affixes(sets):
    burning = "/Game/ParagonSerath/FX/Particles/Abilities/Ultimate/FX/P_Burning"
    green_flames = "/Game/ParagonSevarog/FX/Particles/Abilities/Ultimate/FX/P_Ult_GreenFlames"
    affixes = [
        make_affix("DA_Affix_Molten", affix_id="molten", prefix="Molten", affix_tag="Enemy.Affix.Molten",
                   description="Its hits set you ablaze; it leaves burning ground when it dies.",
                   tint=(1.0, 0.35, 0.05, 0.7), aura_fx=timed_fx(0, system=burning, scale=0.45),
                   on_hit_damage_per_second=6, on_hit_duration=3, on_hit_damage_type="DamageType.Proc.Burn",
                   on_hit_target_fx=timed_fx(3, system=burning, scale=0.4),
                   death_action=unreal.BeyondAffixDeathAction.LINGERING_POOL, pool_radius=260, pool_damage_per_second=15,
                   pool_duration=5, pool_fx=timed_fx(0, system=burning, scale=1.2)),
        make_affix("DA_Affix_Venomous", affix_id="venomous", prefix="Venomous", affix_tag="Enemy.Affix.Venomous",
                   description="Its hits poison you, and it spits venom that pools on the ground.",
                   tint=(0.3, 1.0, 0.25, 0.6), aura_fx=timed_fx(0, system=PFX + "SharedGameplay/States/Poison/P_Poison"),
                   on_hit_damage_per_second=6, on_hit_duration=4, on_hit_damage_type="DamageType.Proc.Poison",
                   on_hit_target_fx=timed_fx(4, system=green_flames, scale=0.35), granted_abilities=sets.get("affix_venom")),
        make_affix("DA_Affix_Stormcharged", affix_id="stormcharged", prefix="Stormcharged",
                   affix_tag="Enemy.Affix.Stormcharged",
                   description="Lightning pulses around it every few seconds; step away when the ring lights up.",
                   tint=(0.3, 0.55, 1.0, 0.7), pulse_interval=8, pulse_radius=420, pulse_damage=25, pulse_wind_up=1.1,
                   pulse_damage_type="DamageType.Proc.Lightning",
                   pulse_fx=timed_fx(1.5, system=FXV + "P_ky_lightning2", scale=0.8, color=(0.3, 0.6, 1.0, 1.0))),
        make_affix("DA_Affix_Warded", affix_id="warded", prefix="Warded", affix_tag="Enemy.Affix.Warded",
                   description="Spells barely hurt it until a sword hit breaks its ward for 5 seconds.",
                   tint=(0.75, 0.45, 1.0, 0.6),
                   aura_fx=timed_fx(0, system=PFX + "SharedGameplay/States/Shield/p_ShieldLoop"),
                   ward=True, ward_break_damage_type="DamageType.Melee", ward_break_duration=5,
                   ward_break_fx=timed_fx(1.5, system=PFX + "SharedGameplay/States/Shield/P_Shield_Deactivate"),
                   damage_taken_multipliers={"DamageType.Projectile": 0.4, "DamageType.Explosion": 0.4, "DamageType.Proc": 0.6}),
        make_affix("DA_Affix_Juggernaut", affix_id="juggernaut", prefix="Juggernaut", affix_tag="Enemy.Affix.Juggernaut",
                   description="Bigger, much tougher and impossible to stagger; keep your distance.",
                   tint=(0.6, 0.62, 0.68, 0.6),
                   aura_fx=timed_fx(0, system=PFX + "SharedGameplay/States/Juggernaut/P_CC_Immunity"),
                   health_multiplier=1.5, scale_multiplier=1.2, speed_multiplier=0.9, bonus_defense=40,
                   granted_tags=["State.Uninterruptible"]),
        make_affix("DA_Affix_Swift", affix_id="swift", prefix="Swift", affix_tag="Enemy.Affix.Swift",
                   description="Much faster, and it lunges at you from a distance.",
                   tint=(1.0, 0.95, 0.4, 0.5),
                   aura_fx=timed_fx(0, system=PFX + "SharedGameplay/States/Speed/P_Shared_SpeedBuff"),
                   speed_multiplier=1.45, granted_abilities=sets.get("affix_swift")),
        make_affix("DA_Affix_Vampiric", affix_id="vampiric", prefix="Vampiric", affix_tag="Enemy.Affix.Vampiric",
                   description="It heals by 30 % of the damage it deals.", tint=(0.85, 0.05, 0.12, 0.7), lifesteal=0.3),
        make_affix("DA_Affix_Brood", affix_id="brood", prefix="Brood", affix_tag="Enemy.Affix.Brood",
                   description="It splits into two smaller copies when it dies.", tint=(0.5, 0.85, 0.3, 0.55),
                   death_action=unreal.BeyondAffixDeathAction.SPLIT, split_count=2, split_health_fraction=0.35,
                   split_scale=0.7),
    ]
    return [a for a in affixes if a is not None]


# ---------------------------------------------------------------- enemies

def make_enemy(name, mesh_path, ai, hit, deaths, **props):
    path = "%s/%s" % (ROSTER, name)
    enemy, fill = data_asset(ROSTER, name, unreal.BeyondEnemyDefinition)
    if enemy is None or not fill:
        if enemy is not None:
            log("kept existing %s (BEYOND_REBUILD_ENEMIES=1 rebuilds it)" % path)
        return enemy

    mesh = load(mesh_path)
    if mesh is None:
        warn("%s: mesh %s missing" % (path, mesh_path))
        return None
    radius, half_height, offset = mesh_fit(mesh, props.pop("capsule_radius", None), props.pop("capsule_half_height", None))

    props["enemy_id"] = unreal.Name(props["enemy_id"])
    props["display_name"] = unreal.Text(props["display_name"])
    props["description"] = unreal.Text(props.get("description", ""))
    props["region"] = tag(props["region"])
    props["tint"] = unreal.LinearColor(*props.get("tint", (0.0, 0.0, 0.0, 0.0)))
    props["mesh"] = mesh
    props["capsule_radius"] = float(radius)
    props["capsule_half_height"] = float(half_height)
    props["mesh_offset"] = offset
    props["hit_reactions"] = tag_map(hit, unreal.AnimMontage)
    props["death_montages"] = [m for m in deaths if m is not None]
    props["ai"] = struct(unreal.BeyondEnemyAIConfig, **{k: (float(v) if isinstance(v, int) and not isinstance(v, bool) else v)
                                                       for k, v in ai.items()})
    if "locomotion" in props and isinstance(props["locomotion"], str):
        props["locomotion"] = load(props["locomotion"])
    for key in ("max_health", "strength", "arcana", "defense", "walk_speed", "scale", "light_hit_react_cooldown",
                "experience_reward"):
        if key in props:
            props[key] = float(props[key])
    for key, value in props.items():
        try:
            enemy.set_editor_property(key, value)
        except Exception as e:
            warn("%s: could not set %s (%s)" % (path, key, e))
    save(enemy, path)
    log("built %s: %s, capsule %.0f x %.0f, %.0f health" % (path, mesh_path.rsplit("/", 1)[1], radius, half_height,
                                                           props.get("max_health", 0.0)))
    return enemy


def step_enemies(sets):
    reg = RANK.REGULAR
    forest = "Region.Forest"
    woods = "Region.CorruptedWoods"
    enemies = [
        make_enemy("DA_Enemy_TimberWolf", BLACK + "Meshes/Buff_Black",
                   dict(sight_radius=1800, alert_radius=1500, wander_radius=450, strafe_radius=320),
                   {LIGHT: montage("Wolf", BLACK + "Animations/HitReaction_FWD"),
                    KNOCK: montage("Wolf", BLACK + "Animations/KnockUp_BWD"),
                    STUN: montage("Wolf", BLACK + "Animations/Stunned")},
                   [montage("Wolf", BLACK + "Animations/Death_front", hold=True),
                    montage("Wolf", BLACK + "Animations/Death_back", hold=True)],
                   enemy_id="forest_wolf", display_name="Timber Wolf", region=forest, rank=reg, pack_size=3,
                   description="Hunts in packs of three; bites and pounces.",
                   locomotion=BLACK + "Animations/BlendSpaces/Locomotion", tint=(0.55, 0.55, 0.62, 0.35), scale=0.75,
                   max_health=90, strength=0, defense=0, walk_speed=520, ability_set=sets.get("wolf"),
                   growth=growth(max_health=11, strength=2, defense=0.5),
                   spawn_montage=montage("Wolf", BLACK + "Animations/Spawn")),
        make_enemy("DA_Enemy_Raider", DAWN_MESHES + "Minion_Lane_Melee_Dawn",
                   dict(sight_radius=1700, alert_radius=1300, wander_radius=300),
                   {LIGHT: montage("Raider", MELEE + "HitReact_Front"), STAGGER: montage("Raider", MELEE + "HitReact_Back"),
                    KNOCK: montage("Raider", MELEE + "KnockUp"), STUN: montage("Raider", MELEE + "Stun")},
                   [montage("Raider", MELEE + "Death_A", hold=True), montage("Raider", MELEE + "Death_B", hold=True),
                    montage("Raider", MELEE + "Death_C", hold=True)],
                   enemy_id="forest_raider", display_name="Raider", region=forest, rank=reg, pack_size=2,
                   description="A forest bandit: three-hit combos and a wide cleave.",
                   locomotion=MELEE + "Blendspaces/IdleToRun_A_Combat", max_health=140, strength=5, defense=5,
                   walk_speed=400, ability_set=sets.get("raider"), growth=growth(max_health=15, strength=2, defense=1)),
        make_enemy("DA_Enemy_RaiderSlinger", DAWN_MESHES + "Minion_Lane_Ranged_Dawn",
                   dict(sight_radius=2000, alert_radius=1300, wander_radius=250, preferred_range=1100,
                        keep_away_distance=600),
                   {LIGHT: montage("Slinger", RANGED + "HitReact_Front_A"), KNOCK: montage("Slinger", RANGED + "KnockUp_A"),
                    STUN: montage("Slinger", RANGED + "Stun")},
                   [montage("Slinger", RANGED + "Death_Front_A", hold=True), montage("Slinger", RANGED + "Death_Back_A", hold=True)],
                   enemy_id="forest_slinger", display_name="Raider Slinger", region=forest, rank=reg, pack_size=1,
                   description="Keeps its distance and shoots; shoves you away when you close in.",
                   locomotion=RANGED + "Blendspaces/IdleToRun_A_Combat", max_health=100, arcana=6, defense=2,
                   walk_speed=380, ability_set=sets.get("slinger"), growth=growth(max_health=11, arcana=2, defense=0.5)),
        make_enemy("DA_Enemy_Treant", RED + "Meshes/Buff_Red",
                   dict(sight_radius=1500, sight_half_angle=60, alert_radius=1000, wander_radius=150, strafe_radius=450),
                   {LIGHT: montage("Treant", RED + "Animations/Hit_Front"), KNOCK: montage("Treant", RED + "Animations/Knock_Bwd"),
                    STUN: montage("Treant", RED + "Animations/Stunned")},
                   [montage("Treant", RED + "Animations/Death_Fwd", hold=True), montage("Treant", RED + "Animations/Death_Back", hold=True)],
                   enemy_id="forest_treant", display_name="Treant", region=forest, rank=reg, pack_size=1,
                   description="A slow, heavy guardian of the old trees: fists, ground smash, uppercut.",
                   locomotion=RED + "Animations/Blendspaces/MovementBlends", tint=(0.25, 0.55, 0.15, 0.55),
                   max_health=320, strength=10, defense=15, walk_speed=300, light_hit_react_cooldown=3.0,
                   experience_reward=45, ability_set=sets.get("treant"), growth=growth(max_health=30, strength=3, defense=2),
                   spawn_montage=montage("Treant", RED + "Animations/Spawn")),
        make_enemy("DA_Enemy_HollowWraith", BLUE + "Meshes/Buff_Blue",
                   dict(sight_radius=2000, alert_radius=1400, wander_radius=400, preferred_range=1000,
                        keep_away_distance=700, uses_attack_tokens=False),
                   {LIGHT: montage("Wraith", BLUE + "Animations/Hitreat_Fwd"), KNOCK: montage("Wraith", BLUE + "Animations/KnockBack_Bwd"),
                    STUN: montage("Wraith", BLUE + "Animations/Stunned")},
                   [montage("Wraith", BLUE + "Animations/Death_Fwd", hold=True), montage("Wraith", BLUE + "Animations/Death_Bwd", hold=True)],
                   enemy_id="woods_wraith", display_name="Hollow Wraith", region=woods, rank=reg, pack_size=2,
                   description="A drifting husk of dark magic: bolts from afar, soul bursts under your feet.",
                   locomotion=BLUE + "Animations/Blendspaces/Fly_MidSpeed_FullSpeed", tint=(0.45, 0.15, 0.85, 0.6),
                   max_health=110, arcana=10, defense=3, walk_speed=420, ability_set=sets.get("wraith"),
                   growth=growth(max_health=12, arcana=3, defense=0.5),
                   spawn_montage=montage("Wraith", BLUE + "Animations/Spawn_A")),
        make_enemy("DA_Enemy_CursedKnight", DUSK_MESHES + "Minion_Lane_Super_Dusk",
                   dict(sight_radius=1700, alert_radius=1500, wander_radius=200, strafe_radius=420),
                   {LIGHT: montage("Knight", SUPER + "Hit_React_Front"), KNOCK: montage("Knight", SUPER + "KnockUp"),
                    STUN: montage("Knight", SUPER + "Stun")},
                   [montage("Knight", SUPER + "Death_Front", hold=True), montage("Knight", SUPER + "Death_Back", hold=True)],
                   enemy_id="woods_knight", display_name="Cursed Knight", region=woods, rank=RANK.ELITE, pack_size=1,
                   description="An oath-breaker in black armour: heavy combos, a wide cleave and a crushing overhead.",
                   locomotion=SUPER + "Blendspaces/Idle2Run_Super", tint=(0.45, 0.15, 0.75, 0.5),
                   max_health=380, strength=15, defense=25, walk_speed=360, light_hit_react_cooldown=4.0,
                   ability_set=sets.get("knight"), growth=growth(max_health=34, strength=3, defense=3)),
        make_enemy("DA_Enemy_CorruptedBeast", BLACK + "Meshes/Buff_Black",
                   dict(sight_radius=1700, alert_radius=1300, wander_radius=400, strafe_radius=340),
                   {LIGHT: montage("Beast", BLACK + "Animations/HitReaction_FWD"),
                    KNOCK: montage("Beast", BLACK + "Animations/KnockUp_BWD"),
                    STUN: montage("Beast", BLACK + "Animations/Stunned")},
                   [montage("Beast", BLACK + "Animations/Death_front", hold=True)],
                   enemy_id="woods_beast", display_name="Corrupted Beast", region=woods, rank=reg, pack_size=2,
                   description="A beast twisted by the dark: bites and spits poison that pools on the ground.",
                   locomotion=BLACK + "Animations/BlendSpaces/Locomotion", tint=(0.35, 0.9, 0.2, 0.5), scale=1.1,
                   max_health=160, strength=8, arcana=6, defense=4, walk_speed=450, ability_set=sets.get("beast"),
                   growth=growth(max_health=17, strength=2, arcana=2, defense=1),
                   spawn_montage=montage("Beast", BLACK + "Animations/Spawn")),
    ]
    return [e for e in enemies if e is not None]


def step_roster(enemies, affixes):
    roster, _fill = data_asset(ROOT, "DA_EnemyRoster", unreal.BeyondEnemyRoster, rebuild=True)
    if roster is None:
        return None
    # Keep entries added by hand (and the bosses of pass 11); add ours
    current = [e for e in roster.get_editor_property("enemies") if e is not None]
    names = {e.get_path_name() for e in current}
    current += [e for e in enemies if e.get_path_name() not in names]
    current_affixes = [a for a in roster.get_editor_property("affixes") if a is not None]
    affix_names = {a.get_path_name() for a in current_affixes}
    current_affixes += [a for a in affixes if a.get_path_name() not in affix_names]
    roster.set_editor_property("enemies", current)
    roster.set_editor_property("affixes", current_affixes)
    save(roster, ROSTER_ASSET)
    log("%s: %d enemies, %d affixes" % (ROSTER_ASSET, len(current), len(current_affixes)))
    return roster


# ---------------------------------------------------------------- test arena

def _spawn(actors, cls, location, yaw=0.0, label=None):
    actor = actors.spawn_actor_from_class(cls, unreal.Vector(*location), unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw))
    if actor is not None and label:
        actor.set_actor_label(label)
    return actor


def _spawner(actors, label, location, entries, elite_chance):
    spawner = _spawn(actors, unreal.BeyondEnemySpawner, location, label=label)
    if spawner is None:
        warn("could not place %s" % label)
        return
    rows = []
    for enemy, count, level in entries:
        if enemy is None:
            continue
        rows.append(struct(unreal.BeyondSpawnEntry, enemy=enemy, count=count, level=level))
    spawner.set_editor_property("entries", rows)
    spawner.set_editor_property("elite_chance", float(elite_chance))
    spawner.set_editor_property("spawn_radius", 550.0)
    log("placed %s (%d entries)" % (label, len(rows)))


def ensure_arena_navmesh(levels):
    """The arena exists: make sure it has a built navmesh (an earlier run could not build it)."""
    if not levels.load_level(ARENA):
        warn("could not open %s to check its navmesh" % ARENA)
        return
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    # Cheap on this small map: always rebuild, so a navmesh actor left empty by an earlier run gets its tiles
    if unreal.BeyondEditorLibrary.build_navigation(world):
        levels.save_current_level()
        log("built the navmesh of %s" % ARENA)
    else:
        warn("the navmesh of %s still isn't built: open it and use Build -> Build Paths" % ARENA)


def step_arena(enemies_by_id):
    exists = EAL.does_asset_exist(ARENA)
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if exists and not REBUILD_ARENA:
        log("kept existing %s (BEYOND_REBUILD_ARENA=1 rebuilds it)" % ARENA)
        ensure_arena_navmesh(levels)
        return
    if exists:
        EAL.delete_asset(ARENA)

    created = False
    try:
        created = levels.new_level_from_template(ARENA, "/Engine/Maps/Templates/Template_Default")
    except Exception as e:
        warn("new_level_from_template failed (%s)" % e)
    if not created:
        try:
            created = levels.new_level(ARENA)
        except Exception as e:
            warn("new_level failed (%s)" % e)
    if not created:
        warn("could not create %s; make an empty level there by hand and run again" % ARENA)
        return

    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

    # The template's small floor would z-fight with ours
    for actor in actors.get_all_level_actors():
        if isinstance(actor, unreal.StaticMeshActor) and "floor" in actor.get_actor_label().lower():
            actors.destroy_actor(actor)

    floor = _spawn(actors, unreal.StaticMeshActor, (0.0, 0.0, -50.0), label="ArenaFloor")
    cube = load("/Engine/BasicShapes/Cube")
    if floor is not None and cube is not None:
        floor.static_mesh_component.set_static_mesh(cube)
        floor.set_actor_scale3d(unreal.Vector(120.0, 120.0, 1.0))
        grid = load("/Engine/BasicShapes/BasicShapeMaterial")
        if grid is not None:
            floor.static_mesh_component.set_material(0, grid)

    # A few rocks to break line of sight around the camps
    for index, (x, y) in enumerate([(-2000.0, 2200.0), (-3000.0, 300.0), (2000.0, 2600.0), (3100.0, 700.0), (0.0, 1200.0)]):
        rock = _spawn(actors, unreal.StaticMeshActor, (x, y, 150.0), yaw=index * 37.0, label="Cover_%d" % index)
        if rock is not None and cube is not None:
            rock.static_mesh_component.set_static_mesh(cube)
            rock.set_actor_scale3d(unreal.Vector(2.5, 1.2, 3.0))

    start = None
    for actor in actors.get_all_level_actors():
        if isinstance(actor, unreal.PlayerStart):
            start = actor
            break
    if start is None:
        start = _spawn(actors, unreal.PlayerStart, (0.0, -3800.0, 120.0), yaw=90.0, label="PlayerStart")
    else:
        start.set_actor_location_and_rotation(unreal.Vector(0.0, -3800.0, 120.0), unreal.Rotator(roll=0.0, pitch=0.0, yaw=90.0),
                                              False, False)

    jiwoong = bp_class(JIWOONG)
    if jiwoong is not None:
        _spawn(actors, jiwoong, (250.0, -3900.0, 120.0), yaw=90.0, label="Ji-Woong")
    else:
        warn("BP_Ji-Woong not found; the arena has only Angel")
    _spawn(actors, unreal.BeyondCheckpoint, (0.0, -3600.0, 60.0), yaw=90.0, label="Checkpoint_Start")

    _spawner(actors, "Camp_Forest", (-2600.0, 1200.0, 60.0),
             [(enemies_by_id.get("forest_wolf"), 3, 2), (enemies_by_id.get("forest_raider"), 2, 2),
              (enemies_by_id.get("forest_slinger"), 1, 2), (enemies_by_id.get("forest_treant"), 1, 3)], 0.15)
    _spawner(actors, "Camp_CorruptedWoods", (2600.0, 1600.0, 60.0),
             [(enemies_by_id.get("woods_wraith"), 2, 8), (enemies_by_id.get("woods_knight"), 1, 9),
              (enemies_by_id.get("woods_beast"), 2, 8)], 0.25)

    bounds = unreal.BeyondEditorLibrary.create_nav_mesh_bounds(world, unreal.Vector(0.0, 0.0, 0.0), unreal.Vector(6000.0, 6000.0, 1200.0))
    if bounds is None:
        warn("could not place the Nav Mesh Bounds Volume")
    if unreal.BeyondEditorLibrary.build_navigation(world):
        log("built the navmesh")
    else:
        warn("the navmesh wasn't built: open %s and use Build -> Build Paths" % ARENA)

    if levels.save_current_level():
        log("created %s" % ARENA)
    else:
        warn("could not save %s" % ARENA)


# ---------------------------------------------------------------- main

def main():
    log("pass 10")
    missing = [name for name in ("BeyondEnemyDefinition", "BeyondAffixDefinition", "BeyondGA_AreaAttack", "BeyondEnemyRoster",
                                 "BeyondProjectile", "BeyondEnemySpawner", "BeyondEditorLibrary", "BeyondStrikeSettings")
               if not hasattr(unreal, name)]
    if missing:
        warn("the editor is running an old build of the WorldBeyond module (%s missing). Close the editor, build "
             "WorldBeyondEditor (Development Editor, Win64) until it says Build succeeded, then run this script again. "
             "Nothing was changed." % ", ".join(missing))
        write_report("last_run_pass10.txt")
        raise RuntimeError("WorldBeyond C++ not rebuilt: %s missing" % ", ".join(missing))

    step_materials()
    sets = step_abilities()
    affixes = step_affixes(sets)
    enemies = step_enemies(sets)
    step_roster(enemies, affixes)
    by_id = {}
    for enemy in enemies:
        by_id[str(enemy.get_editor_property("enemy_id"))] = enemy
    step_arena(by_id)
    write_report("last_run_pass10.txt")


main()
