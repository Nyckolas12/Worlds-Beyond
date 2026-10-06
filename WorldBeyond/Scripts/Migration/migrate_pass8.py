"""
Worlds Beyond - pass 8: skill trees and duo powers (Plan 1B).

Run with the editor closed, after building the C++:
    UnrealEditor-Cmd.exe <path>/WorldBeyond.uproject -run=pythonscript -script=<path>/Scripts/Migration/migrate_pass8.py -unattended -nosplash -NullRHI

- Three skill trees (data assets in /Game/WorldsBeyond/SkillTrees/): Angel's "Stormcaller" and Ji-Woong's "Radiant
  Hexblade" (skill points, one per level) and the shared "Bond of Heaven" duo tree (Bond Points: one every 3 party
  levels and one per main boss). Nodes raise stats, rank up abilities (more damage, shorter cooldowns), unlock duo
  powers and change the Bond meter.
- Two new duo powers made from Heaven's Judgment: GA_Duo_EclipseBrand (the flashes brand enemies, the shockwave sets
  every brand off and lightning chains through them) and GA_Duo_TempestAegis (both demigods get a storm shield that
  cuts damage taken by 40 % and reflects half of each hit).
- K opens the skill tree (IA_SkillTree); the screen itself is C++ (UBeyondSkillTreeWidget) in the SkillTreeSystem
  pack's style.

The trees and the duo powers are only created when missing, so editor tuning survives a re-run. To rebuild the trees
from this script (node ids stay the same, so saved ranks still match), run it with BEYOND_REBUILD_TREES=1.
Every asset saved is copied to Saved/MigrationBackups/<timestamp>/ first.
"""
import os
import sys

import unreal

sys.path.insert(0, os.path.join(os.path.abspath(unreal.Paths.project_dir()), "Scripts", "Migration"))
from migration_common import (ASSET_TOOLS, EAL, bp_class, cdo, fx, load, log, map_keys, save, set_props, tag,  # noqa: E402
                              warn, write_report)

ABILITIES = "/Game/WorldsBeyond/Abilities/"
TREES = "/Game/WorldsBeyond/SkillTrees"
FXV = "/Game/FXVarietyPack/Particles/"
ANGEL = "/Game/WorldsBeyond/Characters/Angel/BP_Angel"
JIWOONG = "/Game/WorldsBeyond/Characters/Ji-Woong/BP_Ji-Woong"
PC = "/Game/WorldsBeyond/Blueprints/BP_PC"

HEAVENS_JUDGMENT = ABILITIES + "Duo/GA_Duo_HeavensJudgment"
ECLIPSE_BRAND = ABILITIES + "Duo/GA_Duo_EclipseBrand"
TEMPEST_AEGIS = ABILITIES + "Duo/GA_Duo_TempestAegis"

PACK_TEXTURES = "/Game/SkillTreeSystem/Textures/"
UI_IMAGES = "/Game/WorldsBeyond/Blueprints/Widgets/Images/"

BLUE = (0.25, 0.5, 1.0, 1.0)
PURPLE = (0.6, 0.2, 1.0, 1.0)
GOLD = (1.0, 0.78, 0.2, 1.0)

REBUILD = os.environ.get("BEYOND_REBUILD_TREES", "") == "1"

S = unreal.BeyondSkillStat
E = unreal.BeyondSkillEffectType


# ---------------------------------------------------------------- duo powers

def duplicate_duo(path):
    """(blueprint, created): an existing duo power, or a copy of Heaven's Judgment (keeps its montages and effects)."""
    if EAL.does_asset_exist(path):
        return load(path), False
    if not EAL.does_asset_exist(HEAVENS_JUDGMENT):
        warn("%s not found; can't make %s" % (HEAVENS_JUDGMENT, path))
        return None, False
    copy = EAL.duplicate_asset(HEAVENS_JUDGMENT, path)
    if copy is None:
        warn("could not duplicate %s to %s" % (HEAVENS_JUDGMENT, path))
        return None, False
    log("created %s (copy of Heaven's Judgment)" % path)
    return copy, True


