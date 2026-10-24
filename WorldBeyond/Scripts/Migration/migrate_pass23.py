"""
Worlds Beyond - pass 23: the UI overhaul's content (Plan 6, phase 5).

Run with the editor closed, after building the C++ and running pass 21 (the art):
    UnrealEditor-Cmd.exe <path>/WorldBeyond.uproject -run=pythonscript -script=<path>/Scripts/Migration/migrate_pass23.py -unattended -nosplash -NullRHI

1. Rings (Accessory) and amulets (Charm): eight new items in /Game/WorldsBeyond/Items/Accessories/ with their icons,
   added to DA_ItemDatabase (so they drop). Made only when missing (BEYOND_REBUILD_ITEMS=1 rebuilds them).
2. Both demigods start with a 5 % Critical Rate (only while it is still 0).
3. The skill trees: new nodes appended by id (Angel: Keen Static, Storm's Embrace, Elemental Resonance, Raging Momentum;
   Ji-Woong: Radiant Zeal, Executioner's Edge, Sunlit Momentum; duo: Resonance at Bond level 3, Concord at Bond level 5)
   with their icons; the trees laid out again in six rows (ids, prerequisites and ranks unchanged, so saves are safe;
   BEYOND_KEEP_TREE_LAYOUT=1 leaves positions alone); element pills, keystone diamonds and one-line summaries where
   a node has none yet.
4. Ability names and descriptions for the menus (only where empty).
5. /Game/WorldsBeyond/Quests/DA_QuestDatabase from quest_content.py: quests missing by id are added, the rest kept
   (BEYOND_REBUILD_QUESTS=1 makes them all again).

Every asset saved is copied to Saved/MigrationBackups/<timestamp>/ first. Report: last_run_pass23.txt.
"""
import importlib
import os
import sys

import unreal

sys.path.insert(0, os.path.join(os.path.abspath(unreal.Paths.project_dir()), "Scripts", "Migration"))
from migration_common import ASSET_TOOLS, EAL, bp_class, cdo, load, log, save, set_props, warn, write_report  # noqa: E402
import quest_content  # noqa: E402

importlib.reload(quest_content)

ITEMS = "/Game/WorldsBeyond/Items"
ACCESSORIES = ITEMS + "/Accessories"
DATABASE = ITEMS + "/DA_ItemDatabase"
TREES = "/Game/WorldsBeyond/SkillTrees"
QUESTS = "/Game/WorldsBeyond/Quests"
QUEST_DATABASE = QUESTS + "/DA_QuestDatabase"
UI = "/Game/WorldsBeyond/UI"
ANGEL = "/Game/WorldsBeyond/Characters/Angel/BP_Angel"
JIWOONG = "/Game/WorldsBeyond/Characters/Ji-Woong/BP_Ji-Woong"
ABILITIES = "/Game/WorldsBeyond/Abilities/"
BP_ABILITIES = "/Game/WorldsBeyond/Blueprints/Gameplay_Abilites/Abilites/"

REBUILD_ITEMS = os.environ.get("BEYOND_REBUILD_ITEMS", "") == "1"
REBUILD_QUESTS = os.environ.get("BEYOND_REBUILD_QUESTS", "") == "1"
KEEP_LAYOUT = os.environ.get("BEYOND_KEEP_TREE_LAYOUT", "") == "1"

S = unreal.BeyondSkillStat
E = unreal.BeyondSkillEffectType
EL = unreal.BeyondElement
SLOT = unreal.BeyondItemSlot
TIER = unreal.BeyondItemTier


def texture(path):
    return load(path) if EAL.does_asset_exist(path) else None


def data_asset(folder, name, cls, rebuild):
    """(asset, fill): an existing asset (fill only when rebuilding) or a new one."""
    path = "%s/%s" % (folder, name)
    exists = EAL.does_asset_exist(path)
    if exists and not rebuild:
        return load(path), False
    asset = load(path) if exists else ASSET_TOOLS.create_asset(name, folder, cls, unreal.DataAssetFactory())
    if asset is None:
        warn("could not create %s" % path)
        return None, False
    return asset, True


