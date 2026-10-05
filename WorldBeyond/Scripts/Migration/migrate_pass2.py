"""
Worlds Beyond - pass 2: regression fixes, Ji-Woong's new powers and the duo super move.

Run with the editor closed, after building the C++ (it needs the new Beyond classes):
    UnrealEditor-Cmd.exe <path>/WorldBeyond.uproject -run=pythonscript -script=<path>/Scripts/Migration/migrate_pass2.py -unattended -nosplash -NullRHI

- Angel's E becomes GA_Angel_LightningStrike (BeyondGA_GroundStrike): the old GA_AOEAttack lost its montages
  and never dealt damage.
- Ji-Woong: Q Gilded Step (dash), E Sunbrand (brand), sword draw/sheathe with animation, sword combo notifies.
- Both: G fires the duo super move "Heaven's Judgment" when the party's Bond meter is full.
- The boss health bar is shown by the player controller (BP_Enemy_Boss gets a Boss Bar Widget Class).
- Ability bar icons for the new abilities (placeholders from the existing icon set).

Every asset saved is copied to Saved/MigrationBackups/<timestamp>/ first. Each step checks the current state,
so running it twice is safe.
"""
import json
import os
import sys

import unreal

sys.path.insert(0, os.path.join(os.path.abspath(unreal.Paths.project_dir()), "Scripts", "Migration"))
from migration_common import (BACKUP_DIR, bp_class, cdo, ensure_blueprint, ensure_montage, fx,  # noqa: E402
                              load, log, map_keys, save, set_props, tag, tag_container, warn, write_report)

ABILITIES = "/Game/WorldsBeyond/Abilities/"
FXV = "/Game/FXVarietyPack/Particles/"
AUDIO = "/Game/Audio/"
SHAKE = "/Game/VFX/CameraShakes/BP_CameraShake_Hit_Enemy"
GOLD = (1.0, 0.78, 0.2, 1.0)
BLUE = (0.25, 0.5, 1.0, 1.0)
PURPLE = (0.6, 0.2, 1.0, 1.0)

LEGACY_EQUIP = "/Game/WorldsBeyond/Blueprints/Gameplay_Abilites/Abilites/GA_EquipWeapon"
LEGACY_BLINK = "/Game/WorldsBeyond/Blueprints/Gameplay_Abilites/Abilites/GA_Blink"
LEGACY_AOE = "/Game/GameplayAbilitySystem/Abilities/GA_AOEAttack"
LEGACY_DASH = "/Game/GameplayAbilitySystem/Abilities/GA_Dash"


# ---------------------------------------------------------------- abilities

def step_lightning_strike():
    """Angel's E: hold to aim the ground decal, release, lightning lands, enemies in the circle take damage."""
    path = ABILITIES + "Angel/GA_Angel_LightningStrike"
    bp = ensure_blueprint(path, unreal.BeyondGA_GroundStrike)
    set_props(bp, path,
              input_tag=tag("Ability.Input.E"),
              target_actor_class=bp_class("/Game/GameplayAbilitySystem/TargetingActors/GATargetActor_GroundTrace_Decal"),
              max_target_range=1500.0,
              cast_montage=load("/Game/EssentialAnimation/MagicStaff/Animation/UE5/Sequence/Attack/UE5_WZ_Attack_02_Seq_Montage"),
              fallback_strike_delay=0.35,
              impact_delay=0.15,
              strike_cue_tag=tag("GameplayCue.LightningBolt"),
              strike_fx=fx(sound=AUDIO + "ground-smash-explode", shake=SHAKE, shake_radius=1500.0),
              damage=100.0,
              radius=256.0,
              damage_type=tag("DamageType.Explosion"),
              hit_response=tag("Event.Hit.Stagger"),
              cooldown_gameplay_effect_class=bp_class("/Game/GameplayAbilitySystem/Effects/GE_AOEAttack_Cooldown"),
              ai_usable=True, ai_min_range=300.0, ai_max_range=1500.0, ai_weight=2.0,
              activation_owned_tags=tag_container("Ability.Active"),
              activation_blocked_tags=tag_container("Ability.Active"))
    return bp


