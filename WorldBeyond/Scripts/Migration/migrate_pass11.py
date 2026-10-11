"""
Worlds Beyond - pass 11: bosses (Plan 3B).

Run with the editor closed, after building the C++ and running pass 10:
    UnrealEditor-Cmd.exe <path>/WorldBeyond.uproject -run=pythonscript -script=<path>/Scripts/Migration/migrate_pass11.py -unattended -nosplash -NullRHI

- The four Paragon hero Blueprints (SevarogPlayerCharacter, RampagePlayerCharacter, KhaimeraPlayerCharacter,
  PhasePlayerCharacter) are reparented from Character to ABeyondBossCharacter: their anim Blueprints cast to them, so the
  bosses are children of them (BP_Boss_* in /Game/WorldsBeyond/Enemies/Bosses/). Their own camera is removed at runtime.
- Boss montages are made on the heroes' FullBody slot (their anim Blueprints have no DefaultSlot).
- Boss abilities, phase ability sets and four boss definitions (UBeyondBossDefinition, added to DA_EnemyRoster):
    Kael'thar, the Molten Colossus (Sevarog x2.5)  - phases 100 / 70 / 40 %, the arena burns at 40 %
    Hrimgar, the Frost Troll King (Rampage Elemental) - 100 / 65 / 30 %, summons, then enrage
    Gorehide, the Pale Alpha (Khaimera Grux-pelt, mini-boss) - 100 / 50 %, howls in a wolf pack
    Veyla, the Hollow Voice (Phase, mini-boss)    - 100 / 60 / 25 %, shadow clones, then darkness
- Four signature weapons (Emberheart, Glacierheart Rod, Gruxfang, Hollow Voice Scepter): each boss's Guaranteed Loot.
- Four boss arenas in /Game/WorldsBeyond/Maps/TestArena (each with a checkpoint at its entrance).

Data is only created when missing (BEYOND_REBUILD_BOSSES=1 rebuilds it); arenas are only added when the map has none.
Every asset saved is copied to Saved/MigrationBackups/<timestamp>/ first. Report: last_run_pass11.txt.
"""
import os
import sys

import unreal

sys.path.insert(0, os.path.join(os.path.abspath(unreal.Paths.project_dir()), "Scripts", "Migration"))
from migration_common import (ASSET_TOOLS, EAL, bp_class, cdo, ensure_blueprint, ensure_montage, fx, load, log,  # noqa: E402
                              reparent, save, set_props, tag, tag_container, warn, write_report)

ROOT = "/Game/WorldsBeyond/Enemies"
BOSSES = ROOT + "/Bosses"
MONTAGES = ROOT + "/Montages"
ABILITIES = ROOT + "/Abilities"
SETS = ROOT + "/AbilitySets"
PROJECTILES = ROOT + "/Projectiles"
ROSTER_ASSET = ROOT + "/DA_EnemyRoster"
ITEMS = "/Game/WorldsBeyond/Items"
WEAPONS = ITEMS + "/Weapons"
DATABASE = ITEMS + "/DA_ItemDatabase"
ARENA = "/Game/WorldsBeyond/Maps/TestArena"

SEV = "/Game/ParagonSevarog/Characters/Heroes/Sevarog/"
RAM = "/Game/ParagonRampage/Characters/Heroes/Rampage/"
KHA = "/Game/ParagonKhaimera/Characters/Heroes/Khaimera/"
PHA = "/Game/ParagonPhase/Characters/Heroes/Phase/"
SEV_FX = "/Game/ParagonSevarog/FX/Particles/Abilities/"
RAM_ICE = "/Game/ParagonRampage/FX/Particles/Rampage_v001_IceBlue/FX/"
KHA_FX = "/Game/ParagonKhaimera/FX/ParticleSystems/Abilities/"
PHA_FX = "/Game/ParagonPhase/FX/Particles/Abilities/"
GIDEON = "/Game/ParagonGideon/FX/"
SERATH_BURN = "/Game/ParagonSerath/FX/Particles/Abilities/Ultimate/FX/P_Burning"

REBUILD = os.environ.get("BEYOND_REBUILD_BOSSES", "") == "1"

SHAPE = unreal.BeyondStrikeShape
AIM = unreal.BeyondStrikeAim
TWIST = unreal.BeyondTwistType
PLACE = unreal.BeyondHazardPlacement

LIGHT = "Event.Hit.Light"
STAGGER = "Event.Hit.Stagger"
STUN = "Event.Hit.Stun"
KNOCK = "Event.Hit.KnockBack"
FULL_BODY = "FullBody"


# ---------------------------------------------------------------- helpers (as pass 10)

def timed_fx(lifetime, **kwargs):
    value = fx(**kwargs)
    value.set_editor_property("max_lifetime", float(lifetime))
    return value


def struct(cls, **props):
    value = cls()
    for key, item in props.items():
        value.set_editor_property(key, item)
    return value


def growth(max_health=0.0, strength=0.0, arcana=0.0, defense=0.0):
    return struct(unreal.BeyondStatGrowth, max_health=float(max_health), max_stamina=0.0, strength=float(strength),
                  arcana=float(arcana), defense=float(defense))


def strike(**props):
    for key in ("damage_type", "hit_response", "linger_damage_type"):
        if isinstance(props.get(key), str):
            props[key] = tag(props[key])
    if "color" in props and isinstance(props["color"], tuple):
        props["color"] = unreal.LinearColor(*props["color"])
    if isinstance(props.get("falling_mesh"), str):
        props["falling_mesh"] = load(props["falling_mesh"])
    for key in ("radius", "inner_radius", "cone_angle", "length", "width", "wind_up", "damage", "linger_duration",
                "linger_damage_per_second", "falling_mesh_scale", "fall_height"):
        if key in props:
            props[key] = float(props[key])
    return struct(unreal.BeyondStrikeSettings, **props)


