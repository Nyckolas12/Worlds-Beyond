"""
Worlds Beyond - pass 4: fixes from the third playtest.

Run with the editor closed, after building the C++:
    UnrealEditor-Cmd.exe <path>/WorldBeyond.uproject -run=pythonscript -script=<path>/Scripts/Migration/migrate_pass4.py -unattended -nosplash -NullRHI

- Ji-Woong's LMB runs the GAS sword combo (GA_JiWoong_SwordCombo) instead of the old Blueprint one: the combo window
  is 15 % longer, his attack voice line plays once per combo, and swings go on the upper body while he moves.
- Angel's staff works like Ji-Woong's sword: it rests on his back, he draws / stows it with the MagicStaff pack's
  animations (upper body), his idle follows it, and he draws it by himself near enemies or when he casts.

The buddy holding still during cutscenes is C++ only. Every asset saved is copied to Saved/MigrationBackups/<timestamp>/
first. Running it twice is safe.
"""
import os
import sys

import unreal

sys.path.insert(0, os.path.join(os.path.abspath(unreal.Paths.project_dir()), "Scripts", "Migration"))
from migration_common import (BACKUP_DIR, bp_class, cdo, ensure_blueprint, load, log, map_keys, save, set_props, tag,  # noqa: E402
                              warn, write_report)

ABILITIES = "/Game/WorldsBeyond/Abilities/"
ANIMS = "/Game/WorldsBeyond/Characters/MetaHuman/Anims/"
STAFF_ANIMS = "/Game/EssentialAnimation/MagicStaff/Animation/UE5/Sequence/"
LEGACY_EQUIP = "/Game/WorldsBeyond/Blueprints/Gameplay_Abilites/Abilites/GA_EquipWeapon"

STAFF_DRAW = STAFF_ANIMS + "Equip/UE5_WZ_Unsheathe_Back_Seq"
STAFF_SHEATHE = STAFF_ANIMS + "Equip/UE5_WZ_Sheathe_Back_Seq"
STAFF_HOLSTER_BONE = "spine_05"
STAFF_HAND_SOCKET = "staff_equipped_socket"


# ---------------------------------------------------------------- Ji-Woong: LMB on the GAS combo

def step_primary_attack_input():
    """LMB -> IA_PrimaryAttack -> Ability.Input.Primary on Ji-Woong; his old raw LMB Blueprint event is switched off."""
    actions = map_keys("/Game/Input/IMC_Default", {"IA_PrimaryAttack": "LeftMouseButton"})
    action = actions["IA_PrimaryAttack"]
    # Angel's magic spell is still a raw LMB key event in BP_Angel; the mapping must not swallow the key
    if action.get_editor_property("consume_input"):
        action.set_editor_property("consume_input", False)
        save(action, "/Game/Input/Actions/IA_PrimaryAttack")
        log("IA_PrimaryAttack lets LMB through to other bindings")

    path = "/Game/WorldsBeyond/Characters/Ji-Woong/BP_Ji-Woong"
    bp = load(path)
    if bp is None:
        return
    bindings = list(cdo(bp).get_editor_property("ability_input_bindings"))
    if not any(str(b.get_editor_property("input_tag").get_editor_property("tag_name")) == "Ability.Input.Primary" for b in bindings):
        binding = unreal.BeyondInputBinding()
        binding.set_editor_property("input_action", action)
        binding.set_editor_property("input_tag", tag("Ability.Input.Primary"))
        bindings.append(binding)
        log("%s: LMB presses Ability.Input.Primary (GA_JiWoong_SwordCombo)" % path)
    # Its only raw key event is the old LMB combo (which replayed the voice line on every press)
    set_props(bp, path, ability_input_bindings=bindings, disable_legacy_key_input=True)


def step_sword_combo():
    path = ABILITIES + "JiWoong/GA_JiWoong_SwordCombo"
    combo = load(path)
    if combo is None:
        return
    set_props(combo, path,
              combo_window_extension=0.15,
              combo_voice_line=load("/Game/WorldsBeyond/Sounds/JI-Woong/JI-Woong_Attacking"),
              legs_follow_movement=True,
              upper_body_slot="UpperBody",
              moving_speed=50.0,
              legs_blend_time=0.2)
    log("%s: window +15 %%, voice once per combo, upper-body swings while moving" % path)


# ---------------------------------------------------------------- Angel: staff on the back

def _compose(a, b):
    """a then b (Unreal's A * B)."""
    return unreal.MathLibrary.compose_transforms(a, b)


