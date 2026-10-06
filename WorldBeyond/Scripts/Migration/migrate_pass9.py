"""
Worlds Beyond - pass 9: items, armor sets and loot (Plan 2).

Run with the editor closed, after building the C++:
    UnrealEditor-Cmd.exe <path>/WorldBeyond.uproject -run=pythonscript -script=<path>/Scripts/Migration/migrate_pass9.py -unattended -nosplash -NullRHI

- Three armor sets (/Game/WorldsBeyond/Items/Sets/): Venomweave (Venom Infusion), Stormforged (Chain Lightning) and
  Sunforged (Sunfire), four pieces each (Helm, Chest, Gauntlets, Boots; they drop at Rare or better).
- Twelve regular armor pieces in three lines: Wanderer's (balanced), Ironbound (Defense / health) and Acolyte's
  (Arcana / stamina); six weapons: swords for Ji-Woong (Traveler's Blade, Gilded Saber, Dawnbreaker) and staves for
  Angel (Ashwood Staff, Stormcaller's Rod, Eclipse Scepter).
- DA_ItemDatabase lists them all (the loot pool; Project Settings -> Worlds Beyond Loot points at it).
- F picks loot up (IA_Interact) and I opens the equipment & inventory screen (IA_Inventory). Ji-Woong's old Blueprint
  F event (it activated Blink, replaced by Gilded Step on Q) is switched off so it doesn't fire with the pickup.

Items are only created when missing, so editor tuning survives a re-run. To rebuild them from this script (asset
names stay the same, so saved items still load), run it with BEYOND_REBUILD_ITEMS=1.
Every asset saved is copied to Saved/MigrationBackups/<timestamp>/ first.
"""
import os
import sys

import unreal

sys.path.insert(0, os.path.join(os.path.abspath(unreal.Paths.project_dir()), "Scripts", "Migration"))
from migration_common import (ASSET_TOOLS, EAL, cdo, fx, load, log, make_key, map_keys, save, set_props, tag,  # noqa: E402
                              warn, write_report)

ITEMS = "/Game/WorldsBeyond/Items"
SETS = ITEMS + "/Sets"
ARMOR = ITEMS + "/Armor"
WEAPONS = ITEMS + "/Weapons"
DATABASE = ITEMS + "/DA_ItemDatabase"
JIWOONG = "/Game/WorldsBeyond/Characters/Ji-Woong/BP_Ji-Woong"
PC = "/Game/WorldsBeyond/Blueprints/BP_PC"

BUFFS = "/Game/ParagonMinions/FX/Particles/PlayerBuffs/"
FXV = "/Game/FXVarietyPack/Particles/"

REBUILD = os.environ.get("BEYOND_REBUILD_ITEMS", "") == "1"

S = unreal.BeyondSkillStat
SLOT = unreal.BeyondItemSlot
TIER = unreal.BeyondItemTier


def stat_list(**values):
    """stat_list(strength=3, max_health=10) -> [BeyondItemStat]"""
    names = {"max_health": S.MAX_HEALTH, "max_stamina": S.MAX_STAMINA, "strength": S.STRENGTH, "arcana": S.ARCANA,
             "defense": S.DEFENSE}
    result = []
    for name, value in values.items():
        item_stat = unreal.BeyondItemStat()
        item_stat.set_editor_property("stat", names[name])
        item_stat.set_editor_property("value", float(value))
        result.append(item_stat)
    return result


def data_asset(folder, name, cls):
    """(asset, fill): an existing asset (fill=False unless rebuilding) or a new one (fill=True)."""
    path = "%s/%s" % (folder, name)
    exists = EAL.does_asset_exist(path)
    if exists and not REBUILD:
        return load(path), False
    asset = load(path) if exists else ASSET_TOOLS.create_asset(name, folder, cls, unreal.DataAssetFactory())
    if asset is None:
        warn("could not create %s" % path)
        return None, False
    return asset, True


def timed_fx(lifetime, **kwargs):
    value = fx(**kwargs)
    value.set_editor_property("max_lifetime", float(lifetime))
    return value


# ---------------------------------------------------------------- sets