def data_asset(folder, name, cls, rebuild=REBUILD):
    path = "%s/%s" % (folder, name)
    exists = EAL.does_asset_exist(path)
    if exists and not rebuild:
        return load(path), False
    asset = load(path) if exists else ASSET_TOOLS.create_asset(name, folder, cls, unreal.DataAssetFactory())
    if asset is None:
        warn("could not create %s" % path)
        return None, False
    return asset, True


_montages = {}


def montage(group, sequence, hold=False, existing=None):
    """A FullBody montage of a hero sequence (or the pack's own montage when it has one)."""
    if existing and EAL.does_asset_exist(existing):
        return load(existing)
    key = (group, sequence)
    if key in _montages:
        return _montages[key]
    name = sequence.rsplit("/", 1)[1]
    path = "%s/%s/AM_%s_%s" % (MONTAGES, group, group, name)
    created = not EAL.does_asset_exist(path)
    result = ensure_montage(sequence, path)
    if result is not None and (created or REBUILD):
        if not unreal.BeyondEditorLibrary.set_montage_slot(result, FULL_BODY):
            warn("%s: could not move it to the %s slot" % (path, FULL_BODY))
        if hold:
            result.set_editor_property("enable_auto_blend_out", False)
        save(result, path)
    _montages[key] = result
    return result


def combo_step(anim, damage, response=LIGHT, rate=1.0):
    return struct(unreal.BeyondComboStep, montage=anim, damage=float(damage), hit_response=tag(response), play_rate=float(rate))


def ability(folder, name, parent, **props):
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


def melee(folder, name, steps, reach, radius, **props):
    return ability(folder, name, unreal.BeyondGA_MeleeCombo, combo_steps=steps, unarmed_reach=float(reach),
                   unarmed_radius=float(radius), legs_follow_movement=False, stop_if_combo_window_missed=False,
                   ai_chains_combo=True, ai_attack_token_cost=0, **props)


def area(folder, name, anim, aim, settings, **props):
    return ability(folder, name, unreal.BeyondGA_AreaAttack, montage=anim, aim=aim, strike=settings, ai_attack_token_cost=0, **props)


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


def projectile(name, trail, impact, speed=2200.0, radius=24.0):
    path = "%s/%s" % (PROJECTILES, name)
    exists = EAL.does_asset_exist(path)
    bp = ensure_blueprint(path, unreal.BeyondProjectile)
    if bp is None:
        return None
    if not exists or REBUILD:
        set_props(bp, path, speed=float(speed), collision_radius=float(radius), trail_fx=trail, impact_fx=impact)
    return bp_class(path)


def twist(kind, announcement="", repeat=0.0, **props):
    value = unreal.BeyondBossTwist()
    value.set_editor_property("type", kind)
    value.set_editor_property("announcement", unreal.Text(announcement))
    value.set_editor_property("repeat_interval", float(repeat))
    if "summon_ids" in props:
        props["summon_ids"] = [unreal.Name(i) for i in props["summon_ids"]]
    if "enrage_tint" in props:
        props["enrage_tint"] = unreal.LinearColor(*props["enrage_tint"])
    for key, item in props.items():
        if isinstance(item, int) and not isinstance(item, bool) and key not in ("hazard_count", "summon_count", "max_alive", "clone_count"):
            item = float(item)
        value.set_editor_property(key, item)
    return value


def phase(name, threshold, transition=None, duration=2.5, transition_fx=None, abilities=None, twists=()):
    value = unreal.BeyondBossPhase()
    value.set_editor_property("name", unreal.Text(name))
    value.set_editor_property("health_threshold", float(threshold))
    value.set_editor_property("transition_montage", transition)
    value.set_editor_property("transition_duration", float(duration))
    if transition_fx is not None:
        value.set_editor_property("transition_fx", transition_fx)
    value.set_editor_property("abilities", abilities)
    value.set_editor_property("twists", list(twists))
    return value


def tag_map(entries, value_type):
    result = unreal.Map(unreal.GameplayTag, value_type)
    for key, value in entries.items():
        if value is not None:
            result[tag(key)] = value
    return result


# ---------------------------------------------------------------- hero Blueprints

def step_heroes():
    """Reparent the hero Blueprints and make the BP_Boss_* children; returns {key: boss class}."""
    heroes = {
        "kaelthar": (SEV + "SevarogPlayerCharacter", BOSSES + "/BP_Boss_Kaelthar"),
        "hrimgar": (RAM + "RampagePlayerCharacter", BOSSES + "/BP_Boss_Hrimgar"),
        "gorehide": (KHA + "KhaimeraPlayerCharacter", BOSSES + "/BP_Boss_Gorehide"),
        "veyla": (PHA + "PhasePlayerCharacter", BOSSES + "/BP_Boss_Veyla"),
    }
    classes = {}
    for key, (hero, boss) in heroes.items():
        bp = reparent(hero, unreal.BeyondBossCharacter)
        if bp is None:
            warn("%s not found: %s has no Blueprint" % (hero, key))
            continue
        set_props(bp, hero, ai_controller_class=unreal.BeyondEnemyController.static_class(),
                  auto_possess_ai=unreal.AutoPossessAI.PLACED_IN_WORLD_OR_SPAWNED,
                  auto_possess_player=unreal.AutoReceiveInput.DISABLED,
                  team_affiliation=unreal.BeyondTeam.ENEMY)
        hero_class = bp_class(hero)
        child = ensure_blueprint(boss, hero_class)
        if child is None:
            warn("could not create %s" % boss)
            continue
        save(child, boss)
        classes[key] = bp_class(boss)
        log("%s -> %s (child of %s)" % (key, boss, hero))
    return classes


# ---------------------------------------------------------------- boss abilities