# ---------------------------------------------------------------- 1. rings and amulets

STAT_NAMES = {
    "max_health": S.MAX_HEALTH, "max_stamina": S.MAX_STAMINA, "strength": S.STRENGTH, "arcana": S.ARCANA, "defense": S.DEFENSE,
    "crit_rate": S.CRIT_RATE, "crit_damage": S.CRIT_DAMAGE, "mastery": S.ELEMENTAL_MASTERY, "verdant": S.VERDANT_AFFINITY,
    "shadow": S.SHADOW_AFFINITY, "frost": S.FROST_AFFINITY, "flame": S.FLAME_AFFINITY,
}

# name, title, description, slot, stats at item level 1 / Common
ACCESSORY_ITEMS = [
    ("BandOfStatic", "Band of Static", "A copper ring that never stops crackling. Spells land where they hurt most.",
     SLOT.ACCESSORY, {"crit_rate": 2, "arcana": 2}),
    ("EmberglassRing", "Emberglass Ring", "Glass from the caldera, still warm. The fire knows its own.",
     SLOT.ACCESSORY, {"flame": 5, "strength": 2}),
    ("RimefrostLoop", "Rimefrost Loop", "Frost never melts from it, and frost never bites its wearer.",
     SLOT.ACCESSORY, {"frost": 5, "defense": 2}),
    ("SunstoneSignet", "Sunstone Signet", "The seal of a sun-priest. Its blows land like judgment.",
     SLOT.ACCESSORY, {"crit_damage": 8, "strength": 2}),
    ("EclipseTalisman", "Eclipse Talisman", "Moon and sun on one cord: the elements answer whoever wears it.",
     SLOT.CHARM, {"mastery": 10, "max_health": 10}),
    ("ShadowveilPendant", "Shadowveil Pendant", "A veil of the corrupted woods, turned against them.",
     SLOT.CHARM, {"shadow": 5, "max_stamina": 6}),
    ("StormglassAmulet", "Stormglass Amulet", "A captured bolt behind glass. It hums before a storm.",
     SLOT.CHARM, {"mastery": 8, "arcana": 3}),
    ("VerdantCharm", "Verdant Charm", "A seed of the first forest, wrapped in living bark.",
     SLOT.CHARM, {"verdant": 5, "max_health": 15}),
]


def stat_list(values):
    result = []
    for name, value in values.items():
        item_stat = unreal.BeyondItemStat()
        item_stat.set_editor_property("stat", STAT_NAMES[name])
        item_stat.set_editor_property("value", float(value))
        result.append(item_stat)
    return result


def step_items():
    made = []
    for name, title, description, slot, stats in ACCESSORY_ITEMS:
        asset_name = "DA_Item_" + name
        path = "%s/%s" % (ACCESSORIES, asset_name)
        item, fill = data_asset(ACCESSORIES, asset_name, unreal.BeyondItemDefinition, REBUILD_ITEMS)
        if item is None:
            continue
        if fill:
            item.set_editor_property("display_name", unreal.Text(title))
            item.set_editor_property("description", unreal.Text(description))
            item.set_editor_property("slot", slot)
            item.set_editor_property("base_stats", stat_list(stats))
            item.set_editor_property("min_tier", TIER.UNCOMMON)
            item.set_editor_property("max_tier", TIER.LEGENDARY)
            item.set_editor_property("drop_weight", 0.8)
        icon = texture("%s/Icons/Items/T_Icon_Item_%s" % (UI, name))
        if icon is not None and item.get_editor_property("icon") != icon:
            item.set_editor_property("icon", icon)
            fill = True
        elif icon is None:
            warn("%s: no icon T_Icon_Item_%s yet (run pass 21)" % (asset_name, name))
        if fill:
            save(item, path)
        made.append(item)

    database = load(DATABASE)
    if database is None:
        warn("no %s (run pass 9 first)" % DATABASE)
        return
    listed = [item for item in database.get_editor_property("items") if item is not None]
    names = {item.get_path_name() for item in listed}
    added = [item for item in made if item.get_path_name() not in names]
    if added:
        database.set_editor_property("items", listed + added)
        save(database, DATABASE)
    log("rings and amulets: %d (%d new in the item database)" % (len(made), len(added)))