def step_duo_powers():
    sunbrand = load(ABILITIES + "JiWoong/GA_JiWoong_Sunbrand")
    brand = cdo(sunbrand).get_editor_property("brand") if sunbrand else unreal.BeyondBrandSettings()
    brand.set_editor_property("duration", 10.0)

    eclipse, created = duplicate_duo(ECLIPSE_BRAND)
    if eclipse and created:
        set_props(eclipse, ECLIPSE_BRAND,
                  flash_count=6, flash_damage=10.0,
                  brand_struck_enemies=True, struck_brand=brand,
                  shockwave_detonates_brands=True, chain_damage=30.0,
                  chain_fx=fx(system=FXV + "P_ky_lightning2", scale=0.7, color=PURPLE),
                  shockwave_damage_center=150.0, shockwave_damage_edge=70.0)
        log("%s: flashes brand what they hit, the shockwave sets every brand off, lightning chains through them" % ECLIPSE_BRAND)
    elif eclipse:
        log("kept existing %s" % ECLIPSE_BRAND)

    aegis, created = duplicate_duo(TEMPEST_AEGIS)
    if aegis and created:
        shield = unreal.BeyondAegisSettings()
        shield.set_editor_property("duration", 8.0)
        shield.set_editor_property("damage_reduction", 0.4)
        shield.set_editor_property("reflect_fraction", 0.5)
        shield.set_editor_property("reflect_hit_response", tag("Event.Hit.Light"))
        aura = fx(system=FXV + "P_ky_thunderBall", scale=0.9, color=BLUE)
        aura.set_editor_property("max_lifetime", 0.0)
        shield.set_editor_property("aura_fx", aura)
        shield.set_editor_property("reflect_fx", fx(system=FXV + "P_ky_lightning1", scale=0.6, color=BLUE))
        set_props(aegis, TEMPEST_AEGIS, flash_count=4, flash_damage=12.0,
                  shockwave_damage_center=120.0, shockwave_damage_edge=60.0, aegis=shield)
        log("%s: storm shield for 8 s after the shockwave (-40 %% damage taken, half reflected)" % TEMPEST_AEGIS)
    elif aegis:
        log("kept existing %s" % TEMPEST_AEGIS)

    return {name: bp_class(path) for name, path in (("judgment", HEAVENS_JUDGMENT), ("eclipse", ECLIPSE_BRAND),
                                                     ("aegis", TEMPEST_AEGIS))}


# ---------------------------------------------------------------- trees

def texture(path):
    return load(path) if EAL.does_asset_exist(path) else None


def ability_icon(ability_class, fallback):
    """The ability's own icon if it has one, else a stand-in from the pack / the ability bar art."""
    if ability_class:
        icon = unreal.get_default_object(ability_class).get_editor_property("icon")
        if icon:
            return icon
    return texture(fallback) if fallback else None


def effect(kind, value=0.0, stat=None, ability=None):
    e = unreal.BeyondSkillEffect()
    e.set_editor_property("type", kind)
    e.set_editor_property("value", float(value))
    if stat is not None:
        e.set_editor_property("stat", stat)
    if ability is not None:
        e.set_editor_property("ability", ability)
    return e


def stat(which, value):
    return effect(E.STAT, value, stat=which)


def rank(ability):
    return effect(E.ABILITY_RANK, ability=ability)


def node(node_id, name, description, x, y, effects, max_rank=1, cost=1, level=1, requires=(), icon=None):
    n = unreal.BeyondSkillNode()
    n.set_editor_property("id", node_id)
    n.set_editor_property("display_name", unreal.Text(name))
    n.set_editor_property("description", unreal.Text(description))
    n.set_editor_property("position", unreal.Vector2D(x, y))
    n.set_editor_property("max_rank", max_rank)
    n.set_editor_property("cost", cost)
    n.set_editor_property("required_level", level)
    n.set_editor_property("requires", list(requires))
    n.set_editor_property("effects", [e for e in effects if e is not None])
    if icon:
        n.set_editor_property("icon", icon)
    return n