def step_abilities():
    sets = {}

    # Kael'thar: hammer swings, a huge cone, then soul siphon and comet rain, then a knock-back nova
    cleave = melee("Kaelthar", "GA_Kaelthar_Cleave", [
        combo_step(montage("Kaelthar", SEV + "Animations/Swing1_Medium", existing=SEV + "Animations/Swing1_Medium_Montage"), 40),
        combo_step(montage("Kaelthar", SEV + "Animations/Swing2_Medium", existing=SEV + "Animations/Swing2_Medium_Montage"), 45),
        combo_step(montage("Kaelthar", SEV + "Animations/Swing3_Medium"), 60, STAGGER)],
        reach=460.0, radius=210.0, ai_max_range=560, ai_weight=3)
    hammerfall = area("Kaelthar", "GA_Kaelthar_Hammerfall", montage("Kaelthar", SEV + "Animations/Ultimate_Swing_120fps"),
                      AIM.FROM_SELF_TOWARD_TARGET,
                      strike(shape=SHAPE.CONE, radius=1050, cone_angle=120, wind_up=1.4, damage=90, damage_type="DamageType.Melee",
                             hit_response=KNOCK, impact_fx=timed_fx(3, system=SEV_FX + "Ultimate/FX/P_UltMeshArc_Expanding", scale=2.0)),
                      ai_max_range=950, ai_weight=1.5, cooldown_duration=10, cooldown_tag="Cooldown.Enemy.Special")
    siphon = ability("Kaelthar", "GA_Kaelthar_SoulSiphon", unreal.BeyondGA_Beam,
                     montage=montage("Kaelthar", SEV + "Animations/Soul_Siphon"), wind_up=0.9, duration=3.0, turn_rate=30.0,
                     length=1500.0, width=230.0, damage_per_second=35.0, damage_type=tag("DamageType.Explosion"),
                     hit_response=tag(LIGHT), drain_fraction=0.5, lane_color=unreal.LinearColor(1.0, 0.45, 0.1, 1.0),
                     beam_fx=timed_fx(0, system=SEV_FX + "SoulSiphon/FX/P_SoulSwirls", scale=2.0),
                     hit_fx=timed_fx(1.5, system=SEV_FX + "SoulSiphon/FX/P_SiphonImpact"),
                     ai_min_range=400, ai_max_range=1400, ai_weight=1.5, cooldown_duration=14, cooldown_tag="Cooldown.Enemy.Beam")
    comets = area("Kaelthar", "GA_Kaelthar_CometRain", montage("Kaelthar", SEV + "Animations/Cast"), AIM.AT_TARGET,
                  strike(shape=SHAPE.CIRCLE, radius=260, wind_up=1.5, damage=55, damage_type="DamageType.Explosion",
                         hit_response=STAGGER, falling_mesh=GIDEON + "Meshes/Heroes/Gideon/Abilities/SM_MeteorChopped_Meteor_Chopped11",
                         falling_mesh_scale=1.2, fall_height=2200,
                         falling_fx=timed_fx(0, system=GIDEON + "Particles/Gideon/Abilities/Meteor/FX/P_Gideon_Meteor_Trail_RED"),
                         impact_fx=timed_fx(2.5, system=SEV_FX + "Primary/FX/P_SevarogComet", scale=1.5)),
                  count=6, interval=0.25, scatter=650.0, ai_max_range=2200, ai_weight=1.2, cooldown_duration=12,
                  cooldown_tag="Cooldown.Enemy.Ultimate")
    subjugate = area("Kaelthar", "GA_Kaelthar_Subjugate", montage("Kaelthar", SEV + "Animations/Subjugation"), AIM.AT_SELF,
                     strike(shape=SHAPE.CIRCLE, radius=650, wind_up=1.2, damage=70, damage_type="DamageType.Explosion",
                            hit_response=KNOCK, impact_fx=timed_fx(2.5, system=SEV_FX + "Subjugate/FX/P_Sevarog_Subjugate_Blast", scale=2.0)),
                     ai_max_range=620, ai_weight=1.5, cooldown_duration=9, cooldown_tag="Cooldown.Enemy.Buff")
    sets["kaelthar"] = ability_set("DA_AbilitySet_Boss_Kaelthar", [cleave, hammerfall])
    sets["kaelthar_2"] = ability_set("DA_AbilitySet_Boss_Kaelthar_Phase2", [siphon, comets])
    sets["kaelthar_3"] = ability_set("DA_AbilitySet_Boss_Kaelthar_Phase3", [subjugate])

    # Hrimgar: fists, boulders, ground smash waves; then avalanche charge and frost roar
    fists = melee("Hrimgar", "GA_Hrimgar_Fists", [
        combo_step(montage("Hrimgar", RAM + "Animations/Attack_Biped_Melee_A", existing=RAM + "Animations/Attack_Biped_Melee_A_Montage"), 35),
        combo_step(montage("Hrimgar", RAM + "Animations/Attack_Biped_Melee_B", existing=RAM + "Animations/Attack_Biped_Melee_B_Montage"), 35),
        combo_step(montage("Hrimgar", RAM + "Animations/Attack_Biped_Melee_C", existing=RAM + "Animations/Attack_Biped_Melee_C_Montage"), 50, STAGGER)],
        reach=310.0, radius=150.0, ai_max_range=380, ai_weight=3)
    boulder = area("Hrimgar", "GA_Hrimgar_Boulder", montage("Hrimgar", RAM + "Animations/Ability_RipNToss_Toss"), AIM.AT_TARGET,
                   strike(shape=SHAPE.CIRCLE, radius=300, wind_up=1.4, damage=60, damage_type="DamageType.Explosion",
                          hit_response=KNOCK, falling_mesh=RAM + "Meshes/Rocks/SM_Rock_To_Hold", falling_mesh_scale=1.6,
                          fall_height=1600, color=(0.35, 0.7, 1.0, 1.0),
                          falling_fx=timed_fx(0, system=RAM_ICE + "P_Rampage_Ice_Rock_Flying"),
                          impact_fx=timed_fx(2.5, system=RAM_ICE + "P_Rampage_Ice_Rock_HitWorld", scale=1.5)),
                   ai_min_range=450, ai_max_range=2000, ai_weight=1.5, cooldown_duration=8, cooldown_tag="Cooldown.Enemy.Special")
    smash = area("Hrimgar", "GA_Hrimgar_GroundSmash", montage("Hrimgar", RAM + "Animations/Ability_GroundSmash_Start"), AIM.AT_SELF,
                 strike(shape=SHAPE.RING, radius=450, inner_radius=0, wind_up=1.0, damage=55, damage_type="DamageType.Explosion",
                        hit_response=KNOCK, color=(0.35, 0.7, 1.0, 1.0),
                        impact_fx=timed_fx(2.5, system=RAM_ICE + "P_Rampage_Ice_SmashArc", scale=1.5)),
                 count=2, interval=0.6, radius_growth=420.0, ai_max_range=520, ai_weight=1.3, cooldown_duration=10,
                 cooldown_tag="Cooldown.Enemy.Ultimate")
    avalanche = ability("Hrimgar", "GA_Hrimgar_Avalanche", unreal.BeyondGA_Dash,
                        dash_distance=950.0, dash_duration=0.6, exit_speed=200.0, invincible_while_dashing=False,
                        pass_through_pawns=True, dash_montage=montage("Hrimgar", RAM + "Animations/Ability_RMB_Smash"),
                        path_damage=50.0, path_radius=200.0, detonate_delay=0.1, path_damage_type=tag("DamageType.Melee"),
                        path_hit_response=tag(KNOCK), trail_fx=timed_fx(1.0, system=RAM_ICE + "P_Rampage_Ice_Lunge_Jumpdust"),
                        ai_min_range=500, ai_max_range=1100, ai_weight=1.2, cooldown_duration=12, cooldown_tag="Cooldown.Enemy.Mobility")
    roar = area("Hrimgar", "GA_Hrimgar_FrostRoar", montage("Hrimgar", RAM + "Animations/Emote_Master_Roar_T3"),
                AIM.FROM_SELF_TOWARD_TARGET,
                strike(shape=SHAPE.CONE, radius=900, cone_angle=90, wind_up=1.0, damage=25, damage_type="DamageType.Explosion",
                       hit_response=STUN, color=(0.35, 0.7, 1.0, 1.0),
                       impact_fx=timed_fx(2.5, system=RAM_ICE + "P_Rampage_Roar_Radius_Ice", scale=1.5)),
                ai_max_range=820, ai_weight=1, cooldown_duration=15, cooldown_tag="Cooldown.Enemy.Buff")
    sets["hrimgar"] = ability_set("DA_AbilitySet_Boss_Hrimgar", [fists, boulder, smash])
    sets["hrimgar_2"] = ability_set("DA_AbilitySet_Boss_Hrimgar_Phase2", [avalanche, roar])

    # Gorehide: claws and pounce; then a slam
    claws = melee("Gorehide", "GA_Gorehide_Claws", [
        combo_step(montage("Gorehide", KHA + "Animations/Melee_A", existing=KHA + "Animations/Melee_A_Montage"), 25),
        combo_step(montage("Gorehide", KHA + "Animations/Melee_B", existing=KHA + "Animations/Melee_B_Montage"), 25),
        combo_step(montage("Gorehide", KHA + "Animations/Melee_C", existing=KHA + "Animations/Melee_C_Montage"), 35, STAGGER)],
        reach=240.0, radius=110.0, ai_max_range=290, ai_weight=3)
    pounce = ability("Gorehide", "GA_Gorehide_Pounce", unreal.BeyondGA_Leap,
                     montage=montage("Gorehide", KHA + "Animations/RMB_60fps"), air_time=0.7, take_off_delay=0.3,
                     landing=strike(shape=SHAPE.CIRCLE, radius=330, damage=40, damage_type="DamageType.Melee", hit_response=KNOCK,
                                    impact_fx=timed_fx(2, system=KHA_FX + "Leap/FX/P_Khaimera_Leap_AOE_Burst")),
                     ai_min_range=450, ai_max_range=1300, ai_weight=1.5, cooldown_duration=8, cooldown_tag="Cooldown.Enemy.Mobility")
    slam = area("Gorehide", "GA_Gorehide_Slam", montage("Gorehide", KHA + "Animations/R_Ability"), AIM.AT_SELF,
                strike(shape=SHAPE.CIRCLE, radius=500, wind_up=1.1, damage=45, damage_type="DamageType.Melee", hit_response=KNOCK,
                       impact_fx=timed_fx(2.5, system=KHA_FX + "Ultimate/FX/P_Khaimera_Ult_Blast")),
                ai_max_range=460, ai_weight=1.2, cooldown_duration=10, cooldown_tag="Cooldown.Enemy.Ultimate")
    sets["gorehide"] = ability_set("DA_AbilitySet_Boss_Gorehide", [claws, pounce])
    sets["gorehide_2"] = ability_set("DA_AbilitySet_Boss_Gorehide_Phase2", [slam])

    # Veyla: void bolts, a blink when crowded, the severing beam; then the soul-link pull and a dark nova
    bolt_class = projectile("BP_Beyond_VeylaBolt",
                            trail=timed_fx(0, system=PHA_FX + "Primary/FX/P_PhaseProjectileRibbons"),
                            impact=timed_fx(2, system=PHA_FX + "Primary/FX/P_PhasePrimaryImpact"), speed=2200.0)
    bolt = ability("Veyla", "GA_Veyla_VoidBolt", unreal.BeyondGA_Projectile,
                   projectile_class=bolt_class,
                   cast_montage=montage("Veyla", PHA + "Animations/Primary_Attack_A_Medium", existing=PHA + "Animations/Primary_Attack_A_Medium_Montage"),
                   damage=22.0, hit_response=tag(LIGHT), projectile_speed=2200.0, max_range=2600.0, fallback_fire_delay=0.3,
                   ai_min_range=200, ai_max_range=1600, ai_weight=3, cooldown_duration=1.4, cooldown_tag="Cooldown.Enemy.Primary")
    blink = ability("Veyla", "GA_Veyla_Blink", unreal.BeyondGA_Teleport,
                    mode=unreal.BeyondTeleportMode.AWAY_FROM_TARGET, distance=800.0,
                    depart_fx=timed_fx(1.5, system=PHA_FX + "Flash/FX/P_PhaseFlash"),
                    arrive_fx=timed_fx(1.5, system=PHA_FX + "Flash/FX/P_FlashImpact"),
                    ai_max_range=350, ai_weight=3, cooldown_duration=6, cooldown_tag="Cooldown.Enemy.Mobility")
    beam = ability("Veyla", "GA_Veyla_SeveringBeam", unreal.BeyondGA_Beam,
                   montage=montage("Veyla", PHA + "Animations/R_Ability_Loop"), wind_up=0.9, duration=3.0, turn_rate=35.0,
                   length=1500.0, width=160.0, damage_per_second=30.0, damage_type=tag("DamageType.Explosion"),
                   hit_response=tag(LIGHT), beam_fx=timed_fx(0, system=PHA_FX + "Beam/FX/P_PhaseBeamGlow"),
                   hit_fx=timed_fx(1.5, system=PHA_FX + "Beam/FX/P_BeamImpactDamage"),
                   ai_min_range=350, ai_max_range=1400, ai_weight=1.2, cooldown_duration=12, cooldown_tag="Cooldown.Enemy.Beam")
    pull = ability("Veyla", "GA_Veyla_SoulLink", unreal.BeyondGA_Pull,
                   montage=montage("Veyla", PHA + "Animations/RMB_Pull"),
                   line=strike(shape=SHAPE.LINE, length=1300, width=150, wind_up=0.9, damage=15, damage_type="DamageType.Explosion",
                               hit_response=LIGHT, color=(0.6, 0.2, 1.0, 1.0)),
                   pull_fx=timed_fx(1.5, system=PHA_FX + "Link/FX/P_LinkPull"),
                   ai_min_range=450, ai_max_range=1250, ai_weight=1.3, cooldown_duration=10, cooldown_tag="Cooldown.Enemy.Special")
    nova = area("Veyla", "GA_Veyla_DarkNova", montage("Veyla", PHA + "Animations/Ability_Q"), AIM.AT_SELF,
                strike(shape=SHAPE.CIRCLE, radius=550, wind_up=1.0, damage=35, damage_type="DamageType.Explosion",
                       hit_response=STAGGER, color=(0.6, 0.2, 1.0, 1.0), impact_fx=timed_fx(2.5, system=PHA_FX + "Ultimate/FX/P_PhaseUlt")),
                ai_max_range=520, ai_weight=1.5, cooldown_duration=9, cooldown_tag="Cooldown.Enemy.Ultimate")
    sets["veyla"] = ability_set("DA_AbilitySet_Boss_Veyla", [bolt, blink, beam])
    sets["veyla_2"] = ability_set("DA_AbilitySet_Boss_Veyla_Phase2", [pull, nova])
    return sets