# ---------------------------------------------------------------- 2. base critical rate

def step_base_crit():
    for path in (ANGEL, JIWOONG):
        blueprint = load(path)
        if blueprint is None:
            warn("no %s" % path)
            continue
        if cdo(blueprint).get_editor_property("base_crit_rate") <= 0.0:
            set_props(blueprint, path, base_crit_rate=5.0)
            log("%s: Critical Rate 5 %%" % path.rsplit("/", 1)[-1])


# ---------------------------------------------------------------- 3. skill trees

def effect(kind, value=0.0, stat=None, ability=None, element=None, trigger=None, action=None, duration=None, cooldown=None):
    e = unreal.BeyondSkillEffect()
    e.set_editor_property("type", kind)
    e.set_editor_property("value", float(value))
    for prop, item in (("stat", stat), ("ability", ability), ("element", element), ("trigger", trigger), ("action", action)):
        if item is not None:
            e.set_editor_property(prop, item)
    if duration is not None:
        e.set_editor_property("duration", float(duration))
    if cooldown is not None:
        e.set_editor_property("internal_cooldown", float(cooldown))
    return e


def passive(trigger, action, value, stat=None, element=None, duration=None, cooldown=None):
    return effect(E.PASSIVE, value, stat=stat, element=element, trigger=getattr(unreal.BeyondPassiveTrigger, trigger),
                  action=getattr(unreal.BeyondPassiveAction, action), duration=duration, cooldown=cooldown)


def new_node(node_id, name, description, summary, effects, max_rank=1, cost=1, level=1, requires=(), element=None, bond_level=0):
    n = unreal.BeyondSkillNode()
    n.set_editor_property("id", node_id)
    n.set_editor_property("display_name", unreal.Text(name))
    n.set_editor_property("description", unreal.Text(description))
    n.set_editor_property("summary", unreal.Text(summary))
    n.set_editor_property("max_rank", max_rank)
    n.set_editor_property("cost", cost)
    n.set_editor_property("required_level", level)
    n.set_editor_property("requires", list(requires))
    n.set_editor_property("effects", effects)
    n.set_editor_property("required_bond_level", bond_level)
    if element is not None:
        n.set_editor_property("element", element)
    return n


def angel_nodes():
    return [
        new_node("Angel.KeenStatic", "Keen Static", "The charge finds the weak point.", "Critical Rate +3% per rank",
                 [effect(E.STAT, 3, stat=S.CRIT_RATE)], max_rank=3, level=5, requires=["Angel.Bolt"], element=EL.STORM),
        new_node("Angel.StormsEmbrace", "Storm's Embrace", "He stops fighting the storm and lets it carry him.",
                 "Increases storm damage by 20%", [effect(E.ELEMENT_DAMAGE, 0.1, element=EL.STORM)], max_rank=2, level=8,
                 requires=["Angel.KeenStatic"], element=EL.STORM),
        new_node("Angel.ElementalResonance", "Elemental Resonance", "Every bolt that lands winds the next one tighter.",
                 "Storm hits shorten every cooldown", [passive("ON_ELEMENTAL_HIT", "REDUCE_COOLDOWNS", 0.5, element=EL.STORM, cooldown=3.0)],
                 max_rank=2, cost=2, level=16, requires=["Angel.Overcharge"], element=EL.STORM),
        new_node("Angel.RagingMomentum", "Raging Momentum", "A clean strike feeds the storm behind it.",
                 "Critical hits raise Magic Power for 6s", [passive("ON_CRIT", "BUFF_STAT", 6, stat=S.ARCANA, duration=6.0, cooldown=10.0)],
                 max_rank=2, cost=2, level=16, requires=["Angel.Vitality2"], element=EL.STORM),
    ]


