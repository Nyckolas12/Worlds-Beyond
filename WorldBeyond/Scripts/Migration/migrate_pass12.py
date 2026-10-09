"""
Worlds Beyond - pass 12: Plan 3 playtest fixes.

Run with the editor closed:
    UnrealEditor-Cmd.exe <path>/WorldBeyond.uproject -run=pythonscript -script=<path>/Scripts/Migration/migrate_pass12.py -unattended -nosplash -NullRHI

- Hit voice lines: BP_Angel and BP_Ji-Woong play their hit cue (PlaySound2D, no concurrency) on every OnDamageResponse,
  so a pack or a multi-hit attack stacked up to 16 copies. Both cues now share SC_Beyond_HitVoice: one hit line at a
  time for the whole party, a new one is dropped while one plays (never cut off), and none starts within
  HIT_VOICE_RETRIGGER seconds of the last. Tune it in the asset (Concurrency > Retrigger Time).
- Ji-Woong's old Blueprint LMB event is switched off again: pass 4 put Left Mouse Button in BP_Ji-Woong's Legacy Keys
  To Disable (his LMB is the GAS sword combo), but the entry was lost before pass 9 added F, so LMB ran both combos and
  the old one's voice line on every press. LMB and F are both off; "1" (draw / sheathe) stays on.

Every asset saved is copied to Saved/MigrationBackups/<timestamp>/ first. Report: last_run_pass12.txt.
"""
import os
import sys

import unreal

sys.path.insert(0, os.path.join(os.path.abspath(unreal.Paths.project_dir()), "Scripts", "Migration"))
from migration_common import ASSET_TOOLS, EAL, cdo, load, log, make_key, save, set_props, warn, write_report  # noqa: E402

JIWOONG = "/Game/WorldsBeyond/Characters/Ji-Woong/BP_Ji-Woong"
JIWOONG_KEYS_OFF = ["LeftMouseButton", "F"]
SOUNDS = "/Game/WorldsBeyond/Sounds"
HIT_VOICE_CONCURRENCY = SOUNDS + "/SC_Beyond_HitVoice"
HIT_VOICES = [
    SOUNDS + "/Angel/Angel_hitReact",
    SOUNDS + "/JI-Woong/JI-Woong_HitReact",
]
HIT_VOICE_RETRIGGER = 4.0


# ---------------------------------------------------------------- hit voice lines

def ensure_hit_voice_concurrency():
    concurrency = EAL.load_asset(HIT_VOICE_CONCURRENCY) if EAL.does_asset_exist(HIT_VOICE_CONCURRENCY) else None
    if concurrency is None:
        folder, name = HIT_VOICE_CONCURRENCY.rsplit("/", 1)
        concurrency = ASSET_TOOLS.create_asset(name, folder, unreal.SoundConcurrency, unreal.SoundConcurrencyFactory())
        if concurrency is None:
            warn("could not create %s" % HIT_VOICE_CONCURRENCY)
            return None
        log("created %s" % HIT_VOICE_CONCURRENCY)

    settings = concurrency.get_editor_property("concurrency")
    # New assets scale MaxCount per platform (PlatformMaxCount, 16), which ignores MaxCount
    settings.set_editor_property("enable_max_count_platform_scaling", False)
    settings.set_editor_property("max_count", 1)
    settings.set_editor_property("limit_to_owner", False)
    settings.set_editor_property("resolution_rule", unreal.MaxConcurrentResolutionRule.PREVENT_NEW)
    settings.set_editor_property("retrigger_time", HIT_VOICE_RETRIGGER)
    concurrency.set_editor_property("concurrency", settings)
    save(concurrency, HIT_VOICE_CONCURRENCY)
    saved = concurrency.get_editor_property("concurrency")
    log("%s: max %d voice(s) (platform scaling %s), prevent new, retrigger %.1f s" % (
        HIT_VOICE_CONCURRENCY, saved.get_editor_property("max_count"), saved.get_editor_property("enable_max_count_platform_scaling"),
        saved.get_editor_property("retrigger_time")))
    return concurrency


def step_hit_voices():
    concurrency = ensure_hit_voice_concurrency()
    if concurrency is None:
        return
    for path in HIT_VOICES:
        sound = load(path)
        if sound is None:
            continue
        sound.set_editor_property("override_concurrency", False)
        sound.set_editor_property("concurrency_set", {concurrency})
        save(sound, path)
        log("%s (%.2f s) -> %s" % (path, sound.get_editor_property("duration"), HIT_VOICE_CONCURRENCY))


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


# ---------------------------------------------------------------- main

def main():
    log("pass 12")
    step_hit_voices()
    step_jiwoong_keys()
    write_report("last_run_pass12.txt")


main()