def make_tree(name, title, subtitle, currency, accent, nodes, starter=None):
    path = "%s/%s" % (TREES, name)
    exists = EAL.does_asset_exist(path)
    if exists and not REBUILD:
        log("kept existing %s (set BEYOND_REBUILD_TREES=1 to rebuild it)" % path)
        return load(path)

    tree = load(path) if exists else ASSET_TOOLS.create_asset(name, TREES, unreal.BeyondSkillTreeAsset, unreal.DataAssetFactory())
    if tree is None:
        warn("could not create %s" % path)
        return None
    tree.set_editor_property("tree_name", unreal.Text(title))
    tree.set_editor_property("subtitle", unreal.Text(subtitle))
    tree.set_editor_property("currency", currency)
    tree.set_editor_property("accent_color", unreal.LinearColor(*accent))
    if starter is not None:
        tree.set_editor_property("starter_duo_power", starter)
    tree.set_editor_property("nodes", nodes)
    save(tree, path)
    problems = tree.validate_tree()
    for problem in problems:
        warn(problem)
    log("%s %s: %d nodes%s" % ("rebuilt" if exists else "created", path, len(nodes), "" if not problems else " (see warnings)"))
    return tree


def angel_tree():
    spikes = bp_class(ABILITIES + "Angel/GA_Angel_LightningStrike")
    bolt = bp_class(ABILITIES + "Angel/GA_Angel_ArcaneBolt")
    spikes_icon = ability_icon(spikes, PACK_TEXTURES + "T_MagicWand")
    bolt_icon = ability_icon(bolt, UI_IMAGES + "crescent-staff")
    nodes = [
        node("Angel.Arcana1", "Storm Affinity", "The storm answers him faster. Spells grow with Arcana.",
             -1, 0, [stat(S.ARCANA, 4)], max_rank=3),
        node("Angel.Vitality1", "Thunder-Tempered", "Lightning hardens the body that carries it.",
             1, 0, [stat(S.MAX_HEALTH, 12)], max_rank=3),
        node("Angel.Spikes", "Crystal Eruption", "Arcane Spikes break the ground harder and return sooner.",
             -2, 1, [rank(spikes)], max_rank=3, level=3, requires=["Angel.Arcana1"], icon=spikes_icon),
        node("Angel.Bolt", "Arcane Volley", "The arcane bolts Angel casts as your companion hit harder.",
             0, 1, [rank(bolt)], max_rank=2, level=4, requires=["Angel.Arcana1"], icon=bolt_icon),
        node("Angel.Stamina", "Second Wind", "More breath for dodging and casting.",
             2, 1, [stat(S.MAX_STAMINA, 10)], max_rank=2, level=4, requires=["Angel.Vitality1"]),
        node("Angel.Arcana2", "Eye of the Storm", "Calm at the centre, ruin at the edge.",
             -1, 2, [stat(S.ARCANA, 8)], max_rank=2, cost=2, level=9, requires=["Angel.Spikes"]),
        node("Angel.Ward", "Static Ward", "A crackling skin that turns blows aside.",
             1, 2, [stat(S.DEFENSE, 4)], max_rank=3, level=6, requires=["Angel.Vitality1"]),
        node("Angel.Overcharge", "Overcharge", "Arcane Spikes and his Arcana, pushed past their limit.",
             -2, 3, [rank(spikes), stat(S.ARCANA, 4)], max_rank=2, cost=2, level=14, requires=["Angel.Arcana2"],
             icon=spikes_icon),
        node("Angel.Vitality2", "Unbroken Spirit", "Storms pass. He doesn't.",
             2, 3, [stat(S.MAX_HEALTH, 30), stat(S.DEFENSE, 5)], cost=2, level=12,
             requires=["Angel.Ward", "Angel.Stamina"]),
        node("Angel.Tempest", "Tempest Incarnate", "Angel becomes the storm.",
             0, 4, [stat(S.ARCANA, 12), stat(S.MAX_HEALTH, 20)], cost=3, level=18,
             requires=["Angel.Arcana2", "Angel.Ward"]),
    ]
    return make_tree("DA_SkillTree_Angel", "Stormcaller", "Angel  -  spells grow with Arcana; the storm keeps him standing",
                     unreal.BeyondSkillCurrency.SKILL_POINTS, (0.10, 0.45, 1.0, 1.0), nodes)