def jiwoong_nodes():
    return [
        new_node("JiWoong.RadiantZeal", "Radiant Zeal", "His hexes burn with the sun's own conviction.", "Increases radiant damage by 20%",
                 [effect(E.ELEMENT_DAMAGE, 0.1, element=EL.RADIANT)], max_rank=2, level=8, requires=["JiWoong.Hex"], element=EL.RADIANT),
        new_node("JiWoong.ExecutionersEdge", "Executioner's Edge", "The blade remembers every opening it was given.",
                 "Critical Damage +15% per rank", [effect(E.STAT, 15, stat=S.CRIT_DAMAGE)], max_rank=2, cost=2, level=14,
                 requires=["JiWoong.Strength2"]),
        new_node("JiWoong.SunlitMomentum", "Sunlit Momentum", "Each foe that falls gives back a little light.",
                 "Kills heal 5% of max health per rank", [passive("ON_KILL", "HEAL", 5, cooldown=1.0)], max_rank=2, cost=2, level=14,
                 requires=["JiWoong.Vitality2"], element=EL.RADIANT),
    ]


def duo_nodes():
    judgment = bp_class(ABILITIES + "Duo/GA_Duo_HeavensJudgment")
    eclipse = bp_class(ABILITIES + "Duo/GA_Duo_EclipseBrand")
    return [
        new_node("Duo.Resonance", "Resonance", "Storm and sun strike as one: both duo powers hit harder.",
                 "Heaven's Judgment and Eclipse Brand +1 rank", [effect(E.ABILITY_RANK, ability=judgment), effect(E.ABILITY_RANK, ability=eclipse)],
                 cost=2, level=12, requires=["Duo.Echo"], element=EL.ECLIPSE, bond_level=3),
        new_node("Duo.Concord", "Concord", "Two rhythms become one: the meter fills faster and keeps more after a duo move.",
                 "Bond meter +10% faster, keeps 10% more", [effect(E.BOND_GAIN, 0.1), effect(E.BOND_ECHO, 0.1)],
                 cost=2, level=15, requires=["Duo.EyeOfTempest"], element=EL.ECLIPSE, bond_level=5),
    ]


