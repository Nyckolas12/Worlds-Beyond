"""
Worlds Beyond - pass 13: dialogue (Plan 4), on top of the Advanced Dialogue System pack (Content/DialogueSystem).

Run with the editor closed, after building the C++:
    UnrealEditor-Cmd.exe <path>/WorldBeyond.uproject -run=pythonscript -script=<path>/Scripts/Migration/migrate_pass13.py -unattended -nosplash -NullRHI

- Rows from dialogue_content.py go into the pack's own tables (DT_Dialogue, DT_TextOverHead, DT_Speakers; their demo
  rows stay). Only missing rows are added, so edits made in the table editor survive; BEYOND_REWRITE_DIALOGUE=1
  writes every row again.
- Speakers get a see-through placeholder portrait (M_Beyond_NoPortrait) until real ones are set in DT_Speakers.
- WBP_MainDialogue: F and Enter also skip lines (next to Space and the gamepad's A).
- BP_NPC_Base = the pack's BP_ExampleCharacter (dialogue interface, over-head text, focus camera) reparented to
  ABeyondNPCCharacter; the villagers are children of it (Manny / Quinn stand-ins).
- DA_Banter: Angel and Ji-Woong's banter lines (Project Settings -> Worlds Beyond Dialogue points at it).
- TestArena gets a village corner north of the start (elder, merchant, gate guard, two chatting pairs, a campfire)
  and banter volumes over the village and the two camps. Skipped when the map already has NPCs
  (BEYOND_REBUILD_VILLAGE=1 places them again).

Every asset saved is copied to Saved/MigrationBackups/<timestamp>/ first. Report: last_run_pass13.txt.
"""
import importlib
import json
import os
import sys

import unreal

sys.path.insert(0, os.path.join(os.path.abspath(unreal.Paths.project_dir()), "Scripts", "Migration"))
from migration_common import (ASSET_TOOLS, BEL, EAL, backup, bp_class, cdo, ensure_blueprint, load, log, reparent,  # noqa: E402
                              save, warn, write_report)
import dialogue_content as content  # noqa: E402

importlib.reload(content)

PACK = "/Game/DialogueSystem"
DT_DIALOGUE = PACK + "/Blueprint/DataTable/DT_Dialogue"
DT_OVERHEAD = PACK + "/Blueprint/DataTable/DT_TextOverHead"
DT_SPEAKERS = PACK + "/Blueprint/DataTable/DT_Speakers"
MAIN_WIDGET = PACK + "/Blueprint/Widget/WBP_MainDialogue"
EXAMPLE_NPC = PACK + "/Demo/ThirdPerson/Blueprints/BP_ExampleCharacter"
MANNEQUINS = PACK + "/Demo/Characters/Mannequins"

ROOT = "/Game/WorldsBeyond/Dialogue"
NPC_ROOT = "/Game/WorldsBeyond/NPC"
NPC_BASE = NPC_ROOT + "/BP_NPC_Base"
PORTRAIT = ROOT + "/M_Beyond_NoPortrait"
BANTER_ASSET = ROOT + "/DA_Banter"
ARENA = "/Game/WorldsBeyond/Maps/TestArena"
PROPS = "/Game/LevelPrototyping/Meshes"

REWRITE = os.environ.get("BEYOND_REWRITE_DIALOGUE", "") == "1"
REBUILD_VILLAGE = os.environ.get("BEYOND_REBUILD_VILLAGE", "") == "1"

MEL = unreal.MaterialEditingLibrary
DTL = unreal.DataTableFunctionLibrary

TRIGGERS = {
    "RegionEntered": "REGION_ENTERED",
    "LowHealth": "LOW_HEALTH",
    "BossDefeated": "BOSS_DEFEATED",
    "Idle": "IDLE",
    "LevelUp": "LEVEL_UP",
    "Revived": "REVIVED",
    "Scripted": "SCRIPTED",
}


def struct(cls, **props):
    value = cls()
    for key, item in props.items():
        value.set_editor_property(key, item)
    return value


# ---------------------------------------------------------------- portrait placeholder

def step_portrait():
    """A see-through UI material: the pack draws a 512 px portrait per speaker, white without one."""
    if EAL.does_asset_exist(PORTRAIT):
        return load(PORTRAIT)
    folder, name = PORTRAIT.rsplit("/", 1)
    material = ASSET_TOOLS.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())
    if material is None:
        warn("could not create %s" % PORTRAIT)
        return None
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_UI)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    zero = MEL.create_material_expression(material, unreal.MaterialExpressionConstant, -300, 0)
    zero.set_editor_property("r", 0.0)
    if not MEL.connect_material_property(zero, "", unreal.MaterialProperty.MP_OPACITY):
        warn("%s: could not connect Opacity" % PORTRAIT)
    MEL.recompile_material(material)
    save(material, PORTRAIT)
    log("created %s" % PORTRAIT)
    return material


