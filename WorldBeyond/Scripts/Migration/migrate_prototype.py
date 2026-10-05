"""
Worlds Beyond - GAS prototype asset migration.

Run with the editor closed:
    UnrealEditor-Cmd.exe <path>/WorldBeyond.uproject -run=pythonscript -script=<path>/Scripts/Migration/migrate_prototype.py -unattended -nosplash -NullRHI

Every asset this script saves is copied to Saved/MigrationBackups/<timestamp>/ first.
Each step checks the current state, so running it twice is safe.
"""
import os
import sys

import unreal

sys.path.insert(0, os.path.join(os.path.abspath(unreal.Paths.project_dir()), "Scripts", "Migration"))
from migration_common import (ASSET_TOOLS, BACKUP_DIR, BEL, EAL, bp_class, cdo, ensure_blueprint,  # noqa: E402
                              load, log, map_keys, reparent, save, set_props, tag, tag_container,
                              write_report)


# ---------------------------------------------------------------- steps

def step_input():
    """New ability slots on free keys (Q/E/R), right mouse cancels targeting."""
    return map_keys("/Game/Input/IMC_Default", {
        "IA_AbilityQ": "Q",
        "IA_AbilityE": "E",
        "IA_AbilityR": "R",
        "IA_TargetCancel": "RightMouseButton",
    })


def step_fix_nexus_effects():
    """Point the migrated effects at this project's attributes."""
    template = cdo(load("/Game/GameplayAbilitySystem/Effects/GE_Damage_Instant")) \
        .get_editor_property("modifiers")[0].get_editor_property("attribute").export_text()
    renames = {"Health": "CurrentHealth", "Stamina": "CurrentStamina"}
    for path in ["/Game/GameplayAbilitySystem/Effects/GE_Dash_Cost",
                 "/Game/GameplayAbilitySystem/Effects/GE_DamageOverTime_Infinite",
                 "/Game/GameplayAbilitySystem/Effects/GE_HealOverTime_Infinite",
                 "/Game/GameplayAbilitySystem/Effects/GE_Status_StaminaRegen"]:
        bp = load(path)
        if bp is None:
            continue
        effect = cdo(bp)
        mods = list(effect.get_editor_property("modifiers"))
        changed = False
        for i, mod in enumerate(mods):
            attr = mod.get_editor_property("attribute")
            old_name = str(attr.get_editor_property("attribute_name"))
            if old_name in renames:
                # The attribute field is read-only from Python, so rebuild the whole modifier from text
                old_attr_text = attr.export_text()
                new_attr_text = template.replace("CurrentHealth", renames[old_name])
                new_mod = unreal.GameplayModifierInfo()
                new_mod.import_text(mod.export_text().replace(old_attr_text, new_attr_text))
                mods[i] = new_mod
                log("%s: modifier %s -> %s" % (path, old_name, str(new_mod.get_editor_property("attribute").get_editor_property("attribute_name"))))
                changed = True
        if changed:
            effect.set_editor_property("modifiers", mods)
            save(bp, path)