def step_gilded_step():
    """Ji-Woong's Q: golden dash through enemies; everyone crossed is hit a moment later."""
    path = ABILITIES + "JiWoong/GA_JiWoong_GildedStep"
    bp = ensure_blueprint(path, unreal.BeyondGA_Dash)
    set_props(bp, path,
              input_tag=tag("Ability.Input.Q"),
              dash_distance=700.0,
              dash_duration=0.25,
              exit_speed=400.0,
              invincible_while_dashing=True,
              pass_through_pawns=True,
              dash_montage=load("/Game/WorldsBeyond/Characters/MetaHuman/Anims/Dash/UE5_Dash_Begin_Seq_Montage"),
              trail_fx=fx(system="/Game/ParagonFengMao/FX/Particles/Abilities/Dash/FX/P_FengMao_Dash_Trail_Mesh",
                          sound="/Game/WorldsBeyond/Sounds/JI-Woong/JI_Blink"),
              path_damage=30.0,
              path_radius=90.0,
              detonate_delay=0.4,
              path_damage_type=tag("DamageType.Melee"),
              path_hit_response=tag("Event.Hit.Stagger"),
              detonate_fx=fx(system="/Game/VFX/Teleport/P_Explosion_Yellow", sound=AUDIO + "sci-fi-explosion", scale=0.6),
              cost_gameplay_effect_class=bp_class("/Game/GameplayAbilitySystem/Effects/GE_Dash_Cost"),
              cooldown_gameplay_effect_class=bp_class("/Game/GameplayAbilitySystem/Effects/GE_Dash_Cooldown"),
              ai_usable=True, ai_min_range=400.0, ai_max_range=900.0, ai_weight=1.0,
              activation_owned_tags=tag_container("Ability.Active"),
              activation_blocked_tags=tag_container("Ability.Active"))
    return bp


def step_sunbrand():
    """Ji-Woong's E: a golden sigil that makes the target take more damage; his next sword hit detonates it."""
    path = ABILITIES + "JiWoong/GA_JiWoong_Sunbrand"
    bp = ensure_blueprint(path, unreal.BeyondGA_Brand)

    brand = unreal.BeyondBrandSettings()
    brand.set_editor_property("duration", 8.0)
    brand.set_editor_property("damage_taken_multiplier", 1.2)
    brand.set_editor_property("detonate_damage", 40.0)
    brand.set_editor_property("detonate_radius", 300.0)
    brand.set_editor_property("detonate_hit_response", tag("Event.Hit.Stagger"))
    brand.set_editor_property("drain_fraction", 0.5)
    brand.set_editor_property("mark_fx", fx(system=FXV + "P_ky_magicCircle1", scale=0.5, offset=(0.0, 0.0, 130.0), color=GOLD))
    brand.set_editor_property("detonate_fx", fx(system="/Game/VFX/Teleport/P_Explosion_Yellow", sound=AUDIO + "ground-smash-explode",
                                                shake=SHAKE, scale=0.8, shake_radius=1500.0))

    set_props(bp, path,
              input_tag=tag("Ability.Input.E"),
              brand=brand,
              range=1500.0,
              aim_assist_angle=20.0,
              cast_montage=load("/Game/Animations/Magic/Montage_Hadouken"),
              fallback_apply_delay=0.5,
              cast_fx=fx(system="/Game/VFX/Teleport/P_TeleportStart_Yellow", sound=AUDIO + "magical-spell-cast-190272", scale=0.5),
              cast_socket="hand_r",
              apply_fx=fx(system="/Game/EssentialAnimation/MagicStaff/Demo/Particles/P_Sparks_Burst", color=GOLD),
              cooldown_duration=8.0,
              cooldown_tags=tag_container("Cooldown.Sunbrand"),
              ai_usable=True, ai_min_range=300.0, ai_max_range=1500.0, ai_weight=2.0,
              activation_owned_tags=tag_container("Ability.Active"),
              activation_blocked_tags=tag_container("Ability.Active"))
    return bp


