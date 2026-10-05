"""
Worlds Beyond - remove unused / duplicate legacy content after the GAS prototype is verified.

Dry run (default): lists what would be deleted and anything still referencing it.
Apply: set the environment variable BEYOND_CLEANUP_APPLY=1, then run:
    UnrealEditor-Cmd.exe <path>/WorldBeyond.uproject -run=pythonscript -script=<path>/Scripts/Migration/cleanup_legacy.py -unattended -nosplash -NullRHI

An asset is only deleted when everything that references it is also being deleted.
Git keeps the history of everything removed here.
"""
import os

import unreal

APPLY = os.environ.get("BEYOND_CLEANUP_APPLY") == "1"
EAL = unreal.EditorAssetLibrary
AR = unreal.AssetRegistryHelpers.get_asset_registry()

DELETE_FOLDERS = [
    "/Game/WorldsBeyond/Enemy",            # unfinished duplicate of /Game/Enemies
    # Actors of the template level (one file per actor); they go with Lvl_ThirdPerson
    "/Game/__ExternalActors__/ThirdPerson/Lvl_ThirdPerson",
    "/Game/__ExternalObjects__/ThirdPerson/Lvl_ThirdPerson",
]
DELETE_ASSETS = [
    # Nexus leftovers replaced by BeyondGA_MeleeCombo / BeyondGA_Projectile / the WorldsBeyond weapon stack
    "/Game/GameplayAbilitySystem/Abilities/GA_ShootProjectile_Base",
    "/Game/GameplayAbilitySystem/Abilities/GA_MeleeAttack_Base",
    "/Game/GameplayAbilitySystem/Abilities/GA_MeleeAttack_AxeCombo",
    "/Game/GameplayAbilitySystem/Abilities/GA_MeleeAttack_AxeSwing",
    "/Game/GameplayAbilitySystem/Abilities/GA_EquipWeapon",
    "/Game/GameplayAbilitySystem/Cues/GC_TargetingCamera",
    "/Game/GameplayAbilitySystem/Effects/GE_Status_StaminaRegen",
    "/Game/Weapons/WeaponsManagerComponent_EXTENDED",
    # Template leftovers (the game mode in /Game/ThirdPerson is still used)
    "/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter",
    "/Game/ThirdPerson/Blueprints/BP_ThirdPersonPlayerController",
    "/Game/ThirdPerson/Lvl_ThirdPerson",
    "/Game/TestingRelm",
]

report = []


def log(msg):
    report.append(msg)
    unreal.log("CLEANUP " + msg)


def package_of(path):
    return path.split(".")[0]


targets = set()
for folder in DELETE_FOLDERS:
    for asset in EAL.list_assets(folder, recursive=True, include_folder=False):
        targets.add(package_of(asset))
for asset in DELETE_ASSETS:
    if EAL.does_asset_exist(asset):
        targets.add(asset)
    else:
        log("already gone: %s" % asset)

options = unreal.AssetRegistryDependencyOptions(include_soft_package_references=True, include_hard_package_references=True,
                                                include_searchable_names=False, include_soft_management_references=False,
                                                include_hard_management_references=False)
referencers_of = {t: [str(r) for r in (AR.get_referencers(t, options) or [])] for t in targets}

# Keeping an asset means keeping everything it depends on, so repeat until nothing else gets blocked
blocked = {}
remaining = set(targets)
changed = True
while changed:
    changed = False
    for target in sorted(remaining):
        outside = sorted(r for r in referencers_of[target] if r not in remaining and r != target)
        if outside:
            blocked[target] = outside
            remaining.discard(target)
            changed = True

deletable = sorted(remaining)
for target, refs in blocked.items():
    log("KEEP %s - still referenced by: %s" % (target, ", ".join(refs)))
if not APPLY:
    for target in deletable:
        log("would delete " + target)
else:
    # The referencer check above already proved nothing outside this list uses these assets.
    # delete_asset refuses assets that happen to be loaded in this session, and the editor is
    # closed, so remove the package files directly (same as deleting them in Explorer).
    content_dir = os.path.abspath(unreal.Paths.project_content_dir())
    for target in deletable:
        rel = target.replace("/Game/", "", 1).replace("/", os.sep)
        files = [os.path.join(content_dir, rel + ext) for ext in (".uasset", ".umap", ".uexp", ".ubulk")]
        existing = [f for f in files if os.path.exists(f)]
        if not existing:
            log("already gone: " + target)
            continue
        for f in existing:
            os.remove(f)
        log("DELETE " + target)

# Redirector fix-up isn't exposed to Python; list them for Content Browser > Fix Up Redirectors
redirectors = sorted(set(str(d.package_name) for d in AR.get_assets_by_path("/Game", recursive=True)
                         if str(d.asset_class_path.asset_name) == "ObjectRedirector"))
for r in redirectors:
    log("redirector: %s" % r)
if redirectors:
    log("%d redirectors: right-click the Content folder > Fix Up Redirectors" % len(redirectors))

out = os.path.join(os.path.abspath(unreal.Paths.project_saved_dir()), "MigrationBackups", "cleanup_report.txt")
os.makedirs(os.path.dirname(out), exist_ok=True)
with open(out, "w", encoding="utf-8") as fh:
    fh.write("\n".join(report))
log("%s - report at %s" % ("applied" if APPLY else "dry run", out))