def step_abilities():
    """Existing abilities become BeyondGameplayAbility so they get input slots and AI hints."""
    wb = "/Game/WorldsBeyond/Blueprints/Gameplay_Abilites/Abilites/"
    gas = "/Game/GameplayAbilitySystem/Abilities/"
    E = unreal.BeyondAITargeting

    settings = {
        wb + "GA_Blink": dict(input_tag=tag("Ability.Input.Q"), ai_usable=False),
        wb + "GA_HealSpell": dict(input_tag=tag("Ability.Input.R"), ai_usable=True, ai_targeting=E.SELF,
                                   ai_use_below_health_percent=0.45, ai_weight=4.0),
        # Uses a targeting actor that asserts without a PlayerController, so players only
        wb + "GA_ShootProjectile": dict(ai_usable=False),
        wb + "GA_EquipWeapon": dict(ai_usable=False),
        gas + "GA_AOEAttack": dict(input_tag=tag("Ability.Input.E"), ai_usable=False),
        gas + "GA_Dash": dict(input_tag=tag("Ability.Input.E"), ai_usable=False),
        gas + "GA_HitReaction": dict(ai_usable=False),
        gas + "GA_Death": dict(ai_usable=False),
        gas + "GA_EquipWeapon": dict(ai_usable=False),
    }
    for path, props in settings.items():
        bp = reparent(path, unreal.BeyondGameplayAbility)
        if bp:
            set_props(bp, path, **props)

    # Abilities the companion can use: C++ ability classes configured with the demigods' existing assets
    angel_bolt_path = "/Game/WorldsBeyond/Abilities/Angel/GA_Angel_ArcaneBolt"
    bolt = ensure_blueprint(angel_bolt_path, unreal.BeyondGA_Projectile)
    set_props(bolt, angel_bolt_path,
              projectile_class=bp_class("/Game/Projectiles/BP_Projectile_GreenFire"),
              cast_montage=load("/Game/EssentialAnimation/MagicStaff/Animation/UE5/Sequence/Attack/UE5_BM_Attack_08_Seq_Montage"),
              damage=25.0, projectile_speed=2200.0, hit_response=tag("Event.Hit.Light"),
              ai_min_range=250.0, ai_max_range=1800.0, ai_weight=3.0,
              activation_owned_tags=tag_container("Ability.Active"),
              activation_blocked_tags=tag_container("Ability.Active"))

    combo_path = "/Game/WorldsBeyond/Abilities/JiWoong/GA_JiWoong_SwordCombo"
    combo = ensure_blueprint(combo_path, unreal.BeyondGA_MeleeCombo)
    step = unreal.BeyondComboStep()
    step.set_editor_property("montage", load("/Game/Animations/Melee/Montage_SwordCombo"))
    step.set_editor_property("damage", 22.0)
    step.set_editor_property("hit_response", tag("Event.Hit.Light"))
    step.set_editor_property("play_rate", 1.0)
    set_props(combo, combo_path, combo_steps=[step], ai_max_range=220.0, ai_weight=3.0,
              activation_owned_tags=tag_container("Ability.Active"),
              activation_blocked_tags=tag_container("Ability.Active"))
    return bolt, combo


def ensure_ability_set(path, entries, effects):
    folder, name = path.rsplit("/", 1)
    if EAL.does_asset_exist(path):
        # Later passes (migrate_pass2.py) own the contents; don't put the first-pass abilities back
        return load(path)
    data = ASSET_TOOLS.create_asset(name, folder, unreal.BeyondAbilitySet, unreal.DataAssetFactory())
    log("created ability set %s" % path)
    abilities = []
    for ability_class, input_tag in entries:
        entry = unreal.BeyondAbilitySet_Ability()
        entry.set_editor_property("ability", ability_class)
        entry.set_editor_property("level", 1)
        if input_tag:
            entry.set_editor_property("input_tag", tag(input_tag))
        abilities.append(entry)
    effect_entries = []
    for effect_class in effects:
        e = unreal.BeyondAbilitySet_Effect()
        e.set_editor_property("effect", effect_class)
        e.set_editor_property("level", 1.0)
        effect_entries.append(e)
    data.set_editor_property("abilities", abilities)
    data.set_editor_property("effects", effect_entries)
    save(data, path)
    return data