def step_equip_weapon():
    """Ji-Woong draws / sheathes his sword with animation (the "1" key sends Event.Weapon.Equipped)."""
    path = ABILITIES + "JiWoong/GA_JiWoong_EquipWeapon"
    bp = ensure_blueprint(path, unreal.BeyondGA_EquipWeapon)

    sword = unreal.BeyondWeaponLoadout()
    sword.set_editor_property("weapon_tag", tag("Weapon.Melee.Sword"))
    sword.set_editor_property("weapon_class", bp_class("/Game/WorldsBeyond/Weapons/BP_Weapon_Sword_JI"))
    sword.set_editor_property("attach_socket", "melee_equipped_socket")
    sword.set_editor_property("equip_montage", load("/Game/Animations/Melee/Montage_Unsheath_Sword"))
    sword.set_editor_property("attach_time", 0.42)
    sword.set_editor_property("unequip_montage", load("/Game/Animations/Melee/Montage_Sheath_Sword"))
    sword.set_editor_property("detach_time", 1.27)
    sword.set_editor_property("armed_anim_class", bp_class("/Game/WorldsBeyond/Characters/MetaHuman/Anims/ABP_JI-Woong_Sword"))
    sword.set_editor_property("armed_walk_speed", 500.0)

    set_props(bp, path,
              loadouts=[sword],
              unarmed_anim_class=bp_class("/Game/WorldsBeyond/Characters/MetaHuman/Anims/ABP_JI-Woong"),
              unarmed_walk_speed=500.0)
    return bp


def step_duo():
    """G with a full Bond meter: Angel's lightning flashes, Ji-Woong absorbs it and slams out a shockwave."""
    conduit_montage = ensure_montage(
        "/Game/EssentialAnimation/MagicStaff/Animation/UE5/Sequence/Telekinesis/UE5_BM_Telekinesis_A1_Slam_Seq",
        ABILITIES + "Duo/AM_Duo_Conduit_Telekinesis")

    path = ABILITIES + "Duo/GA_Duo_HeavensJudgment"
    bp = ensure_blueprint(path, unreal.BeyondGA_DuoStrike)
    set_props(bp, path,
              input_tag=tag("Ability.Input.Duo"),
              partner_range=1500.0,
              # 1 Heaven
              conduit_montage=conduit_montage,
              flash_count=7,
              flash_interval=0.2,
              flash_radius=1200.0,
              flash_damage=15.0,
              flash_launch_speed=500.0,
              flash_fx=[fx(system="/Game/VFX/LightningStrike/Particles/NS_LightningBolt", sound=AUDIO + "ground-smash-cast", color=BLUE),
                        fx(system=FXV + "P_ky_lightning3", sound=AUDIO + "ground-smash-cast", color=PURPLE)],
              # 2 Absorb
              absorb_duration=0.8,
              striker_charge_montage=load("/Game/Animations/Axe/Montage_Axe_Battlecry"),
              conduit_release_fx=fx(system=FXV + "P_ky_thunderBall", scale=0.6, color=PURPLE),
              arc_fx=fx(system=FXV + "P_ky_lightning2", scale=0.6, color=BLUE),
              arc_steps=4,
              charged_aura_fx=[fx(system=FXV + "P_ky_thunderBall", scale=1.2, color=BLUE),
                               fx(system="/Game/VFX/Teleport/P_TeleportStart_Yellow", scale=1.0)],
              # 3 Judgment
              striker_slam_montage=load("/Game/Animations/Melee/Montage_Sword_Jump_Attack"),
              slam_impact_time=0.75,
              shockwave_radius=900.0,
              shockwave_damage_center=180.0,
              shockwave_damage_edge=80.0,
              knockback_speed=900.0,
              shockwave_hit_response=tag("Event.Hit.KnockBack"),
              shockwave_fx=[fx(system=FXV + "P_ky_shotShockwave", sound=AUDIO + "ground-smash-explode", shake=SHAKE,
                               scale=3.0, color=GOLD, shake_radius=3000.0),
                            fx(system="/Game/VFX/Teleport/P_Explosion_Yellow", scale=2.5),
                            fx(system=FXV + "P_ky_lightning3", scale=2.0, color=PURPLE),
                            fx(system="/Game/ParagonSparrow/FX/Particles/Sparrow/Skins/Rogue/P_Sparrow_UltHit", scale=2.0)],
              recovery_time=0.6)
    return bp


# ---------------------------------------------------------------- characters