# Six rows, columns -2 .. 2: prerequisites run straight down where they can
LAYOUT = {
    "DA_SkillTree_Angel": {
        "Angel.Arcana1": (-1, 0), "Angel.Vitality1": (1, 0),
        "Angel.Spikes": (-2, 1), "Angel.Bolt": (0, 1), "Angel.Stamina": (2, 1),
        "Angel.Arcana2": (-1, 2), "Angel.KeenStatic": (0, 2), "Angel.Ward": (1, 2),
        "Angel.Overcharge": (-2, 3), "Angel.StormsEmbrace": (0, 3), "Angel.Vitality2": (2, 3),
        "Angel.ElementalResonance": (-2, 4), "Angel.RagingMomentum": (2, 4),
        "Angel.Tempest": (0, 5),
    },
    "DA_SkillTree_JiWoong": {
        "JiWoong.Strength1": (-1, 0), "JiWoong.Vitality1": (1, 0),
        "JiWoong.Combo": (-2, 1), "JiWoong.Brand": (0, 1), "JiWoong.Stamina": (2, 1),
        "JiWoong.Step": (-1, 2), "JiWoong.Hex": (0, 2), "JiWoong.Guard": (1, 2),
        "JiWoong.Strength2": (-2, 3), "JiWoong.RadiantZeal": (1, 3), "JiWoong.Vitality2": (2, 3),
        "JiWoong.ExecutionersEdge": (-2, 4), "JiWoong.SunlitMomentum": (2, 4),
        "JiWoong.Judgment": (0, 5),
    },
    "DA_SkillTree_Duo": {
        "Duo.Kindred": (0, 0),
        "Duo.Wrath": (-1.5, 1), "Duo.Eclipse": (0, 1), "Duo.Aegis": (1.5, 1),
        "Duo.Echo": (-1.5, 2), "Duo.TotalEclipse": (0, 2), "Duo.EyeOfTempest": (1.5, 2),
        "Duo.Resonance": (-1.5, 3), "Duo.SoulBound": (0, 3), "Duo.Concord": (1.5, 3),
    },
}
# Element pills and keystones for the nodes from pass 8
ELEMENTS = {
    "Angel.Arcana1": EL.STORM, "Angel.Spikes": EL.STORM, "Angel.Bolt": EL.STORM, "Angel.Arcana2": EL.STORM, "Angel.Overcharge": EL.STORM,
    "Angel.Tempest": EL.STORM, "JiWoong.Brand": EL.RADIANT, "JiWoong.Step": EL.RADIANT, "JiWoong.Hex": EL.RADIANT,
    "JiWoong.Judgment": EL.RADIANT, "Duo.Wrath": EL.ECLIPSE, "Duo.Eclipse": EL.ECLIPSE, "Duo.Aegis": EL.ECLIPSE,
    "Duo.TotalEclipse": EL.ECLIPSE, "Duo.EyeOfTempest": EL.ECLIPSE,
}
KEYSTONES = {"Angel.Tempest", "JiWoong.Judgment", "Duo.SoulBound"}
SUMMARIES = {
    "Angel.Arcana1": "Magic Power +4 per rank", "Angel.Vitality1": "Max Health +12 per rank", "Angel.Stamina": "Max Stamina +10 per rank",
    "Angel.Arcana2": "Magic Power +8 per rank", "Angel.Ward": "Defense +4 per rank", "Angel.Tempest": "Angel becomes the storm",
    "JiWoong.Strength1": "Attack Power grows with every rank", "JiWoong.Vitality1": "Max Health grows with every rank",
    "JiWoong.Hex": "Brands and shockwaves grow with Magic Power", "JiWoong.Guard": "Defense +6 per rank",
    "Duo.Kindred": "The Bond meter fills 15% faster per rank", "Duo.Echo": "The meter keeps 15% after a duo move",
    "Duo.SoulBound": "The Bond meter fills 25% faster",
}


def step_trees():
    extra = {"DA_SkillTree_Angel": angel_nodes(), "DA_SkillTree_JiWoong": jiwoong_nodes(), "DA_SkillTree_Duo": duo_nodes()}
    for tree_name, added_nodes in extra.items():
        path = "%s/%s" % (TREES, tree_name)
        tree = load(path)
        if tree is None:
            warn("no %s (run pass 8 first)" % path)
            continue
        nodes = list(tree.get_editor_property("nodes"))
        ids = {str(n.get_editor_property("id")) for n in nodes}
        appended = [n for n in added_nodes if str(n.get_editor_property("id")) not in ids]
        nodes.extend(appended)
        layout = LAYOUT[tree_name]
        tree_prefix = tree_name[len("DA_SkillTree_"):]
        for n in nodes:
            node_id = str(n.get_editor_property("id"))
            if not KEEP_LAYOUT and node_id in layout:
                n.set_editor_property("position", unreal.Vector2D(*layout[node_id]))
            if node_id in ELEMENTS and n.get_editor_property("element") == EL.NONE:
                n.set_editor_property("element", ELEMENTS[node_id])
            if node_id in KEYSTONES:
                n.set_editor_property("shape", unreal.BeyondSkillNodeShape.DIAMOND)
            if node_id in SUMMARIES and not str(n.get_editor_property("summary")):
                n.set_editor_property("summary", unreal.Text(SUMMARIES[node_id]))
            icon = texture("%s/Icons/Skills/T_Icon_Skill_%s_%s" % (UI, tree_prefix, node_id.split(".", 1)[1]))
            if icon is not None and n.get_editor_property("icon") is None:
                n.set_editor_property("icon", icon)
        tree.set_editor_property("nodes", nodes)
        save(tree, path)
        for problem in tree.validate_tree():
            warn(problem)
        log("%s: %d nodes (%d new)%s" % (tree_name, len(nodes), len(appended), "" if KEEP_LAYOUT else ", laid out again"))