def _component_pose(anim, bone, time):
    """Bone transform in component space at time, from the animation's local poses."""
    AL = unreal.AnimationLibrary
    path = AL.find_bone_path_to_root(anim, bone)
    if not path:
        return None
    pose = unreal.Transform()
    for name in path:
        pose = _compose(pose, AL.get_bone_pose_for_time(anim, name, time, False))
    return pose


def _length(anim):
    try:
        return unreal.AnimationLibrary.get_sequence_length(anim)
    except Exception:
        return anim.get_play_length()


def _relative(anim, bone, to_bone, time):
    """bone's transform relative to to_bone at time, or None if the animation lacks either."""
    pose, parent = _component_pose(anim, bone, time), _component_pose(anim, to_bone, time)
    return unreal.MathLibrary.make_relative_transform(pose, parent) if pose is not None and parent is not None else None


def _on_back_until(anim, from_end, tolerance=1.5, step=1.0 / 60.0):
    """
    The pack animates its staff with the weapon_r bone. While the staff rests on the back, weapon_r doesn't move
    relative to the back bone. Draw (from the start): the last moment it is still on the back = the grab.
    Stow (from the end): the first moment it is back for good = the release. None without a weapon_r track.
    """
    length = _length(anim)
    times = [min(i * step, length) for i in range(int(length / step) + 2)]
    if from_end:
        times.reverse()
    resting = _relative(anim, "weapon_r", STAFF_HOLSTER_BONE, times[0])
    if resting is None:
        return None
    found = times[0]
    for t in times:
        pose = _relative(anim, "weapon_r", STAFF_HOLSTER_BONE, t)
        if pose is None or (pose.translation - resting.translation).length() > tolerance:
            break
        found = t
    return found


def _staff_holster(draw):
    """
    Where our staff rests on the back, relative to the holster bone: the pack staff's resting place (weapon_r at the
    start of the draw), corrected for how our staff sits in the hand (staff_equipped_socket) versus the pack's
    (weapon_r once drawn).
    """
    body = load("/Game/WorldsBeyond/Characters/Angel/Body/SKM_Angel_BodyMesh")
    socket = body.find_socket(STAFF_HAND_SOCKET) if body else None
    if socket is None:
        return None
    hand_bone = str(socket.get_editor_property("bone_name"))
    in_hand = unreal.Transform(location=socket.get_editor_property("relative_location"),
                               rotation=socket.get_editor_property("relative_rotation"),
                               scale=unreal.Vector(1.0, 1.0, 1.0))
    pack_in_hand = _relative(draw, "weapon_r", hand_bone, _length(draw))
    pack_on_back = _relative(draw, "weapon_r", STAFF_HOLSTER_BONE, 0.0)
    if pack_in_hand is None or pack_on_back is None:
        return None
    ours_from_pack = unreal.MathLibrary.make_relative_transform(in_hand, pack_in_hand)
    offset = _compose(ours_from_pack, pack_on_back)
    offset.scale3d = unreal.Vector(1.0, 1.0, 1.0)
    return offset


def add_to_ability_set(set_path, ability_class):
    data = load(set_path)
    if data is None or ability_class is None:
        return
    entries = list(data.get_editor_property("abilities"))
    if any(e.get_editor_property("ability") and e.get_editor_property("ability").get_name() == ability_class.get_name() for e in entries):
        return
    entry = unreal.BeyondAbilitySet_Ability()
    entry.set_editor_property("ability", ability_class)
    entry.set_editor_property("level", 1)
    entries.append(entry)
    data.set_editor_property("abilities", entries)
    save(data, set_path)
    log("%s: + %s (triggered by Event.Weapon.Equipped)" % (set_path, ability_class.get_name()))