# ---------------------------------------------------------------- signature weapons

def make_weapon(name, title, description, stats, weapon_tag):
    path = "%s/%s" % (WEAPONS, name)
    item, fill = data_asset(WEAPONS, name, unreal.BeyondItemDefinition)
    if item is None or not fill:
        return item
    values = []
    names = {"max_health": unreal.BeyondSkillStat.MAX_HEALTH, "strength": unreal.BeyondSkillStat.STRENGTH,
             "arcana": unreal.BeyondSkillStat.ARCANA, "defense": unreal.BeyondSkillStat.DEFENSE,
             "max_stamina": unreal.BeyondSkillStat.MAX_STAMINA}
    for stat, value in stats.items():
        values.append(struct(unreal.BeyondItemStat, stat=names[stat], value=float(value)))
    item.set_editor_property("display_name", unreal.Text(title))
    item.set_editor_property("description", unreal.Text(description))
    item.set_editor_property("slot", unreal.BeyondItemSlot.WEAPON)
    item.set_editor_property("base_stats", values)
    item.set_editor_property("min_tier", unreal.BeyondItemTier.EPIC)
    item.set_editor_property("max_tier", unreal.BeyondItemTier.LEGENDARY)
    # Never in random drops: only its boss has it
    item.set_editor_property("drop_weight", 0.0)
    item.set_editor_property("weapon_tag", tag(weapon_tag))
    save(item, path)
    log("built %s" % path)
    return item