def update_ability_set(path, remove_paths, add_entries):
    """Drop the replaced abilities, add the new ones (keeps anything else already in the set)."""
    data = load(path)
    if data is None:
        return
    removed = [bp_class(p) for p in remove_paths]
    removed_names = {c.get_name() for c in removed if c}

    entries = [e for e in data.get_editor_property("abilities")
               if not (e.get_editor_property("ability") and e.get_editor_property("ability").get_name() in removed_names)]
    present = {e.get_editor_property("ability").get_name() for e in entries if e.get_editor_property("ability")}

    for ability_class, input_tag in add_entries:
        if not ability_class or ability_class.get_name() in present:
            continue
        entry = unreal.BeyondAbilitySet_Ability()
        entry.set_editor_property("ability", ability_class)
        entry.set_editor_property("level", 1)
        if input_tag:
            entry.set_editor_property("input_tag", tag(input_tag))
        entries.append(entry)
        log("%s: + %s (%s)" % (path, ability_class.get_name(), input_tag or "no slot"))

    data.set_editor_property("abilities", entries)
    save(data, path)


def with_duo_binding(bindings, duo_action):
    result = list(bindings)
    if not any(str(b.get_editor_property("input_tag").get_editor_property("tag_name")) == "Ability.Input.Duo" for b in result):
        binding = unreal.BeyondInputBinding()
        binding.set_editor_property("input_action", duo_action)
        binding.set_editor_property("input_tag", tag("Ability.Input.Duo"))
        result.append(binding)
    return result


def step_characters(actions, strike, gilded, sunbrand, equip, duo):
    update_ability_set(ABILITIES + "DA_AbilitySet_Angel", [LEGACY_AOE],
                       [(strike.generated_class(), "Ability.Input.E"),
                        (duo.generated_class(), "Ability.Input.Duo")])
    update_ability_set(ABILITIES + "DA_AbilitySet_JiWoong", [LEGACY_DASH],
                       [(gilded.generated_class(), "Ability.Input.Q"),
                        (sunbrand.generated_class(), "Ability.Input.E"),
                        (equip.generated_class(), None),
                        (duo.generated_class(), "Ability.Input.Duo")])

    angel_path = "/Game/WorldsBeyond/Characters/Angel/BP_Angel"
    angel = load(angel_path)
    if angel:
        set_props(angel, angel_path,
                  duo_role=unreal.BeyondDuoRole.CONDUIT,
                  ability_input_bindings=with_duo_binding(cdo(angel).get_editor_property("ability_input_bindings"), actions["IA_Duo"]))
        log("configured %s (duo conduit)" % angel_path)

    ji_path = "/Game/WorldsBeyond/Characters/Ji-Woong/BP_Ji-Woong"
    ji = load(ji_path)
    if ji:
        set_props(ji, ji_path,
                  duo_role=unreal.BeyondDuoRole.STRIKER,
                  # Blink gives way to Gilded Step on Q; the Blueprint equip only worked for Angel
                  suppressed_abilities=[bp_class(LEGACY_EQUIP), bp_class(LEGACY_BLINK)],
                  default_weapon_tag=tag("Weapon.Melee.Sword"),
                  ability_input_bindings=with_duo_binding(cdo(ji).get_editor_property("ability_input_bindings"), actions["IA_Duo"]))
        log("configured %s (duo striker, sword on spawn)" % ji_path)


def step_boss():
    path = "/Game/Enemies/BossEnemy/BP_Enemy_Boss"
    bp = load(path)
    if bp:
        set_props(bp, path, boss_bar_widget_class=bp_class("/Game/Widgets/W_BossHealthBar"), boss_bar_show_radius=2500.0)
        log("configured %s (boss bar)" % path)


# ---------------------------------------------------------------- sword combo