def jiwoong_tree():
    combo = bp_class(ABILITIES + "JiWoong/GA_JiWoong_SwordCombo")
    step = bp_class(ABILITIES + "JiWoong/GA_JiWoong_GildedStep")
    brand = bp_class(ABILITIES + "JiWoong/GA_JiWoong_Sunbrand")
    combo_icon = ability_icon(combo, PACK_TEXTURES + "T_Sword")
    step_icon = ability_icon(step, UI_IMAGES + "sprint")
    brand_icon = ability_icon(brand, PACK_TEXTURES + "T_MagicWand")
    nodes = [
        node("JiWoong.Strength1", "Gilded Edge", "Every swing carries more of the sun. Sword hits grow with Strength.",
             -1, 0, [stat(S.STRENGTH, 4)], max_rank=3),
        node("JiWoong.Vitality1", "Sunlit Vigor", "The front line needs a sturdy body.",
             1, 0, [stat(S.MAX_HEALTH, 15)], max_rank=3),
        node("JiWoong.Combo", "Sun-Forged Combo", "The sword combo cuts deeper.",
             -2, 1, [rank(combo)], max_rank=3, level=3, requires=["JiWoong.Strength1"], icon=combo_icon),
        node("JiWoong.Brand", "Deeper Brand", "Sunbrand's detonation hits harder and the brand returns sooner.",
             0, 1, [rank(brand)], max_rank=3, level=4, requires=["JiWoong.Strength1"], icon=brand_icon),
        node("JiWoong.Stamina", "Relentless", "He keeps pressing long after others tire.",
             2, 1, [stat(S.MAX_STAMINA, 10)], max_rank=2, level=4, requires=["JiWoong.Vitality1"]),
        node("JiWoong.Step", "Radiant Stride", "Gilded Step strikes harder and recharges sooner.",
             -1, 2, [rank(step)], max_rank=2, level=6, requires=["JiWoong.Combo"], icon=step_icon),
        node("JiWoong.Hex", "Hex Scholar", "Brand detonations and the duo shockwave grow with Arcana.",
             0, 2, [stat(S.ARCANA, 5)], max_rank=2, level=7, requires=["JiWoong.Brand"]),
        node("JiWoong.Guard", "Bulwark", "Golden plate of light turns blows aside.",
             1, 2, [stat(S.DEFENSE, 6)], max_rank=3, level=6, requires=["JiWoong.Vitality1"]),
        node("JiWoong.Strength2", "Radiant Might", "The blade of a demigod.",
             -2, 3, [stat(S.STRENGTH, 8)], max_rank=2, cost=2, level=10, requires=["JiWoong.Combo"]),
        node("JiWoong.Vitality2", "Unyielding", "He does not fall while Angel stands.",
             2, 3, [stat(S.MAX_HEALTH, 35), stat(S.DEFENSE, 6)], cost=2, level=12,
             requires=["JiWoong.Guard", "JiWoong.Stamina"]),
        node("JiWoong.Judgment", "Radiant Judgment", "Sword and brand, perfected.",
             0, 4, [stat(S.STRENGTH, 12), stat(S.DEFENSE, 8), rank(brand)], cost=3, level=18,
             requires=["JiWoong.Strength2", "JiWoong.Hex"], icon=brand_icon),
    ]
    return make_tree("DA_SkillTree_JiWoong", "Radiant Hexblade", "Ji-Woong  -  blade and brand; Strength and Defense",
                     unreal.BeyondSkillCurrency.SKILL_POINTS, (1.0, 0.55, 0.12, 1.0), nodes)