def step_weapons():
    sword, staff = "Weapon.Melee.Sword", "Weapon.Ranged.Staff"
    weapons = {
        "kaelthar": make_weapon("DA_Item_Emberheart", "Emberheart", "Forged in the Colossus's chest; it never cools.",
                                {"strength": 12, "max_health": 25}, sword),
        "hrimgar": make_weapon("DA_Item_GlacierheartRod", "Glacierheart Rod", "A shard of the Troll King's crown, humming with frost.",
                               {"arcana": 12, "defense": 4}, staff),
        "gorehide": make_weapon("DA_Item_Gruxfang", "Gruxfang", "A fang longer than a forearm, honed into a blade.",
                                {"strength": 8, "max_stamina": 10}, sword),
        "veyla": make_weapon("DA_Item_HollowVoiceScepter", "Hollow Voice Scepter", "It whispers in a voice you almost know.",
                             {"arcana": 9, "max_health": 15}, staff),
    }
    database = load(DATABASE)
    if database is not None:
        listed = [i for i in database.get_editor_property("items") if i is not None]
        names = {i.get_path_name() for i in listed}
        added = [w for w in weapons.values() if w is not None and w.get_path_name() not in names]
        if added:
            database.set_editor_property("items", listed + added)
            save(database, DATABASE)
            log("%s: added %d signature weapons" % (DATABASE, len(added)))
    return weapons


