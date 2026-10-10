"""
Worlds Beyond - pass 12: Plan 3 playtest fixes.

Run with the editor closed, after building the C++:
    UnrealEditor-Cmd.exe <path>/WorldBeyond.uproject -run=pythonscript -script=<path>/Scripts/Migration/migrate_pass12.py -unattended -nosplash -NullRHI

- Hit voice lines: BP_Angel and BP_Ji-Woong play their hit cue (PlaySound2D, no concurrency) on every OnDamageResponse,
  so a pack or a multi-hit attack stacked up to 16 copies. Each cue now has its own concurrency override: one line at a
  time per demigod, a new one is dropped while one plays (never cut off), and none starts within HIT_VOICE_RETRIGGER
  seconds of that demigod's last (Concurrency > Override > Retrigger Time on the cue). The first version shared one
  group between both cues, so the buddy's lines silenced the player's; that SC_Beyond_HitVoice asset is removed.
- Ji-Woong's old Blueprint LMB event is switched off again: pass 4 put Left Mouse Button in BP_Ji-Woong's Legacy Keys
  To Disable (his LMB is the GAS sword combo), but the entry was lost before pass 9 added F, so LMB ran both combos and
  the old one's voice line on every press. LMB and F are both off; "1" (draw / sheathe) stays on.
- Duo moves (GA_Duo_HeavensJudgment, GA_Duo_EclipseBrand, GA_Duo_TempestAegis): the charged Ji-Woong rushes to the
  locked enemy (Approach): dash begin / loop animations, Gilded Step's golden trail, a yellow blink as the fallback.
- Boss arenas in TestArena: Party Rush (the dash in before the walls rise) gets the same dash animations and a blink.

Every asset saved is copied to Saved/MigrationBackups/<timestamp>/ first. Report: last_run_pass12.txt.
"""
import os
import sys

import unreal

sys.path.insert(0, os.path.join(os.path.abspath(unreal.Paths.project_dir()), "Scripts", "Migration"))
from migration_common import EAL, backup, cdo, fx, load, log, make_key, package_file, save, set_props, warn, write_report  # noqa: E402

JIWOONG = "/Game/WorldsBeyond/Characters/Ji-Woong/BP_Ji-Woong"
JIWOONG_KEYS_OFF = ["LeftMouseButton", "F"]
SOUNDS = "/Game/WorldsBeyond/Sounds"
SHARED_HIT_VOICE_CONCURRENCY = SOUNDS + "/SC_Beyond_HitVoice"
HIT_VOICES = [
    SOUNDS + "/Angel/Angel_hitReact",
    SOUNDS + "/JI-Woong/JI-Woong_HitReact",
]
HIT_VOICE_RETRIGGER = 4.0

DUO = "/Game/WorldsBeyond/Abilities/Duo/"
DUO_ABILITIES = [DUO + "GA_Duo_HeavensJudgment", DUO + "GA_Duo_EclipseBrand", DUO + "GA_Duo_TempestAegis"]
DASH_ANIMS = "/Game/WorldsBeyond/Characters/MetaHuman/Anims/Dash/"
ARENA_MAP = "/Game/WorldsBeyond/Maps/TestArena"


# ---------------------------------------------------------------- hit voice lines

def step_hit_voices():
    for path in HIT_VOICES:
        sound = load(path)
        if sound is None:
            continue
        settings = sound.get_editor_property("concurrency_overrides")
        # Platform scaling would use PlatformMaxCount (16) and ignore Max Count
        settings.set_editor_property("enable_max_count_platform_scaling", False)
        settings.set_editor_property("max_count", 1)
        settings.set_editor_property("limit_to_owner", False)
        settings.set_editor_property("resolution_rule", unreal.MaxConcurrentResolutionRule.PREVENT_NEW)
        settings.set_editor_property("retrigger_time", HIT_VOICE_RETRIGGER)
        sound.set_editor_property("concurrency_overrides", settings)
        sound.set_editor_property("override_concurrency", True)
        sound.set_editor_property("concurrency_set", set())
        save(sound, path)
        saved = sound.get_editor_property("concurrency_overrides")
        log("%s (%.2f s): its own concurrency, max %d, prevent new, retrigger %.1f s" % (
            path, sound.get_editor_property("duration"), saved.get_editor_property("max_count"), saved.get_editor_property("retrigger_time")))

    # The first version's shared group (made by this pass earlier on 2026-10-09)
    if EAL.does_asset_exist(SHARED_HIT_VOICE_CONCURRENCY):
        referencers = [str(r) for r in EAL.find_package_referencers_for_asset(SHARED_HIT_VOICE_CONCURRENCY, False)]
        if referencers:
            warn("%s is still used by %s; left in place" % (SHARED_HIT_VOICE_CONCURRENCY, ", ".join(referencers)))
        else:
            backup(SHARED_HIT_VOICE_CONCURRENCY)
            stale_file = package_file(SHARED_HIT_VOICE_CONCURRENCY)
            if EAL.delete_asset(SHARED_HIT_VOICE_CONCURRENCY):
                # A commandlet unloads the package but can leave its file behind
                if stale_file and os.path.exists(stale_file):
                    os.remove(stale_file)
                log("removed %s (no longer used)" % SHARED_HIT_VOICE_CONCURRENCY)
            else:
                warn("could not remove %s" % SHARED_HIT_VOICE_CONCURRENCY)