def step_sword_combo():
    """Hit and combo-window notifies on Montage_SwordCombo (they match its Slash / ResumeComboWindow notifies)."""
    AL = unreal.AnimationLibrary
    montage_path = "/Game/Animations/Melee/Montage_SwordCombo"
    montage = load(montage_path)
    if montage is None:
        return

    notify = {name: bp_class("/Game/AnimNotifies/" + name)
              for name in ("AN_HitScanStart", "AN_HitScanEnd", "AN_ContinueComboStart", "AN_ContinueComboEnd")}
    existing = set()
    for event in AL.get_animation_notify_events(montage):
        obj = event.get_editor_property("notify")
        if obj:
            existing.add(obj.get_class().get_name())

    wanted = [("AN_HitScanStart", t) for t in (0.62, 1.20, 2.45)] + \
             [("AN_HitScanEnd", t) for t in (0.80, 1.40, 2.65)] + \
             [("AN_ContinueComboStart", t) for t in (0.618, 1.177)] + \
             [("AN_ContinueComboEnd", t) for t in (1.012, 1.592)]

    if all((name + "_C") in existing for name, _ in wanted):
        log("%s already has the combat notifies" % montage_path)
    else:
        track = "BeyondCombat"
        if not AL.is_valid_anim_notify_track_name(montage, track):
            AL.add_animation_notify_track(montage, track, unreal.LinearColor(1.0, 0.78, 0.2, 1.0))
        for name, time in wanted:
            if (name + "_C") in existing:
                continue
            if notify[name] is None:
                warn("missing notify class %s" % name)
                continue
            AL.add_animation_notify_event(montage, track, time, notify[name])
        save(montage, montage_path)
        log("%s: added hit-scan and combo-window notifies" % montage_path)

    combo_path = ABILITIES + "JiWoong/GA_JiWoong_SwordCombo"
    combo = load(combo_path)
    if combo:
        set_props(combo, combo_path, stop_if_combo_window_missed=True)


# ---------------------------------------------------------------- ability bar icons

def step_ability_icons(classes):
    """Rows in DT_AbilityMetaData (the ability bar looks icons up by class name). Placeholder art for now."""
    path = "/Game/WorldsBeyond/Blueprints/Widgets/Data/DT_AbilityMetaData"
    table = load(path)
    if table is None:
        return
    lib = unreal.DataTableFunctionLibrary
    try:
        rows = json.loads(lib.export_data_table_to_json_string(table))
    except Exception as e:
        warn("could not read %s (%s); add icon rows by hand" % (path, e))
        return
    if not rows:
        warn("%s is empty; add icon rows by hand" % path)
        return

    by_name = {row.get("Name"): row for row in rows}
    template = by_name.get("GA_HealSpell_C") or rows[0]
    images = {row.get("Name"): next((v for v in row.values() if isinstance(v, str) and "/Images/" in v), None) for row in rows}

    changed = False
    for class_name, icon_row, label in classes:
        if class_name in by_name:
            continue
        row = json.loads(json.dumps(template))
        row["Name"] = class_name
        image = images.get(icon_row)
        for key, value in row.items():
            if key == "Name":
                continue
            if isinstance(value, str) and "/Images/" in value and image:
                row[key] = image
            elif isinstance(value, str) and ("name" in key.lower() or "title" in key.lower()):
                row[key] = label
        rows.append(row)
        changed = True
        log("%s: + %s (icon from %s)" % (path, class_name, icon_row))

    if changed:
        if lib.fill_data_table_from_json_string(table, json.dumps(rows)):
            save(table, path)
        else:
            warn("could not write %s" % path)


# ---------------------------------------------------------------- main

def main():
    log("backups -> %s" % BACKUP_DIR)
    actions = map_keys("/Game/Input/IMC_Default", {"IA_Duo": "G"})
    strike = step_lightning_strike()
    gilded = step_gilded_step()
    sunbrand = step_sunbrand()
    equip = step_equip_weapon()
    duo = step_duo()
    step_characters(actions, strike, gilded, sunbrand, equip, duo)
    step_boss()
    step_sword_combo()
    step_ability_icons([
        ("GA_Angel_LightningStrike_C", "GA_EquipWeapon_C", "Lightning Strike"),
        ("GA_JiWoong_GildedStep_C", "GA_Blink_C", "Gilded Step"),
        ("GA_JiWoong_Sunbrand_C", "GA_EquipWeapon_C", "Sunbrand"),
        ("GA_Duo_HeavensJudgment_C", "GA_HealSpell_C", "Heaven's Judgment"),
    ])
    write_report("last_run_pass2.txt")


main()