# ---------------------------------------------------------------- boss definitions

def make_boss(name, boss_class, **props):
    path = "%s/%s" % (BOSSES, name)
    boss, fill = data_asset(BOSSES, name, unreal.BeyondBossDefinition)
    if boss is None or not fill:
        if boss is not None:
            log("kept existing %s (BEYOND_REBUILD_BOSSES=1 rebuilds it)" % path)
        return boss
    for key in ("enemy_id", "boss_id"):
        props[key] = unreal.Name(props[key])
    for key in ("display_name", "title", "description"):
        props[key] = unreal.Text(props.get(key, ""))
    if isinstance(props.get("region"), str):
        props["region"] = tag(props["region"])
    props["tint"] = unreal.LinearColor(*props.get("tint", (0.0, 0.0, 0.0, 0.0)))
    if isinstance(props.get("mesh"), str):
        props["mesh"] = load(props["mesh"])
    props["hit_reactions"] = tag_map(props.pop("hit", {}), unreal.AnimMontage)
    props["death_montages"] = [m for m in props.pop("deaths", []) if m is not None]
    ai = props.pop("ai")
    props["ai"] = struct(unreal.BeyondEnemyAIConfig, **{k: (float(v) if isinstance(v, int) and not isinstance(v, bool) else v)
                                                       for k, v in ai.items()})
    props["guaranteed_loot"] = [i for i in props.get("guaranteed_loot", []) if i is not None]
    for key in ("max_health", "strength", "arcana", "defense", "walk_speed", "scale", "bar_show_radius"):
        if key in props:
            props[key] = float(props[key])
    props["uninterruptible"] = True
    props["can_be_elite"] = False
    for key, value in props.items():
        try:
            boss.set_editor_property(key, value)
        except Exception as e:
            warn("%s: could not set %s (%s)" % (path, key, e))
    if boss_class is not None:
        try:
            boss.set_editor_property("character_class", boss_class)
        except Exception:
            try:
                boss.set_editor_property("character_class", unreal.SoftClassPath(boss_class.get_path_name()))
            except Exception as e:
                warn("%s: could not set the character class (%s)" % (path, e))
    save(boss, path)
    log("built %s" % path)
    return boss