# ---------------------------------------------------------------- tables

def dialogue_row(name, speaker, text, next_row, free=False, event=None, options=None, side=None):
    party = speaker in content.PARTY
    if free:
        # Seconds after the line has finished typing (0.025 s a letter)
        skip = round(min(max(1.4 + 0.03 * len(text), 2.2), 5.0), 1)
    else:
        skip = 0
    return {
        "Name": name,
        "DialogueType": "free movement dialogue" if free else "face to face dialogue",
        "NextRow": next_row or "None",
        "Speaker_Row": speaker or "None",
        "SpeakerExpression": "Neutral" if speaker else "None",
        "SpeakerImagePosition": side or (content.SPEAKERS[speaker][1] if speaker else "Left"),
        # Face to face: the camera cuts to whoever speaks
        "FocusCamera": bool(speaker) and not free,
        "SpeakerForFocusCamera": "OurCharacter" if party else "NPC",
        "SpecialSound": "None",
        "SpecialSoundTrigger": "At the beginning of the text",
        "FaceAnim": "None",
        "AnimMontage": "None",
        "DialogueAnim": "OurCharacter" if party else "NPC",
        "Text": text if not options else "",
        "Option": [{"Option": label, "NextRow": target} for label, target in (options or [])],
        "TextOrOption": "Option" if options else "Text",
        "SkipDialogueWithDuration": skip,
        "SpecialEvent": event or "None",
        "SpecialEventTrigger": "At the beginning of the text",
    }


def chain_names(start, count):
    return [start if index == 0 else "%s.%d" % (start, index) for index in range(count)]


def build_dialogue_rows():
    rows = []
    for start, chain in content.CONVERSATIONS.items():
        lines = chain["lines"]
        names = chain_names(start, len(lines))
        for index, line in enumerate(lines):
            next_row = names[index + 1] if index + 1 < len(lines) else chain.get("next")
            if line[0] == "@options":
                rows.append(dialogue_row(names[index], None, "", None, options=line[1]))
            else:
                speaker, text = line[0], line[1]
                event = line[2] if len(line) > 2 else None
                rows.append(dialogue_row(names[index], speaker, text, next_row, event=event))
    for banter_id, (_trigger, _context, _once, _cooldown, _leader, lines) in content.BANTER.items():
        names = chain_names("Banter_" + banter_id, len(lines))
        for index, (speaker, text) in enumerate(lines):
            next_row = names[index + 1] if index + 1 < len(lines) else None
            rows.append(dialogue_row(names[index], speaker, text, next_row, free=True, side=content.BANTER_SIDES.get(speaker)))
    return rows


def build_overhead_rows():
    rows = []
    for start, lines in content.CHATTER.items():
        names = chain_names(start, len(lines))
        for index, (who, text, seconds) in enumerate(lines):
            rows.append({
                "Name": names[index],
                "NextRow": names[index + 1] if index + 1 < len(lines) else "None",
                "ItIsSelfDialogue": who == "self",
                "DialogueActorIndex": 0 if who == "self" else int(who),
                "SpecialSound": "None",
                "FaceAnim": "None",
                "AnimMontage": "None",
                "Text": text,
                "Duration": float(seconds),
            })
    return rows


def build_speaker_rows(portrait):
    material = portrait.get_path_name() if portrait else "None"
    rows = []
    for row, (name, _side) in content.SPEAKERS.items():
        rows.append({
            "Name": row,
            "SpeakerName": name,
            "SpeakerDetail": [{
                "SpeakerExpression": "Neutral",
                "BackgroundTexture": "None",
                "BackgroundMaterial": material,
                "BackgroundType": "Material" if portrait else "Texture",
            }],
        })
    return rows