def make_set(name, title, color, two_piece, effect, description, settings):
    path = "%s/%s" % (SETS, name)
    armor_set, fill = data_asset(SETS, name, unreal.BeyondArmorSet)
    if armor_set is None:
        return None
    if not fill:
        log("kept existing %s (set BEYOND_REBUILD_ITEMS=1 to rebuild it)" % path)
        return armor_set
    armor_set.set_editor_property("set_name", unreal.Text(title))
    armor_set.set_editor_property("color", unreal.LinearColor(*color))
    armor_set.set_editor_property("two_piece_stats", two_piece)
    armor_set.set_editor_property("full_set_effect", effect)
    armor_set.set_editor_property("full_set_description", unreal.Text(description))
    armor_set.set_editor_property("effect", settings)
    save(armor_set, path)
    log("built %s: %s" % (path, description))
    return armor_set


def effect_settings(**props):
    settings = unreal.BeyondSetEffectSettings()
    for key, value in props.items():
        settings.set_editor_property(key, value)
    return settings


def step_sets():
    venom = make_set(
        "DA_Set_Venomweave", "Venomweave", (0.3, 1.0, 0.35, 1.0), stat_list(strength=5, arcana=5),
        unreal.BeyondSetEffect.VENOM_INFUSION,
        "Venom Infusion: using Q / E / R coats weapon and spells in venom for 6 s (12 s cooldown); infused hits poison "
        "for 4 s (8 damage a second).",
        effect_settings(infusion_duration=6.0, infusion_cooldown=12.0, damage_per_second=8.0, dot_duration=4.0,
                        aura_fx=timed_fx(6.0, system=BUFFS + "P_CarriedBuff_GreenBuff"),
                        target_fx=timed_fx(4.0, system="/Game/ParagonSevarog/FX/Particles/Abilities/Ultimate/FX/P_Ult_GreenFlames",
                                           scale=0.35)))
    storm = make_set(
        "DA_Set_Stormforged", "Stormforged", (0.3, 0.6, 1.0, 1.0), stat_list(arcana=8),
        unreal.BeyondSetEffect.CHAIN_LIGHTNING,
        "Chain Lightning: hits have a 25 % chance to arc lightning to up to 3 enemies within 6 m (20 damage each).",
        effect_settings(proc_chance=0.25, proc_cooldown=0.5, proc_damage=20.0, radius=600.0, max_targets=3,
                        proc_fx=timed_fx(1.5, system=FXV + "P_ky_lightning2", scale=0.6, color=(0.3, 0.6, 1.0, 1.0))))
    sun = make_set(
        "DA_Set_Sunforged", "Sunforged", (1.0, 0.72, 0.15, 1.0), stat_list(defense=10),
        unreal.BeyondSetEffect.SUNFIRE,
        "Sunfire: hits set enemies ablaze for 3 s (6 damage a second); above half health the wearer takes 15 % less damage.",
        effect_settings(damage_per_second=6.0, dot_duration=3.0, damage_taken_reduction=0.15, reduction_health_threshold=0.5,
                        target_fx=timed_fx(3.0, system="/Game/ParagonSerath/FX/Particles/Abilities/Ultimate/FX/P_Burning",
                                           scale=0.5)))
    return {"venom": venom, "storm": storm, "sun": sun}


# ---------------------------------------------------------------- items

def make_item(folder, name, title, description, slot, stats, min_tier=TIER.COMMON, max_tier=TIER.LEGENDARY,
              weight=1.0, armor_set=None, weapon_tag=None):
    path = "%s/%s" % (folder, name)
    item, fill = data_asset(folder, name, unreal.BeyondItemDefinition)
    if item is None or not fill:
        return item
    item.set_editor_property("display_name", unreal.Text(title))
    item.set_editor_property("description", unreal.Text(description))
    item.set_editor_property("slot", slot)
    item.set_editor_property("base_stats", stats)
    item.set_editor_property("min_tier", min_tier)
    item.set_editor_property("max_tier", max_tier)
    item.set_editor_property("drop_weight", float(weight))
    if armor_set is not None:
        item.set_editor_property("set", armor_set)
    if weapon_tag:
        item.set_editor_property("weapon_tag", tag(weapon_tag))
    save(item, path)
    return item