def step_bosses(classes, sets, weapons):
    boss_ai = dict(sight_radius=4000, sight_half_angle=180, close_sense_radius=900, alert_radius=0, leash_radius=9000,
                   wander_radius=0, strafe_radius=600, attack_recovery=0.6, uses_attack_tokens=False)
    caster_ai = dict(boss_ai, preferred_range=900, keep_away_distance=600)
    bosses = {}
    bosses["kaelthar"] = make_boss(
        "DA_Boss_Kaelthar", classes.get("kaelthar"), enemy_id="boss_kaelthar", boss_id="kaelthar",
        display_name="Kael'thar", title="the Molten Colossus", region="Region.Molten",
        description="A demon of slag and soul-fire, three times a man's height.", rank=unreal.BeyondEnemyRank.BOSS,
        story_boss=True, mesh=SEV + "Skins/Tier_1/Sevarog_Red/Meshes/SevarogBloodred", scale=2.5,
        tint=(1.0, 0.35, 0.05, 0.35), max_health=3000, strength=20, arcana=15, defense=30, walk_speed=430,
        growth=growth(max_health=300, strength=3, arcana=3, defense=2), ability_set=sets.get("kaelthar"),
        deaths=[montage("Kaelthar", SEV + "Animations/Death_front", hold=True)],
        spawn_montage=load(SEV + "Animations/LevelStart_Montage"), ai=boss_ai, bar_show_radius=3200,
        guaranteed_loot=[weapons.get("kaelthar")],
        phases=[
            phase("The Colossus Wakes", 1.0, duration=0),
            phase("Soul Furnace", 0.7, montage("Kaelthar", SEV + "Animations/Speedburst"), 2.5,
                  timed_fx(2.5, system=SEV_FX + "Ultimate/FX/P_UltActivate", scale=2.0), sets.get("kaelthar_2")),
            phase("The Ground Burns", 0.4, montage("Kaelthar", SEV + "Animations/Victory_Emote"), 3.0,
                  timed_fx(3, system=SEV_FX + "Subjugate/FX/P_Sevarog_Subjugate_Blast", scale=2.5), sets.get("kaelthar_3"), [
                twist(TWIST.HAZARDS, "The ground burns! Stay near the centre.",
                      hazard=strike(shape=SHAPE.RING, radius=1600, inner_radius=950, wind_up=2.0, damage=0, linger_duration=900,
                                    linger_damage_per_second=30, linger_damage_type="DamageType.Proc.Burn",
                                    color=(1.0, 0.4, 0.05, 1.0)),
                      hazard_count=1, placement=PLACE.ARENA_CENTRE),
                twist(TWIST.HAZARDS, "", 12.0,
                      hazard=strike(shape=SHAPE.CIRCLE, radius=280, wind_up=1.6, damage=40, damage_type="DamageType.Explosion",
                                    hit_response=STAGGER, linger_duration=6, linger_damage_per_second=20,
                                    linger_damage_type="DamageType.Proc.Burn",
                                    falling_mesh=GIDEON + "Meshes/Heroes/Gideon/Abilities/SM_MeteorChopped_Meteor_Chopped11",
                                    falling_fx=timed_fx(0, system=GIDEON + "Particles/Gideon/Abilities/Meteor/FX/P_Gideon_Meteor_Trail_RED"),
                                    impact_fx=timed_fx(2.5, system=SEV_FX + "Primary/FX/P_SevarogComet", scale=1.5),
                                    linger_fx=timed_fx(0, system=SERATH_BURN, scale=1.0)),
                      hazard_count=3, placement=PLACE.AT_TARGETS, spread=350)]),
        ])
    bosses["hrimgar"] = make_boss(
        "DA_Boss_Hrimgar", classes.get("hrimgar"), enemy_id="boss_hrimgar", boss_id="hrimgar",
        display_name="Hrimgar", title="the Frost Troll King", region="Region.Frost",
        description="The troll king of the snowbound peaks; he throws mountains.", rank=unreal.BeyondEnemyRank.BOSS,
        story_boss=True, mesh=RAM + "Skins/Tier2/Elemental/Meshes/Rampage_Elemental", scale=1.7,
        tint=(0.4, 0.7, 1.0, 0.3), max_health=2600, strength=18, defense=35, walk_speed=430,
        growth=growth(max_health=280, strength=3, defense=3), ability_set=sets.get("hrimgar"),
        deaths=[montage("Hrimgar", RAM + "Animations/Death_A", hold=True)],
        spawn_montage=load(RAM + "Animations/LevelStart_Montage"), ai=boss_ai, bar_show_radius=3200,
        guaranteed_loot=[weapons.get("hrimgar")],
        phases=[
            phase("King of the Peaks", 1.0, duration=0),
            phase("The Mountain Calls", 0.65, montage("Hrimgar", RAM + "Animations/Emote_Master_Roar_T3"), 2.5,
                  timed_fx(2.5, system=RAM_ICE + "P_Rampage_Roar_Radius_Ice", scale=2.0), sets.get("hrimgar_2"), [
                twist(TWIST.SUMMON, "Hrimgar calls the pack!", summon_ids=["forest_wolf", "forest_raider"], summon_count=3,
                      max_alive=4)]),
            phase("Frozen Fury", 0.3, montage("Hrimgar", RAM + "Animations/Ability_Enrage_Start"), 2.0,
                  timed_fx(2, system=RAM_ICE + "P_Rampage_Ice_Enrage_Cast", scale=1.5), None, [
                twist(TWIST.ENRAGE, "Hrimgar is enraged!", enrage_damage_bonus=40, enrage_speed_multiplier=1.3,
                      enrage_tint=(0.3, 0.6, 1.0, 0.7), enrage_fx=timed_fx(0, system=RAM_ICE + "P_Rampage_Ice_Enrage_Looping")),
                twist(TWIST.SUMMON, "", 25.0, summon_ids=["forest_wolf"], summon_count=2, max_alive=4)]),
        ])
    bosses["gorehide"] = make_boss(
        "DA_Boss_Gorehide", classes.get("gorehide"), enemy_id="boss_gorehide", boss_id="gorehide",
        display_name="Gorehide", title="the Pale Alpha", region="Region.Forest",
        description="The beast the wolves follow, wearing the hide of the last thing that challenged it.",
        rank=unreal.BeyondEnemyRank.MINI_BOSS, story_boss=False,
        mesh=KHA + "Skins/Tier2/GruxPelt/Meshes/Khaimera_GruxPelt", scale=1.3, max_health=1400, strength=12, defense=10,
        walk_speed=480, growth=growth(max_health=120, strength=2, defense=1), ability_set=sets.get("gorehide"),
        deaths=[montage("Gorehide", KHA + "Animations/Death_A", hold=True), montage("Gorehide", KHA + "Animations/Death_B", hold=True)],
        spawn_montage=load(KHA + "Animations/LevelStart_Montage"), ai=boss_ai, bar_show_radius=2500,
        guaranteed_loot=[weapons.get("gorehide")],
        phases=[
            phase("The Pale Alpha", 1.0, duration=0),
            phase("Blood Howl", 0.5, montage("Gorehide", KHA + "Animations/Emote_Taunt_Howl_T1"), 2.0,
                  timed_fx(2, system=KHA_FX + "ThreeStrikeBuff/FX/P_Khaimera_BeastActivate"), sets.get("gorehide_2"), [
                twist(TWIST.SUMMON, "Gorehide howls - the pack shields him!", summon_ids=["forest_wolf"], summon_count=3,
                      max_alive=3, damage_taken_while_summons_live=0.5),
                twist(TWIST.ENRAGE, "", enrage_damage_bonus=20, enrage_speed_multiplier=1.2, enrage_tint=(0.9, 0.1, 0.05, 0.6),
                      enrage_fx=timed_fx(0, system=KHA_FX + "ThreeStrikeBuff/FX/P_Khaimera_BeastActive_Looping"))]),
        ])
    bosses["veyla"] = make_boss(
        "DA_Boss_Veyla", classes.get("veyla"), enemy_id="boss_veyla", boss_id="veyla",
        display_name="Veyla", title="the Hollow Voice", region="Region.CorruptedWoods",
        description="The voice of the cult in the corrupted woods; she is never only one.",
        rank=unreal.BeyondEnemyRank.MINI_BOSS, story_boss=False, scale=1.1, tint=(0.45, 0.1, 0.8, 0.4),
        max_health=1100, arcana=18, defense=5, walk_speed=400, growth=growth(max_health=110, arcana=3, defense=1),
        ability_set=sets.get("veyla"), deaths=[montage("Veyla", PHA + "Animations/Death", hold=True)],
        spawn_montage=load(PHA + "Animations/LevelStart_Montage"), ai=caster_ai, bar_show_radius=2500,
        guaranteed_loot=[weapons.get("veyla")],
        phases=[
            phase("The Hollow Voice", 1.0, duration=0),
            phase("Many Voices", 0.6, montage("Veyla", PHA + "Animations/Ability_R_Alt"), 2.0,
                  timed_fx(2, system=PHA_FX + "Link/FX/P_PhaseLinkActivate"), sets.get("veyla_2"), [
                twist(TWIST.SHADOW_CLONES, "Veyla splits into shadows!", clone_count=2, clone_health_fraction=0.1,
                      clone_damage_fraction=0.3)]),
            phase("The Dark Hour", 0.25, montage("Veyla", PHA + "Animations/Cast"), 1.5, None, None, [
                twist(TWIST.DARKNESS, "The light fails...", darkness=0.65),
                twist(TWIST.SHADOW_CLONES, "", 25.0, clone_count=2, clone_health_fraction=0.1, clone_damage_fraction=0.3)]),
        ])
    return {k: v for k, v in bosses.items() if v is not None}


