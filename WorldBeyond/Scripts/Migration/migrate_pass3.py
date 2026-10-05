"""
Worlds Beyond - pass 3: fixes from the second playtest.

Run with the editor closed, after building the C++:
    UnrealEditor-Cmd.exe <path>/WorldBeyond.uproject -run=pythonscript -script=<path>/Scripts/Migration/migrate_pass3.py -unattended -nosplash -NullRHI

- "2" and R both press the heal slot once (the old IA_Heal fired every frame the key was held -> double voice lines);
  Angel's heal animation + voice now come from the heal ability itself (BP_Angel Ability Montages).
- Ji-Woong's sword rests on his left hip, draws / sheathes on the upper body, switches his idle, and is drawn
  automatically near enemies or when the sword combo starts; it goes back on the hip after a quiet spell.
- The ability bar keeps 3 slots (Q, E, R); the duo move has its own slot next to the Bond meter.
- Effects that the abilities manage themselves don't get the default 5 s lifetime.

Every asset saved is copied to Saved/MigrationBackups/<timestamp>/ first. Running it twice is safe.
"""
import os
import sys

import unreal

sys.path.insert(0, os.path.join(os.path.abspath(unreal.Paths.project_dir()), "Scripts", "Migration"))
from migration_common import (BACKUP_DIR, bp_class, cdo, load, log, make_key, save, set_props, tag, warn,  # noqa: E402
                              write_report)

ABILITIES = "/Game/WorldsBeyond/Abilities/"
ANIMS = "/Game/WorldsBeyond/Characters/MetaHuman/Anims/"
SWORD_ANIMS = "/Game/AwesomeSwordAnimationV3/Animation/IP/"


def step_heal_keys():
    """Two -> IA_AbilityR (the heal slot) instead of IA_Heal."""
    path = "/Game/Input/IMC_Default"
    imc = load(path)
    heal, slot_r = load("/Game/Input/Actions/IA_Heal"), load("/Game/Input/Actions/IA_AbilityR")
    if not (imc and heal and slot_r):
        return

    def mapped(action):
        return any(m.get_editor_property("action") == action and str(m.get_editor_property("key").get_editor_property("key_name")) == "Two"
                   for m in imc.get_editor_property("default_key_mappings").get_editor_property("mappings"))

    changed = False
    if mapped(heal):
        imc.unmap_key(heal, make_key("Two"))
        log("unmapped Two -> IA_Heal")
        changed = True
    if not mapped(slot_r):
        imc.map_key(slot_r, make_key("Two"))
        log("mapped Two -> IA_AbilityR")
        changed = True
    if changed:
        save(imc, path)


def step_heal_montage():
    """Angel plays AM_Heal (animation + voice) whenever her heal ability activates, once."""
    path = "/Game/WorldsBeyond/Characters/Angel/BP_Angel"
    bp = load(path)
    heal = bp_class("/Game/WorldsBeyond/Blueprints/Gameplay_Abilites/Abilites/GA_HealSpell")
    montage = load(ANIMS + "Heals/AM_Heal")
    if bp and heal and montage:
        set_props(bp, path, ability_montages={heal: montage})
        log("%s: GA_HealSpell plays AM_Heal" % path)


def step_sword_holster():
    """Ji-Woong: sword on the left hip, upper-body draw / sheathe, relaxed idle while sheathed, auto draw / sheathe."""
    path = ABILITIES + "JiWoong/GA_JiWoong_EquipWeapon"
    bp = load(path)
    if bp is None:
        return

    sword = unreal.BeyondWeaponLoadout()
    sword.set_editor_property("weapon_tag", tag("Weapon.Melee.Sword"))
    sword.set_editor_property("weapon_class", bp_class("/Game/WorldsBeyond/Weapons/BP_Weapon_Sword_JI"))
    sword.set_editor_property("attach_socket", "melee_equipped_socket")
    # Add this socket on metahuman_base_skel to fine-tune; until then the pelvis offset below is used
    sword.set_editor_property("holster_socket", "sword_hip_socket")
    sword.set_editor_property("holster_bone", "pelvis")
    sword.set_editor_property("holster_offset", unreal.Transform(
        location=unreal.Vector(-38.7, -45.0, -15.2),
        rotation=unreal.Rotator(roll=96.9, pitch=-66.2, yaw=-46.4),
        scale=unreal.Vector(1.0, 1.0, 1.0)))
    sword.set_editor_property("draw_animation", load(SWORD_ANIMS + "A_AwesomeSwordAnimationsV3_Unsheath_IP"))
    sword.set_editor_property("grab_time", 0.418)
    sword.set_editor_property("sheathe_animation", load(SWORD_ANIMS + "A_AwesomeSwordAnimationsV3_Sheath_IP"))
    sword.set_editor_property("release_time", 0.711)
    sword.set_editor_property("animation_play_rate", 1.0)
    sword.set_editor_property("armed_idle", load("/Game/Animations/Melee/sword-idle"))
    sword.set_editor_property("armed_walk_speed", 500.0)

    set_props(bp, path,
              loadouts=[sword],
              spawn_holstered=True,
              animation_slot="UpperBody",
              idle_variable="IdleAnimation",
              unarmed_idle=load(ANIMS + "Unarmed/MM_Idle1"),
              unarmed_walk_speed=500.0,
              auto_draw_radius=1000.0,
              auto_sheathe_delay=8.0,
              auto_sheathe_radius=1500.0,
              draw_on_montages=[load("/Game/Animations/Melee/Montage_SwordCombo")])
    log("configured %s (hip holster, upper-body draw, auto draw / sheathe)" % path)


def step_ability_bar():
    """Three slots (Q, E, R) once FillAbilitiesBar uses Get Ability Bar Abilities."""
    path = "/Game/WorldsBeyond/Blueprints/Widgets/Abilites/W_AbilitesBar"
    bp = load(path)
    if bp is None:
        return
    obj = cdo(bp)
    for name in ("MinimumSlots", "minimum_slots"):
        try:
            if obj.get_editor_property(name) != 3:
                obj.set_editor_property(name, 3)
                save(bp, path)
                log("%s: MinimumSlots = 3" % path)
            return
        except Exception:
            continue
    warn("%s: no MinimumSlots variable found" % path)


def step_fx_lifetimes():
    """Effects the abilities remove themselves: no automatic 5 s cut-off."""
    sunbrand_path = ABILITIES + "JiWoong/GA_JiWoong_Sunbrand"
    sunbrand = load(sunbrand_path)
    if sunbrand:
        brand = cdo(sunbrand).get_editor_property("brand")
        mark = brand.get_editor_property("mark_fx")
        if mark.get_editor_property("max_lifetime") != 0.0:
            mark.set_editor_property("max_lifetime", 0.0)
            brand.set_editor_property("mark_fx", mark)
            set_props(sunbrand, sunbrand_path, brand=brand)
            log("%s: brand mark lives as long as the brand" % sunbrand_path)

    step_path = ABILITIES + "JiWoong/GA_JiWoong_GildedStep"
    step = load(step_path)
    if step:
        trail = cdo(step).get_editor_property("trail_fx")
        if trail.get_editor_property("max_lifetime") != 0.0:
            trail.set_editor_property("max_lifetime", 0.0)
            set_props(step, step_path, trail_fx=trail)
            log("%s: trail is stopped by the dash" % step_path)


def main():
    log("backups -> %s" % BACKUP_DIR)
    step_heal_keys()
    step_heal_montage()
    step_sword_holster()
    step_ability_bar()
    step_fx_lifetimes()
    write_report("last_run_pass3.txt")


main()