# ---------------------------------------------------------------- 4. ability names

ABILITY_TEXT = {
    ABILITIES + "Angel/GA_Angel_LightningStrike": ("Arcane Spikes", "Crystals of lightning burst from the ground under the target."),
    ABILITIES + "Angel/GA_Angel_ArcaneBolt": ("Arcane Bolt", "A bolt of storm-light from the staff."),
    BP_ABILITIES + "GA_ShootProjectile": ("Arcane Bolt", "A bolt of storm-light from the staff."),
    BP_ABILITIES + "GA_Blink": ("Lightning Blink", "Angel vanishes in a flash and strikes down a short way ahead."),
    BP_ABILITIES + "GA_HealSpell": ("Healing Light", "Storm-light that mends instead of burns."),
    ABILITIES + "JiWoong/GA_JiWoong_SwordCombo": ("Sun-Forged Combo", "Ji-Woong's sword chain, each cut brighter than the last."),
    ABILITIES + "JiWoong/GA_JiWoong_Sunbrand": ("Sunbrand", "Brands an enemy with the sun; the brand bursts on his next blow."),
    ABILITIES + "JiWoong/GA_JiWoong_GildedStep": ("Gilded Step", "A golden lunge that cuts through everything in its path."),
    ABILITIES + "Duo/GA_Duo_HeavensJudgment": ("Heaven's Judgment", "Storm and sun together: lightning falls and a radiant shockwave answers."),
    ABILITIES + "Duo/GA_Duo_EclipseBrand": ("Eclipse Brand", "The lightning brands every enemy it strikes; the shockwave sets them all off."),
    ABILITIES + "Duo/GA_Duo_TempestAegis": ("Tempest Aegis", "After the shockwave both demigods carry a storm shield."),
}


def step_abilities():
    named = 0
    for path, (name, description) in ABILITY_TEXT.items():
        blueprint = load(path)
        if blueprint is None:
            continue
        default = cdo(blueprint)
        try:
            props = {}
            if not str(default.get_editor_property("display_name")):
                props["display_name"] = unreal.Text(name)
            if not str(default.get_editor_property("description")):
                props["description"] = unreal.Text(description)
        except Exception:  # noqa: BLE001 - not a UBeyondGameplayAbility
            warn("%s has no display name (not a Beyond ability)" % path)
            continue
        if props:
            set_props(blueprint, path, **props)
            named += 1
    log("ability names: %d set" % named)


# ---------------------------------------------------------------- 5. quests

OBJECTIVE_TYPES = {
    "flag": "FLAG", "kill": "KILL_ENEMIES", "item": "COLLECT_ITEM", "level": "REACH_LEVEL", "waystones": "ATTUNE_WAYSTONES",
    "bond": "REACH_BOND_LEVEL", "quest": "COMPLETE_QUEST", "talk": "TALK_TO",
}


def make_objective(spec):
    kind = spec[0]
    objective = unreal.BeyondQuestObjective()
    objective.set_editor_property("type", getattr(unreal.BeyondObjectiveType, OBJECTIVE_TYPES[kind]))
    if kind in ("level", "waystones", "bond"):
        objective.set_editor_property("count", int(spec[1]))
    elif kind in ("kill", "item"):
        if spec[1]:
            objective.set_editor_property("target", spec[1])
        objective.set_editor_property("count", int(spec[2]))
    else:
        objective.set_editor_property("target", spec[1])
    objective.set_editor_property("text", unreal.Text(spec[-1]))
    return objective