def merge_rows(table_path, new_rows):
    table = load(table_path)
    if table is None:
        warn("%s missing: is the dialogue pack installed?" % table_path)
        return False
    existing = json.loads(DTL.export_data_table_to_json_string(table) or "[]")
    by_name = {row["Name"]: index for index, row in enumerate(existing)}
    added = replaced = 0
    for row in new_rows:
        if row["Name"] in by_name:
            if REWRITE:
                existing[by_name[row["Name"]]] = row
                replaced += 1
        else:
            by_name[row["Name"]] = len(existing)
            existing.append(row)
            added += 1
    if not added and not replaced:
        log("%s: all %d rows already there" % (table_path, len(new_rows)))
        return True

    backup(table_path)
    if not DTL.fill_data_table_from_json_string(table, json.dumps(existing, ensure_ascii=False)):
        warn("%s: the rows could not be imported (see the log for the row that failed)" % table_path)
        return False
    missing = [row["Name"] for row in new_rows if row["Name"] not in [str(n) for n in DTL.get_data_table_row_names(table)]]
    if missing:
        warn("%s: rows missing after the import: %s" % (table_path, ", ".join(missing[:10])))
    save(table, table_path)
    log("%s: %d rows added, %d rewritten (%d in the table)" % (table_path, added, replaced, len(existing)))
    return True


def check_links(dialogue_rows, overhead_rows):
    """Every NextRow / option / speaker / NPC row points at something."""
    dialogue = {row["Name"] for row in dialogue_rows}
    overhead = {row["Name"] for row in overhead_rows}
    problems = []
    for row in dialogue_rows:
        targets = [row["NextRow"]] + [option["NextRow"] for option in row["Option"]]
        problems += ["%s -> %s" % (row["Name"], t) for t in targets if t != "None" and t not in dialogue]
        if row["Speaker_Row"] != "None" and row["Speaker_Row"] not in content.SPEAKERS:
            problems.append("%s: speaker %s" % (row["Name"], row["Speaker_Row"]))
    for row in overhead_rows:
        if row["NextRow"] != "None" and row["NextRow"] not in overhead:
            problems.append("%s -> %s" % (row["Name"], row["NextRow"]))
    for bp_name, npc in content.NPCS.items():
        for start, _req, _blk, _once in npc.get("conversations", []):
            if start not in dialogue:
                problems.append("%s: conversation %s" % (bp_name, start))
        for start, _req, _blk in npc.get("chatter", []):
            if start not in overhead:
                problems.append("%s: chatter %s" % (bp_name, start))
        for start in npc.get("greet", []):
            if start not in overhead:
                problems.append("%s: greeting %s" % (bp_name, start))
    for problem in problems:
        warn("dialogue content: broken link %s" % problem)
    return not problems


def step_tables(portrait):
    dialogue_rows = build_dialogue_rows()
    overhead_rows = build_overhead_rows()
    check_links(dialogue_rows, overhead_rows)
    merge_rows(DT_SPEAKERS, build_speaker_rows(portrait))
    merge_rows(DT_DIALOGUE, dialogue_rows)
    merge_rows(DT_OVERHEAD, overhead_rows)


def step_skip_keys():
    widget = load(MAIN_WIDGET)
    if widget is None:
        return
    main = cdo(widget)
    keys = list(main.get_editor_property("SkipDialogueKeys"))
    names = {str(k.get_editor_property("key_name")) for k in keys}
    changed = False
    for name in ("F", "Enter"):
        if name not in names:
            key = unreal.Key()
            key.set_editor_property("key_name", name)
            keys.append(key)
            changed = True
    if changed:
        main.set_editor_property("SkipDialogueKeys", keys)
        save(widget, MAIN_WIDGET)
        log("WBP_MainDialogue: skip keys %s" % ", ".join(str(k.get_editor_property("key_name")) for k in keys))


# ---------------------------------------------------------------- NPCs

def step_npc_base():
    if not EAL.does_asset_exist(NPC_BASE):
        if EAL.duplicate_asset(EXAMPLE_NPC, NPC_BASE) is None:
            warn("could not copy %s to %s" % (EXAMPLE_NPC, NPC_BASE))
            return None
        log("copied %s to %s" % (EXAMPLE_NPC, NPC_BASE))
    bp = reparent(NPC_BASE, unreal.BeyondNPCCharacter)
    if bp is None:
        return None
    BEL.compile_blueprint(bp)
    save(bp, NPC_BASE)
    return bp