def step_angel_staff():
    """Angel: staff on the back, upper-body draw / stow, idle follows the staff, auto draw / stow, quick-draw on casts."""
    draw, sheathe = load(STAFF_DRAW), load(STAFF_SHEATHE)

    # A staff that never leaves the back, or never rests on it, means no usable weapon_r track
    grab = release = offset = None
    try:
        grab = _on_back_until(draw, False) if draw else None
        release = _on_back_until(sheathe, True) if sheathe else None
        offset = _staff_holster(draw) if draw else None
    except Exception as e:
        warn("could not read the staff animations' bone poses (%s)" % e)
    if draw:
        log("staff draw %s: %.3f s long" % (draw.get_name(), _length(draw)))
    if sheathe:
        log("staff stow %s: %.3f s long" % (sheathe.get_name(), _length(sheathe)))
    if grab is None or grab < 0.05 or grab > _length(draw) - 0.05:
        grab = _length(draw) * 0.45 if draw else 0.4
        warn("staff draw: no usable weapon_r track, grab time falls back to %.2f s" % grab)
    if release is None or release < 0.05 or release > _length(sheathe) - 0.05:
        release = _length(sheathe) * 0.55 if sheathe else 0.7
        warn("staff stow: no usable weapon_r track, release time falls back to %.2f s" % release)
    if offset is None:
        offset = unreal.Transform()
        warn("could not work out where the staff rests on the back; add a '%s' socket on %s (metahuman_base_skel) "
             "to place it" % ("staff_back_socket", STAFF_HOLSTER_BONE))
    location, rotation = offset.translation, offset.rotation.rotator()
    log("staff: grab %.3f s, release %.3f s, back offset on %s: location (%.1f, %.1f, %.1f) rotation (pitch %.1f, yaw %.1f, roll %.1f)"
        % (grab, release, STAFF_HOLSTER_BONE, location.x, location.y, location.z, rotation.pitch, rotation.yaw, rotation.roll))

    staff = unreal.BeyondWeaponLoadout()
    staff.set_editor_property("weapon_tag", tag("Weapon.Ranged.Staff"))
    staff.set_editor_property("weapon_class", bp_class("/Game/WorldsBeyond/Weapons/BP_Weapon_Staff"))
    staff.set_editor_property("attach_socket", STAFF_HAND_SOCKET)
    # Add this socket on metahuman_base_skel to fine-tune; until then the computed offset below is used
    staff.set_editor_property("holster_socket", "staff_back_socket")
    staff.set_editor_property("holster_bone", STAFF_HOLSTER_BONE)
    staff.set_editor_property("holster_offset", offset)
    staff.set_editor_property("draw_animation", draw)
    staff.set_editor_property("grab_time", grab)
    staff.set_editor_property("sheathe_animation", sheathe)
    staff.set_editor_property("release_time", release)
    staff.set_editor_property("animation_play_rate", 1.0)
    staff.set_editor_property("armed_idle", load("/Game/WorldsBeyond/Characters/Angel/Anims/UE5_WZ_Idle_Seq"))

    path = ABILITIES + "Angel/GA_Angel_EquipStaff"
    bp = ensure_blueprint(path, unreal.BeyondGA_EquipWeapon)

    casts = [load(STAFF_ANIMS + "Attack/UE5_WZ_Attack_02_Seq_Montage"),   # LMB spell, Lightning Strike
             load(STAFF_ANIMS + "Attack/UE5_BM_Attack_08_Seq_Montage"),   # Arcane Bolt
             load(ABILITIES + "Duo/AM_Duo_Conduit_Telekinesis")]          # Heaven's Judgment
    set_props(bp, path,
              loadouts=[staff],
              spawn_holstered=True,
              animation_slot="UpperBody",
              idle_variable="IdleAnimation",
              unarmed_idle=load(ANIMS + "Unarmed/MM_Idle"),
              auto_draw_radius=1000.0,
              auto_sheathe_delay=8.0,
              auto_sheathe_radius=1500.0,
              draw_on_montages=[m for m in casts if m])
    log("configured %s (staff on the back, upper-body draw / stow, auto draw / stow)" % path)

    add_to_ability_set(ABILITIES + "DA_AbilitySet_Angel", bp.generated_class())

    angel_path = "/Game/WorldsBeyond/Characters/Angel/BP_Angel"
    angel = load(angel_path)
    if angel:
        suppressed = list(cdo(angel).get_editor_property("suppressed_abilities"))
        legacy = bp_class(LEGACY_EQUIP)
        if legacy and not any(c and c.get_name() == legacy.get_name() for c in suppressed):
            suppressed.append(legacy)
        # The "1" key keeps sending Event.Weapon.Equipped / Weapon.Ranged.Staff; the new ability toggles draw / stow
        set_props(angel, angel_path, suppressed_abilities=suppressed, default_weapon_tag=tag("Weapon.Ranged.Staff"))
        log("%s: old Blueprint equip suppressed, spawns with the staff on his back" % angel_path)


def main():
    log("backups -> %s" % BACKUP_DIR)
    step_primary_attack_input()
    step_sword_combo()
    step_angel_staff()
    write_report("last_run_pass4.txt")


main()