# ---------------------------------------------------------------- Ji-Woong's legacy keys

def step_jiwoong_keys():
    jiwoong = load(JIWOONG)
    if jiwoong is None:
        return
    keys = list(cdo(jiwoong).get_editor_property("legacy_keys_to_disable"))
    names = [str(key.get_editor_property("key_name")) for key in keys]
    missing = [name for name in JIWOONG_KEYS_OFF if name not in names]
    if not missing and cdo(jiwoong).get_editor_property("disable_legacy_key_input"):
        log("%s: legacy key events already off: %s" % (JIWOONG, ", ".join(names)))
        return
    keys += [make_key(name) for name in missing]
    set_props(jiwoong, JIWOONG, disable_legacy_key_input=True, legacy_keys_to_disable=keys)
    log("%s: legacy key events off: %s (added %s)" % (JIWOONG, ", ".join(names + missing), ", ".join(missing) or "none"))


# ---------------------------------------------------------------- rushes (duo approach, arena entry)

def rush_settings(speed, stop_distance, max_distance, timeout, golden_trail):
    settings = unreal.BeyondRushSettings()
    settings.set_editor_property("speed", speed)
    settings.set_editor_property("stop_distance", stop_distance)
    settings.set_editor_property("max_rush_distance", max_distance)
    settings.set_editor_property("timeout", timeout)
    settings.set_editor_property("invincible", True)
    settings.set_editor_property("pass_through_pawns", True)
    settings.set_editor_property("dash_montage", load(DASH_ANIMS + "UE5_Dash_Begin_Seq_Montage"))
    settings.set_editor_property("loop_montage", load(DASH_ANIMS + "UE5_Dash_Loop_Seq_Montage"))
    if golden_trail:
        # Ji-Woong's Gilded Step trail; left empty, each demigod uses its own dash trail (if it has one)
        settings.set_editor_property("trail_fx", fx(system="/Game/ParagonFengMao/FX/Particles/Abilities/Dash/FX/P_FengMao_Dash_Trail_Mesh",
                                                    sound=SOUNDS + "/JI-Woong/JI_Blink"))
    settings.set_editor_property("blink_depart_fx", fx(system="/Game/VFX/Teleport/P_TeleportStart_Yellow", sound="/Game/Audio/teleport-sound", scale=0.6))
    settings.set_editor_property("blink_arrive_fx", fx(system="/Game/VFX/Teleport/P_Explosion_Yellow", scale=0.5))
    return settings


def step_duo_approach():
    for path in DUO_ABILITIES:
        bp = load(path)
        if bp is None:
            continue
        set_props(bp, path, approach=rush_settings(2600.0, 100.0, 3000.0, 1.2, True))
        log("%s: Ji-Woong rushes to the locked enemy (target search %.0f, slams in place within %.0f)" % (
            path, cdo(bp).get_editor_property("target_search_radius"), cdo(bp).get_editor_property("dash_animation_distance")))


def step_arena_rush():
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not EAL.does_asset_exist(ARENA_MAP) or not levels.load_level(ARENA_MAP):
        warn("could not open %s; arenas keep the default Party Rush (no dash animation)" % ARENA_MAP)
        return
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    arenas = [a for a in actors.get_all_level_actors() if isinstance(a, unreal.BeyondBossArena)]
    for arena in arenas:
        arena.set_editor_property("pull_party_inside", True)
        arena.set_editor_property("party_rush", rush_settings(2400.0, 60.0, 1500.0, 1.0, False))
        log("%s (%s): party dashes in before the walls rise" % (arena.get_actor_label(), arena.get_editor_property("boss").get_name()
                                                               if arena.get_editor_property("boss") else "no boss"))
    if not arenas:
        warn("no BeyondBossArena in %s (run migrate_pass11.py first)" % ARENA_MAP)
        return
    backup(ARENA_MAP)
    if levels.save_current_level():
        log("saved %s" % ARENA_MAP)
    else:
        warn("could not save %s" % ARENA_MAP)


# ---------------------------------------------------------------- main

def main():
    log("pass 12")
    missing = [name for name in ("BeyondRushSettings", "BeyondRushComponent") if not hasattr(unreal, name)]
    if missing:
        warn("the editor is running an old build of the WorldBeyond module (%s missing). Close the editor, build "
             "WorldBeyondEditor (Development Editor, Win64) until it says Build succeeded, then run this script again. "
             "Nothing was changed." % ", ".join(missing))
        write_report("last_run_pass12.txt")
        raise RuntimeError("WorldBeyond C++ not rebuilt: %s missing" % ", ".join(missing))

    step_hit_voices()
    step_jiwoong_keys()
    step_duo_approach()
    step_arena_rush()
    write_report("last_run_pass12.txt")


main()