def step_characters(actions, bolt, combo):
    """Demigods: team, ability sets, input slots, swap sounds."""
    angel_set = ensure_ability_set("/Game/WorldsBeyond/Abilities/DA_AbilitySet_Angel",
                                   [(bolt.generated_class(), None),
                                    (bp_class("/Game/GameplayAbilitySystem/Abilities/GA_AOEAttack"), "Ability.Input.E")],
                                   [])
    jiwoong_set = ensure_ability_set("/Game/WorldsBeyond/Abilities/DA_AbilitySet_JiWoong",
                                     [(combo.generated_class(), None),
                                      (bp_class("/Game/GameplayAbilitySystem/Abilities/GA_Dash"), "Ability.Input.E")],
                                     # Angel's Blueprint handles stamina regen itself; Ji-Woong never had it
                                     [bp_class("/Game/WorldsBeyond/Blueprints/Gameplay_Abilites/Effects/GE_Status_StaminaRegen")])

    def bindings(bp):
        slots = (("IA_AbilityQ", "Ability.Input.Q"), ("IA_AbilityE", "Ability.Input.E"), ("IA_AbilityR", "Ability.Input.R"))
        # Keep slots added by later passes (e.g. Ability.Input.Duo)
        result = [b for b in cdo(bp).get_editor_property("ability_input_bindings")
                  if str(b.get_editor_property("input_tag").get_editor_property("tag_name")) not in {s for _, s in slots}]
        for action_name, slot in slots:
            b = unreal.BeyondInputBinding()
            b.set_editor_property("input_action", actions[action_name])
            b.set_editor_property("input_tag", tag(slot))
            result.append(b)
        return result

    for path, ability_set, sound in (
            ("/Game/WorldsBeyond/Characters/Angel/BP_Angel", angel_set, "/Game/WorldsBeyond/Sounds/Angel/Angel_Switch"),
            ("/Game/WorldsBeyond/Characters/Ji-Woong/BP_Ji-Woong", jiwoong_set, "/Game/WorldsBeyond/Sounds/JI-Woong/JI-Woong_Switch")):
        bp = load(path)
        if bp is None:
            continue
        set_props(bp, path,
                  team_affiliation=unreal.BeyondTeam.PLAYER,
                  ability_set=ability_set,
                  ability_input_bindings=bindings(bp),
                  cancel_target_action=actions["IA_TargetCancel"],
                  swap_in_sound=load(sound),
                  max_attack_tokens=2)
        log("configured %s" % path)


def step_enemies():
    """Enemies get an ability system: GAS owns their health, the old component mirrors it."""
    path = "/Game/Enemies/BP_Enemy_Base"
    bp = reparent(path, unreal.BeyondCharacterBase)
    if bp:
        set_props(bp, path, team_affiliation=unreal.BeyondTeam.ENEMY, destroy_delay_after_death=15.0)
    # Recompile children against the new parent
    for child in ["/Game/Enemies/MeleeEnemy/BP_Enemy_Melee", "/Game/Enemies/MageEnemy/BP_Enemy_Mage",
                  "/Game/Enemies/BossEnemy/BP_Enemy_Boss", "/Game/Enemies/BP_Enemy_Ranged"]:
        child_bp = load(child)
        if child_bp:
            ok = BEL.compile_blueprint(child_bp)
            save(child_bp, child)
            log("recompiled %s (compiles=%s)" % (child, ok))


def step_weapons():
    for path in ["/Game/WorldsBeyond/Weapons/BP_Weapon_Base", "/Game/Weapons/BP_Weapon_Base"]:
        bp = reparent(path, unreal.BeyondWeapon)
        if bp:
            save(bp, path)


def step_game_framework():
    pc_path = "/Game/WorldsBeyond/Blueprints/BP_PC"
    pc = reparent(pc_path, unreal.BeyondPlayerController)
    if pc:
        set_props(pc, pc_path,
                  default_mapping_contexts=[load("/Game/Input/IMC_Default"), load("/Game/Input/IMC_MouseLook")],
                  swap_action=load("/Game/Input/Actions/IA_Swap"),
                  hud_widget_class=bp_class("/Game/WorldsBeyond/Blueprints/Widgets/PlayerHud/W_PlayerHud"))

    gm_path = "/Game/ThirdPerson/Blueprints/BP_ThirdPersonGameMode"
    gm = reparent(gm_path, unreal.BeyondGameMode)
    if gm and pc:
        set_props(gm, gm_path, player_controller_class=pc.generated_class())


def main():
    log("backups -> %s" % BACKUP_DIR)
    actions = step_input()
    step_fix_nexus_effects()
    bolt, combo = step_abilities()
    step_weapons()
    step_characters(actions, bolt, combo)
    step_enemies()
    step_game_framework()
    write_report("last_run.txt")


main()