def step_items(sets):
    made = []

    def add(item):
        if item is not None:
            made.append(item)

    # Set pieces: Rare or better, a little rarer than regular gear
    set_pieces = {
        "venom": ("Venomweave", "Woven from serpent silk and nightshade; it remembers every poison.", {
            "Helm": stat_list(strength=2, arcana=2, max_health=8),
            "Chest": stat_list(strength=3, arcana=3, max_health=18),
            "Gauntlets": stat_list(strength=4, arcana=2),
            "Boots": stat_list(arcana=3, max_stamina=10)}),
        "storm": ("Stormforged", "Hammered on an anvil struck by lightning; it still hums.", {
            "Helm": stat_list(arcana=4, max_health=8),
            "Chest": stat_list(arcana=5, max_health=15, defense=2),
            "Gauntlets": stat_list(arcana=5),
            "Boots": stat_list(arcana=3, max_stamina=12)}),
        "sun": ("Sunforged", "Gilded plate that drinks the dawn and gives it back as fire.", {
            "Helm": stat_list(defense=4, max_health=12),
            "Chest": stat_list(defense=6, max_health=25),
            "Gauntlets": stat_list(strength=4, defense=2),
            "Boots": stat_list(defense=3, max_stamina=8)}),
    }
    slots = {"Helm": SLOT.HELM, "Chest": SLOT.CHEST, "Gauntlets": SLOT.GAUNTLETS, "Boots": SLOT.BOOTS}
    for key, (title, description, pieces) in set_pieces.items():
        for slot_name, stats in pieces.items():
            add(make_item(ARMOR, "DA_Item_%s%s" % (title, slot_name), "%s %s" % (title, slot_name), description,
                          slots[slot_name], stats, min_tier=TIER.RARE, weight=0.5, armor_set=sets.get(key)))

    # Regular armor lines
    lines = {
        "Wanderers": ("Wanderer's", "Road-worn and dependable.", TIER.COMMON, TIER.RARE, {
            "Helm": stat_list(max_health=10, defense=2),
            "Chest": stat_list(max_health=20, defense=3),
            "Gauntlets": stat_list(strength=2, arcana=2),
            "Boots": stat_list(max_stamina=10, defense=1)}),
        "Ironbound": ("Ironbound", "Heavy iron plate for those who stand in front.", TIER.COMMON, TIER.EPIC, {
            "Helm": stat_list(defense=4, max_health=12),
            "Chest": stat_list(defense=7, max_health=25),
            "Gauntlets": stat_list(defense=3, strength=2),
            "Boots": stat_list(defense=3, max_health=10)}),
        "Acolytes": ("Acolyte's", "Temple cloth stitched with quiet wards.", TIER.COMMON, TIER.EPIC, {
            "Helm": stat_list(arcana=4, max_stamina=6),
            "Chest": stat_list(arcana=5, max_stamina=12, max_health=10),
            "Gauntlets": stat_list(arcana=4),
            "Boots": stat_list(max_stamina=12, arcana=2)}),
    }
    for asset_prefix, (title, description, min_tier, max_tier, pieces) in lines.items():
        for slot_name, stats in pieces.items():
            add(make_item(ARMOR, "DA_Item_%s%s" % (asset_prefix, slot_name), "%s %s" % (title, slot_name), description,
                          slots[slot_name], stats, min_tier=min_tier, max_tier=max_tier))

    # Weapons: swords for Ji-Woong, staves for Angel
    sword, staff = "Weapon.Melee.Sword", "Weapon.Ranged.Staff"
    weapons = [
        ("DA_Item_TravelersBlade", "Traveler's Blade", "A plain, honest sword.", stat_list(strength=4), TIER.COMMON, TIER.RARE, 1.0, sword),
        ("DA_Item_GildedSaber", "Gilded Saber", "Light on the wrist, bright in the sun.", stat_list(strength=6, defense=2),
         TIER.UNCOMMON, TIER.EPIC, 0.8, sword),
        ("DA_Item_Dawnbreaker", "Dawnbreaker", "Said to have cut the first morning out of the night.",
         stat_list(strength=9, max_health=15), TIER.RARE, TIER.LEGENDARY, 0.5, sword),
        ("DA_Item_AshwoodStaff", "Ashwood Staff", "Cut from a tree that survived a lightning strike.", stat_list(arcana=4),
         TIER.COMMON, TIER.RARE, 1.0, staff),
        ("DA_Item_StormcallersRod", "Stormcaller's Rod", "Clouds gather when it is raised.", stat_list(arcana=6, max_stamina=8),
         TIER.UNCOMMON, TIER.EPIC, 0.8, staff),
        ("DA_Item_EclipseScepter", "Eclipse Scepter", "Half sun, half moon; wholly dangerous.", stat_list(arcana=9, max_health=15),
         TIER.RARE, TIER.LEGENDARY, 0.5, staff),
    ]
    for name, title, description, stats, min_tier, max_tier, weight, weapon_tag in weapons:
        add(make_item(WEAPONS, name, title, description, SLOT.WEAPON, stats, min_tier=min_tier, max_tier=max_tier,
                      weight=weight, weapon_tag=weapon_tag))

    log("%d items %s" % (len(made), "rebuilt" if REBUILD else "ready (existing ones kept)"))
    return made