def step_roster(bosses):
    roster = load(ROSTER_ASSET)
    if roster is None:
        warn("%s missing: run migrate_pass10.py first" % ROSTER_ASSET)
        return
    enemies = [e for e in roster.get_editor_property("enemies") if e is not None]
    names = {e.get_path_name() for e in enemies}
    added = [b for b in bosses.values() if b.get_path_name() not in names]
    if added:
        roster.set_editor_property("enemies", enemies + added)
        save(roster, ROSTER_ASSET)
    log("%s: %d bosses listed (%d added)" % (ROSTER_ASSET, len(bosses), len(added)))


# ---------------------------------------------------------------- arenas in the test map

def step_arenas(bosses):
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not EAL.does_asset_exist(ARENA) or not levels.load_level(ARENA):
        warn("%s missing: run migrate_pass10.py first" % ARENA)
        return
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    existing = [a for a in actors.get_all_level_actors() if isinstance(a, unreal.BeyondBossArena)]
    if existing:
        log("%s already has %d boss arenas" % (ARENA, len(existing)))
        return

    burning = timed_fx(0, system=SERATH_BURN, scale=1.6)
    frost = timed_fx(0, system=RAM_ICE + "P_Rampage_Ice_Enrage_Looping", scale=1.2)
    layout = [
        # key, centre, radius, level, seal FX, entrance (checkpoint) direction
        ("gorehide", (-4300.0, -2800.0), 1300.0, 5, None),
        ("veyla", (4300.0, -2800.0), 1300.0, 9, None),
        ("kaelthar", (-3300.0, 4300.0), 1600.0, 15, burning),
        ("hrimgar", (3300.0, 4300.0), 1600.0, 12, frost),
    ]
    for key, (x, y), radius, level, seal in layout:
        boss = bosses.get(key)
        if boss is None:
            warn("no boss definition for %s; arena skipped" % key)
            continue
        arena = actors.spawn_actor_from_class(unreal.BeyondBossArena, unreal.Vector(x, y, 20.0), unreal.Rotator(roll=0.0, pitch=0.0, yaw=0.0))
        if arena is None:
            warn("could not place the %s arena" % key)
            continue
        arena.set_actor_label("Arena_%s" % key.capitalize())
        arena.set_editor_property("boss", boss)
        arena.set_editor_property("boss_level", level)
        arena.set_editor_property("arena_radius", radius)
        arena.set_editor_property("engage_radius", radius * 0.75)
        if seal is not None:
            arena.set_editor_property("seal_fx", seal)
        # Checkpoint at the entrance, on the side facing the start
        toward = unreal.Vector(-x, -3800.0 - y, 0.0)
        length = max((toward.x ** 2 + toward.y ** 2) ** 0.5, 1.0)
        entrance = unreal.Vector(x + toward.x / length * (radius + 400.0), y + toward.y / length * (radius + 400.0), 60.0)
        import math
        yaw = math.degrees(math.atan2(-toward.y, -toward.x))
        checkpoint = actors.spawn_actor_from_class(unreal.BeyondCheckpoint, entrance, unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw))
        if checkpoint is not None:
            checkpoint.set_actor_label("Checkpoint_%s" % key.capitalize())
        log("placed the %s arena at (%.0f, %.0f), radius %.0f, level %d" % (key, x, y, radius, level))

    if unreal.BeyondEditorLibrary.build_navigation(world):
        log("rebuilt the navmesh")
    if levels.save_current_level():
        log("saved %s" % ARENA)
    else:
        warn("could not save %s" % ARENA)


# ---------------------------------------------------------------- main

def main():
    log("pass 11")
    missing = [name for name in ("BeyondBossCharacter", "BeyondBossDefinition", "BeyondBossArena", "BeyondGA_Leap", "BeyondGA_Beam",
                                 "BeyondGA_Summon", "BeyondGA_Teleport", "BeyondGA_Pull", "BeyondBossTwist", "BeyondEditorLibrary")
               if not hasattr(unreal, name)]
    if missing:
        warn("the editor is running an old build of the WorldBeyond module (%s missing). Close the editor, build "
             "WorldBeyondEditor (Development Editor, Win64) until it says Build succeeded, then run this script again. "
             "Nothing was changed." % ", ".join(missing))
        write_report("last_run_pass11.txt")
        raise RuntimeError("WorldBeyond C++ not rebuilt: %s missing" % ", ".join(missing))

    classes = step_heroes()
    sets = step_abilities()
    weapons = step_weapons()
    bosses = step_bosses(classes, sets, weapons)
    step_roster(bosses)
    step_arenas(bosses)
    write_report("last_run_pass11.txt")


main()
