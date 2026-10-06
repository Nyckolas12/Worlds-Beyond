"""
Worlds Beyond - pass 7: leveling (Plan 1A).

Run with the editor closed, after building the C++:
    UnrealEditor-Cmd.exe <path>/WorldBeyond.uproject -run=pythonscript -script=<path>/Scripts/Migration/migrate_pass7.py -unattended -nosplash -NullRHI

- Enemy ranks decide how much EXP a kill is worth (Project Settings -> Game -> Worlds Beyond Progression):
  BP_Enemy_Base / Melee / Ranged are Regular, BP_Enemy_Mage is Elite, BP_Enemy_Boss is a Boss.
- The demigods get names for the HUD, starting stats and what they gain per level: Angel leans Arcana (spells),
  Ji-Woong leans Strength (sword) and Defense.
- Both get a level-up burst (gold column + charge-up sound).

The level badge / EXP bar / LEVEL UP banner need no asset: BP_PC creates them in C++ and puts them in W_PlayerHud.
Every asset saved is copied to Saved/MigrationBackups/<timestamp>/ first. Running it twice is safe.
"""
import os
import sys

import unreal

sys.path.insert(0, os.path.join(os.path.abspath(unreal.Paths.project_dir()), "Scripts", "Migration"))
from migration_common import EAL, bp_class, fx, load, log, set_props, warn, write_report  # noqa: E402

ANGEL = "/Game/WorldsBeyond/Characters/Angel/BP_Angel"
JIWOONG = "/Game/WorldsBeyond/Characters/Ji-Woong/BP_Ji-Woong"

ENEMY_RANKS = [
    ("/Game/Enemies/BP_Enemy_Base", "REGULAR"),
    ("/Game/Enemies/MeleeEnemy/BP_Enemy_Melee", "REGULAR"),
    ("/Game/Enemies/BP_Enemy_Ranged", "REGULAR"),
    ("/Game/Enemies/MageEnemy/BP_Enemy_Mage", "ELITE"),
    ("/Game/Enemies/BossEnemy/BP_Enemy_Boss", "BOSS"),
]

GOLD = (1.0, 0.78, 0.2, 1.0)


def growth(max_health, max_stamina, strength, arcana, defense):
    value = unreal.BeyondStatGrowth()
    value.set_editor_property("max_health", max_health)
    value.set_editor_property("max_stamina", max_stamina)
    value.set_editor_property("strength", strength)
    value.set_editor_property("arcana", arcana)
    value.set_editor_property("defense", defense)
    return value


def _reward(rank):
    try:
        return "%.0f" % unreal.BeyondProgressionSettings.get_experience_reward(rank, 1)
    except Exception:  # only used for the log
        return "?"


def step_enemy_ranks():
    for path, rank_name in ENEMY_RANKS:
        if not EAL.does_asset_exist(path):
            warn("%s not found; rank not set" % path)
            continue
        bp = load(path)
        if bp is None:
            continue
        rank = getattr(unreal.BeyondEnemyRank, rank_name)
        set_props(bp, path, rank=rank)
        log("%s: rank %s (%s EXP at level 1)" % (path, rank_name.title(), _reward(rank)))


def step_demigods():
    level_up = fx(system="/Game/VFX/Teleport/P_TeleportStart_Yellow", sound="/Game/Audio/energy-charge-up", scale=0.8,
                  color=GOLD)
    demigods = [
        # Spells: Arcana. A little sturdier each level, but less than Ji-Woong
        (ANGEL, "Angel", dict(base_strength=0.0, base_arcana=10.0, base_defense=4.0),
         growth(max_health=10.0, max_stamina=4.0, strength=1.0, arcana=3.0, defense=1.0)),
        # Sword: Strength, and the front-liner's Defense
        (JIWOONG, "Ji-Woong", dict(base_strength=10.0, base_arcana=0.0, base_defense=8.0),
         growth(max_health=14.0, max_stamina=5.0, strength=3.0, arcana=1.0, defense=2.0)),
    ]
    for path, name, stats, stat_growth in demigods:
        bp = load(path)
        if bp is None:
            warn("%s not found" % path)
            continue
        set_props(bp, path, display_name=unreal.Text(name), stat_growth=stat_growth, level_up_fx=level_up, **stats)
        log("%s: \"%s\", starting %s, per level +%.0f health +%.0f stamina +%.0f Strength +%.0f Arcana +%.0f Defense" % (
            path, name, ", ".join("%s %.0f" % (key.replace("base_", ""), value) for key, value in stats.items()),
            stat_growth.max_health, stat_growth.max_stamina, stat_growth.strength, stat_growth.arcana, stat_growth.defense))


def step_check_controller():
    """BP_PC inherits the level display from C++; report if it was cleared by hand."""
    pc_class = bp_class("/Game/WorldsBeyond/Blueprints/BP_PC")
    if pc_class is None:
        warn("BP_PC not found; check its Progress Widget Class by hand")
        return
    widget_class = unreal.get_default_object(pc_class).get_editor_property("progress_widget_class")
    if widget_class is None:
        warn("BP_PC -> UI|Progression -> Progress Widget Class is empty: the level display stays hidden")
    else:
        log("BP_PC shows the level display (%s)" % widget_class.get_name())


def main():
    log("pass 7")
    # The new C++ types only exist once the editor module is rebuilt; stop before touching any asset
    missing = [name for name in ("BeyondStatGrowth", "BeyondEnemyRank", "BeyondProgressionSettings", "BeyondProgressWidget")
               if not hasattr(unreal, name)]
    if missing:
        warn("the editor is running an old build of the WorldBeyond module (%s missing). Close the editor, build "
             "WorldBeyondEditor (Development Editor, Win64) in Rider until it says Build succeeded, then run this "
             "script again. Nothing was changed." % ", ".join(missing))
        write_report("last_run_pass7.txt")
        raise RuntimeError("WorldBeyond C++ not rebuilt: %s missing" % ", ".join(missing))
    step_enemy_ranks()
    step_demigods()
    step_check_controller()
    write_report("last_run_pass7.txt")


main()