def step_npcs(base):
    meshes = {
        "Manny": (load(MANNEQUINS + "/Meshes/SKM_Manny"), bp_class(MANNEQUINS + "/Animations/ABP_Manny")),
        "Quinn": (load(MANNEQUINS + "/Meshes/SKM_Quinn"), bp_class(MANNEQUINS + "/Animations/ABP_Quinn")),
    }
    parent = bp_class(NPC_BASE)
    classes = {}
    for bp_name, npc in content.NPCS.items():
        path = "%s/%s" % (NPC_ROOT, bp_name)
        bp = ensure_blueprint(path, parent)
        if bp is None:
            warn("could not create %s" % path)
            continue
        obj = cdo(bp)
        mesh, anim = meshes[npc["mesh"]]
        conversations = [struct(unreal.BeyondNPCConversation, start_row=start, required_flags=req, blocked_by_flags=blk, once=once)
                         for start, req, blk, once in npc.get("conversations", [])]
        chatter = [struct(unreal.BeyondNPCChatter, start_row=start, required_flags=req, blocked_by_flags=blk, needs_partners=True)
                   for start, req, blk in npc.get("chatter", [])]
        materials = [load("%s/Materials/Instances/%s/%s" % (MANNEQUINS, "Manny" if name.startswith("MI_Manny") else "Quinn", name))
                     for name in npc.get("materials", [])]
        props = {
            "npc_id": npc["id"],
            "display_name": unreal.Text(npc["name"]),
            "npc_mesh": mesh,
            "npc_anim_class": anim,
            "npc_materials": [m for m in materials if m is not None],
            "npc_scale": float(npc.get("scale", 1.0)),
            "can_talk": bool(conversations),
            "conversations": conversations,
            "chatter": chatter,
            "greet_rows": list(npc.get("greet", [])),
            "wander_radius": float(npc.get("wander", 0.0)),
        }
        for key, value in props.items():
            try:
                obj.set_editor_property(key, value)
            except Exception as error:  # keep going; report at the end
                warn("%s: could not set %s (%s)" % (path, key, error))
        BEL.compile_blueprint(bp)
        save(bp, path)
        classes[bp_name] = bp_class(path)
        log("%s: %s, %d conversations, %d chatters" % (path, npc["name"], len(conversations), len(chatter)))
    return classes


# ---------------------------------------------------------------- banter

def step_banter():
    exists = EAL.does_asset_exist(BANTER_ASSET)
    if exists and not REWRITE:
        banter = load(BANTER_ASSET)
        have = {str(line.get_editor_property("id")) for line in banter.get_editor_property("lines")}
        if all(banter_id in have for banter_id in content.BANTER):
            log("%s: all %d lines already there" % (BANTER_ASSET, len(content.BANTER)))
            return banter
        lines = list(banter.get_editor_property("lines"))
    else:
        folder, name = BANTER_ASSET.rsplit("/", 1)
        banter = load(BANTER_ASSET) if exists else ASSET_TOOLS.create_asset(name, folder, unreal.BeyondBanterSet, unreal.DataAssetFactory())
        if banter is None:
            warn("could not create %s" % BANTER_ASSET)
            return None
        lines = []

    have = {str(line.get_editor_property("id")) for line in lines}
    leaders = {key: unreal.EditorAssetLibrary.load_blueprint_class(path.rsplit(".", 1)[0]) for key, path in content.LEADERS.items()}
    for banter_id, (trigger, context, once, cooldown, leader, _lines) in content.BANTER.items():
        if banter_id in have:
            continue
        line = struct(unreal.BeyondBanterLine,
                      id=banter_id,
                      trigger=getattr(unreal.BeyondBanterTrigger, TRIGGERS[trigger]),
                      context=context or "None",
                      start_row="Banter_" + banter_id,
                      once=bool(once),
                      cooldown=float(cooldown),
                      weight=1.0)
        if leader:
            if leaders.get(leader) is None:
                warn("banter %s: leader %s not found" % (banter_id, leader))
            else:
                line.set_editor_property("required_leader", leaders[leader])
        lines.append(line)
    banter.set_editor_property("lines", lines)
    save(banter, BANTER_ASSET)
    log("%s: %d banter lines" % (BANTER_ASSET, len(lines)))
    return banter


# ---------------------------------------------------------------- test arena village

def _spawn(actors, cls, location, yaw=0.0, label=None):
    actor = actors.spawn_actor_from_class(cls, unreal.Vector(*location), unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw))
    if actor is not None and label:
        actor.set_actor_label(label)
    return actor


def _prop(actors, mesh, label, location, scale, yaw=0.0):
    actor = _spawn(actors, unreal.StaticMeshActor, location, yaw=yaw, label=label)
    if actor is not None and mesh is not None:
        actor.static_mesh_component.set_static_mesh(mesh)
        actor.set_actor_scale3d(unreal.Vector(*scale))
    return actor


def _tune_fire_light(light):
    component = light.get_component_by_class(unreal.PointLightComponent)
    component.set_editor_property("light_color", unreal.Color(r=255, g=150, b=70, a=255))
    component.set_editor_property("intensity", 400.0)
    component.set_editor_property("attenuation_radius", 650.0)