def duo_tree(powers):
    judgment, eclipse, aegis = powers["judgment"], powers["eclipse"], powers["aegis"]
    duo_icon = ability_icon(judgment, UI_IMAGES + "crescent-staff")
    nodes = [
        node("Duo.Kindred", "Kindred Spirits", "The Bond meter fills faster.",
             0, 0, [effect(E.BOND_GAIN, 0.15)], max_rank=3),
        node("Duo.Wrath", "Heaven's Wrath", "Heaven's Judgment hits harder. Right-click to put it back on G.",
             -1.5, 1, [rank(judgment)], max_rank=3, level=3, requires=["Duo.Kindred"], icon=duo_icon),
        node("Duo.Eclipse", "Eclipse Brand", "New duo power: the lightning brands every enemy it strikes, the shockwave "
             "sets all the brands off and lightning chains through them.",
             0, 1, [effect(E.DUO_POWER, ability=eclipse)], cost=2, level=6, requires=["Duo.Kindred"],
             icon=ability_icon(eclipse, UI_IMAGES + "crescent-staff")),
        node("Duo.Aegis", "Tempest Aegis", "New duo power: after the shockwave both demigods carry a storm shield for "
             "8 s that cuts damage taken by 40 % and throws half of each hit back.",
             1.5, 1, [effect(E.DUO_POWER, ability=aegis)], cost=2, level=9, requires=["Duo.Kindred"],
             icon=ability_icon(aegis, UI_IMAGES + "crescent-staff")),
        node("Duo.Echo", "Lingering Bond", "After a duo move the meter keeps some of its charge.",
             -1.5, 2, [effect(E.BOND_ECHO, 0.15)], max_rank=2, level=10, requires=["Duo.Wrath"]),
        node("Duo.TotalEclipse", "Total Eclipse", "Eclipse Brand hits harder.",
             0, 2, [rank(eclipse)], max_rank=2, level=12, requires=["Duo.Eclipse"],
             icon=ability_icon(eclipse, UI_IMAGES + "crescent-staff")),
        node("Duo.EyeOfTempest", "Eye of the Tempest", "Tempest Aegis's shockwave and reflected lightning hit harder.",
             1.5, 2, [rank(aegis)], max_rank=2, level=12, requires=["Duo.Aegis"],
             icon=ability_icon(aegis, UI_IMAGES + "crescent-staff")),
        node("Duo.SoulBound", "Soul-Bound", "Two demigods, one heartbeat: the meter fills much faster.",
             0, 3, [effect(E.BOND_GAIN, 0.25)], cost=2, level=15, requires=["Duo.Echo"]),
    ]
    return make_tree("DA_SkillTree_Duo", "Bond of Heaven",
                     "Shared by both  -  Bond Points: one every 3 levels and one per main boss",
                     unreal.BeyondSkillCurrency.BOND_POINTS, (0.62, 0.3, 1.0, 1.0), nodes, starter=judgment)


# ---------------------------------------------------------------- wiring

def step_assign(angel, jiwoong, duo):
    for path, tree in ((ANGEL, angel), (JIWOONG, jiwoong)):
        bp = load(path)
        if bp and tree:
            set_props(bp, path, skill_tree=tree)
            log("%s: skill tree %s" % (path, tree.get_name()))

    actions = map_keys("/Game/Input/IMC_Default", {"IA_SkillTree": "K"})
    pc = load(PC)
    if pc is None:
        warn("%s not found; set Duo Skill Tree Asset and Skill Tree Action on your player controller by hand" % PC)
        return
    props = dict(skill_tree_action=actions["IA_SkillTree"])
    if duo:
        props["duo_skill_tree_asset"] = duo
    set_props(pc, PC, **props)
    widget = cdo(pc).get_editor_property("skill_tree_widget_class")
    log("%s: K opens the skill tree (%s), duo tree %s" % (PC, widget.get_name() if widget else "NO WIDGET CLASS",
                                                         duo.get_name() if duo else "missing"))
    if widget is None:
        warn("BP_PC -> UI|Skill Tree -> Skill Tree Widget Class is empty: K won't open anything")


def main():
    log("pass 8")
    # The new C++ types only exist once the editor module is rebuilt; stop before touching any asset
    missing = [name for name in ("BeyondSkillTreeAsset", "BeyondSkillNode", "BeyondAegisSettings", "BeyondSkillTreeWidget")
               if not hasattr(unreal, name)]
    if missing:
        warn("the editor is running an old build of the WorldBeyond module (%s missing). Close the editor, build "
             "WorldBeyondEditor (Development Editor, Win64) in Rider until it says Build succeeded, then run this "
             "script again. Nothing was changed." % ", ".join(missing))
        write_report("last_run_pass8.txt")
        raise RuntimeError("WorldBeyond C++ not rebuilt: %s missing" % ", ".join(missing))

    powers = step_duo_powers()
    angel = angel_tree()
    jiwoong = jiwoong_tree()
    duo = duo_tree(powers)
    step_assign(angel, jiwoong, duo)
    write_report("last_run_pass8.txt")


main()