def find_item(name):
    for folder in ("Armor", "Weapons", "Accessories"):
        path = "%s/%s/%s" % (ITEMS, folder, name)
        if EAL.does_asset_exist(path):
            return load(path)
    warn("quest reward: no item %s" % name)
    return None


def make_quest(spec, kind):
    quest = unreal.BeyondQuest()
    quest.set_editor_property("quest_id", spec["id"])
    quest.set_editor_property("kind", kind)
    quest.set_editor_property("chapter", int(spec.get("chapter", 0)))
    quest.set_editor_property("title", unreal.Text(spec["title"]))
    quest.set_editor_property("summary", unreal.Text(spec["summary"]))
    quest.set_editor_property("location", unreal.Text(spec.get("location", "")))
    thumbnail = texture("%s/Quests/T_Quest_%s" % (UI, spec["thumbnail"])) if spec.get("thumbnail") else None
    if thumbnail is not None:
        quest.set_editor_property("thumbnail", thumbnail)
    elif spec.get("thumbnail"):
        warn("%s: no thumbnail T_Quest_%s (run pass 21)" % (spec["id"], spec["thumbnail"]))
    quest.set_editor_property("requires", list(spec.get("requires", [])))
    if spec.get("flag"):
        quest.set_editor_property("required_flag", spec["flag"])
    quest.set_editor_property("required_level", int(spec.get("level", 1)))
    quest.set_editor_property("objectives", [make_objective(o) for o in spec["objectives"]])

    given = spec.get("reward", {})
    reward = unreal.BeyondQuestReward()
    reward.set_editor_property("experience", float(given.get("exp", 0)))
    reward.set_editor_property("bond_experience", float(given.get("bond", 0)))
    money = unreal.BeyondCurrencyBundle()
    for prop, key in (("gold", "gold"), ("crystals", "crystals"), ("echo_shards", "shards"), ("reverie_fragments", "fragments")):
        money.set_editor_property(prop, int(given.get(key, 0)))
    reward.set_editor_property("currencies", money)
    reward.set_editor_property("items", [item for item in (find_item(name) for name in given.get("items", [])) if item is not None])
    reward.set_editor_property("item_tier", getattr(TIER, given.get("tier", "RARE")))
    quest.set_editor_property("reward", reward)
    return quest


def step_quests():
    database, fill = data_asset(QUESTS, "DA_QuestDatabase", unreal.BeyondQuestDatabase, REBUILD_QUESTS)
    if database is None:
        return
    existing = [] if fill else list(database.get_editor_property("quests"))
    ids = {str(q.get_editor_property("quest_id")) for q in existing}
    wanted = [(spec, unreal.BeyondQuestKind.MAIN) for spec in quest_content.MAIN] + [(spec, unreal.BeyondQuestKind.SIDE) for spec in quest_content.SIDE]
    added = [make_quest(spec, kind) for spec, kind in wanted if spec["id"] not in ids]
    if added or fill:
        database.set_editor_property("quests", existing + added)
        save(database, QUEST_DATABASE)
    for problem in database.validate_quests():
        warn(problem)
    log("%s: %d quests (%d added)" % (QUEST_DATABASE, len(existing) + len(added), len(added)))


def main():
    log("pass 23")
    missing = [name for name in ("BeyondQuestDatabase", "BeyondCurrencyBundle", "BeyondPassiveTrigger") if not hasattr(unreal, name)]
    if missing:
        warn("the editor is running an old build of the WorldBeyond module (%s missing). Close the editor, build WorldBeyondEditor, "
             "then run this script again. Nothing was changed." % ", ".join(missing))
        write_report("last_run_pass23.txt")
        raise RuntimeError("WorldBeyond C++ not rebuilt: %s missing" % ", ".join(missing))
    step_items()
    step_base_crit()
    step_trees()
    step_abilities()
    step_quests()
    write_report("last_run_pass23.txt")


main()