def step_village(classes):
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not EAL.does_asset_exist(ARENA) or not levels.load_level(ARENA):
        warn("%s missing: run migrate_pass10.py first" % ARENA)
        return
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

    ours = [a for a in actors.get_all_level_actors()
            if isinstance(a, (unreal.BeyondNPCCharacter, unreal.BeyondBanterVolume)) or a.get_actor_label().startswith("Village_")]
    if any(isinstance(a, unreal.BeyondNPCCharacter) for a in ours) and not REBUILD_VILLAGE:
        log("%s already has its village (BEYOND_REBUILD_VILLAGE=1 places it again)" % ARENA)
        # The first version's campfire light was far too bright
        lights = [a for a in ours if a.get_actor_label() == "Village_FireLight"]
        component = lights[0].get_component_by_class(unreal.PointLightComponent) if lights else None
        if component is not None and component.get_editor_property("intensity") > 400.0:
            _tune_fire_light(lights[0])
            if levels.save_current_level():
                log("toned the campfire light down")
        return
    for actor in ours:
        actors.destroy_actor(actor)

    cx, cy = content.VILLAGE_CENTRE
    cube = load(PROPS + "/SM_Cube")
    cylinder = load(PROPS + "/SM_Cylinder")

    # Campfire with a warm light, the merchant's stall, the north gate
    _prop(actors, cylinder, "Village_Campfire", (cx - 20.0, cy - 60.0, 0.0), (0.9, 0.9, 0.15))
    light = _spawn(actors, unreal.PointLight, (cx - 20.0, cy - 60.0, 90.0), label="Village_FireLight")
    if light is not None:
        component = light.get_component_by_class(unreal.PointLightComponent)
        _tune_fire_light(light)
    _prop(actors, cube, "Village_Stall", (cx - 870.0, cy - 80.0, 50.0), (0.8, 2.2, 1.0))
    for side in (-1.0, 1.0):
        _prop(actors, cube, "Village_GatePost_%s" % ("W" if side < 0 else "E"), (cx + 60.0 + side * 330.0, cy + 900.0, 150.0), (0.6, 0.6, 3.0))

    placed = {}
    for bp_name, label, (dx, dy), yaw, _partners in content.VILLAGE:
        cls = classes.get(bp_name)
        if cls is None:
            warn("no class for %s; %s skipped" % (bp_name, label))
            continue
        npc = _spawn(actors, cls, (cx + dx, cy + dy, 100.0), yaw=yaw, label=label)
        if npc is None:
            warn("could not place %s" % label)
            continue
        placed[label] = npc
    for _bp_name, label, _offset, _yaw, partners in content.VILLAGE:
        if label in placed and partners:
            placed[label].set_editor_property("chatter_partners", [placed[p] for p in partners if p in placed])

    for label, context, (x, y), (hx, hy) in content.BANTER_VOLUMES:
        volume = _spawn(actors, unreal.BeyondBanterVolume, (x, y, 200.0), label=label)
        if volume is None:
            warn("could not place %s" % label)
            continue
        volume.set_editor_property("context", context)
        volume.get_editor_property("box").set_box_extent(unreal.Vector(hx, hy, 400.0))

    log("placed the village (%d NPCs) and %d banter volumes" % (len(placed), len(content.BANTER_VOLUMES)))
    if unreal.BeyondEditorLibrary.build_navigation(world):
        log("rebuilt the navmesh")
    if levels.save_current_level():
        log("saved %s" % ARENA)
    else:
        warn("could not save %s" % ARENA)


# ---------------------------------------------------------------- main

def main():
    log("pass 13")
    missing = [name for name in ("BeyondNPCCharacter", "BeyondBanterSet", "BeyondBanterLine", "BeyondBanterVolume",
                                 "BeyondNPCConversation", "BeyondNPCChatter", "BeyondDialogueSettings", "BeyondEditorLibrary")
               if not hasattr(unreal, name)]
    if missing:
        warn("the editor is running an old build of the WorldBeyond module (%s missing). Close the editor, build "
             "WorldBeyondEditor (Development Editor, Win64) until it says Build succeeded, then run this script again. "
             "Nothing was changed." % ", ".join(missing))
        write_report("last_run_pass13.txt")
        raise RuntimeError("WorldBeyond C++ not rebuilt: %s missing" % ", ".join(missing))

    portrait = step_portrait()
    step_tables(portrait)
    step_skip_keys()
    base = step_npc_base()
    classes = step_npcs(base) if base is not None else {}
    step_banter()
    step_village(classes)
    write_report("last_run_pass13.txt")


main()