def step_database(items):
    database, _ = data_asset(ITEMS, "DA_ItemDatabase", unreal.BeyondItemDatabase)
    if database is None:
        return None
    # Ours plus anything added in the editor
    listed = [item for item in database.get_editor_property("items") if item is not None]
    names = {item.get_path_name() for item in listed}
    added = 0
    for item in items:
        if item.get_path_name() not in names:
            listed.append(item)
            names.add(item.get_path_name())
            added += 1
    database.set_editor_property("items", listed)
    save(database, DATABASE)
    log("%s: %d items (%d added)" % (DATABASE, len(listed), added))

    settings = unreal.get_default_object(unreal.BeyondLootSettings.static_class())
    configured = settings.get_editor_property("item_database")
    if configured is None:
        log("loot settings: Item Database not loaded here; it should be %s (Project Settings -> Worlds Beyond Loot)" % DATABASE)
    elif not configured.get_path_name().startswith(DATABASE):
        warn("Project Settings -> Worlds Beyond Loot -> Item Database is '%s', not %s: no loot will drop from the new "
             "items until it points there" % (configured.get_path_name(), DATABASE))
    return database


# ---------------------------------------------------------------- input

def step_input():
    actions = map_keys("/Game/Input/IMC_Default", {"IA_Interact": "F", "IA_Inventory": "I"})
    pc = load(PC)
    if pc is None:
        warn("%s not found; set Interact Action and Inventory Action on your player controller by hand" % PC)
    else:
        set_props(pc, PC, interact_action=actions["IA_Interact"], inventory_action=actions["IA_Inventory"])
        widget = cdo(pc).get_editor_property("inventory_widget_class")
        log("%s: F picks loot up, I opens the equipment screen (%s)" % (PC, widget.get_name() if widget else "NO WIDGET CLASS"))
        if widget is None:
            warn("BP_PC -> UI|Inventory -> Inventory Widget Class is empty: I won't open anything")

    # Ji-Woong's Blueprint still has a raw F event (Blink); keep it from firing with the pickup
    jiwoong = load(JIWOONG)
    if jiwoong is None:
        return
    keys = list(cdo(jiwoong).get_editor_property("legacy_keys_to_disable"))
    names = [str(key.get_editor_property("key_name")) for key in keys]
    if "F" in names:
        log("%s: its Blueprint F event was already switched off" % JIWOONG)
        return
    keys.append(make_key("F"))
    set_props(jiwoong, JIWOONG, disable_legacy_key_input=True, legacy_keys_to_disable=keys)
    log("%s: Blueprint F event (Blink dash) switched off; legacy keys off: %s" % (JIWOONG, ", ".join(names + ["F"])))


def main():
    log("pass 9")
    # The new C++ types only exist once the editor module is rebuilt; stop before touching any asset
    missing = [name for name in ("BeyondItemDefinition", "BeyondArmorSet", "BeyondItemDatabase", "BeyondItemStat",
                                 "BeyondSetEffectSettings", "BeyondLootSettings", "BeyondInventoryWidget")
               if not hasattr(unreal, name)]
    if missing:
        warn("the editor is running an old build of the WorldBeyond module (%s missing). Close the editor, build "
             "WorldBeyondEditor (Development Editor, Win64) in Rider until it says Build succeeded, then run this "
             "script again. Nothing was changed." % ", ".join(missing))
        write_report("last_run_pass9.txt")
        raise RuntimeError("WorldBeyond C++ not rebuilt: %s missing" % ", ".join(missing))

    sets = step_sets()
    items = step_items(sets)
    step_database(items)
    step_input()
    write_report("last_run_pass9.txt")


main()
