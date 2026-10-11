"""
Worlds Beyond - rows for the dialogue pack's tables (DT_Dialogue, DT_TextOverHead, DT_Speakers) built from content
dictionaries shaped like dialogue_content.py's, and merged into the tables without touching rows already there.
Used by migrate_pass17.py (pass 13 keeps its own copy of the same builders).
"""
import json

PARTY = ("Angel", "JiWoong")
BANTER_SIDES = {"Angel": "Left", "JiWoong": "Right"}


def dialogue_row(name, speaker, text, next_row, speakers, free=False, event=None, options=None, side=None):
    party = speaker in PARTY
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
        "SpeakerImagePosition": side or (speakers[speaker][1] if speaker else "Left"),
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


def build_dialogue_rows(conversations, banter, speakers):
    """Face-to-face conversations and free-movement banter ("Banter_<id>" chains)."""
    rows = []
    for start, chain in conversations.items():
        lines = chain["lines"]
        names = chain_names(start, len(lines))
        for index, line in enumerate(lines):
            next_row = names[index + 1] if index + 1 < len(lines) else chain.get("next")
            if line[0] == "@options":
                rows.append(dialogue_row(names[index], None, "", None, speakers, options=line[1]))
            else:
                speaker, text = line[0], line[1]
                event = line[2] if len(line) > 2 else None
                rows.append(dialogue_row(names[index], speaker, text, next_row, speakers, event=event))
    for banter_id, entry in banter.items():
        lines = entry[5]
        names = chain_names("Banter_" + banter_id, len(lines))
        for index, (speaker, text) in enumerate(lines):
            next_row = names[index + 1] if index + 1 < len(lines) else None
            rows.append(dialogue_row(names[index], speaker, text, next_row, speakers, free=True, side=BANTER_SIDES.get(speaker)))
    return rows


def build_overhead_rows(chatter):
    rows = []
    for start, lines in chatter.items():
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


def build_speaker_rows(speakers, portrait_path):
    rows = []
    for row, (name, _side) in speakers.items():
        rows.append({
            "Name": row,
            "SpeakerName": name,
            "SpeakerDetail": [{
                "SpeakerExpression": "Neutral",
                "BackgroundTexture": "None",
                "BackgroundMaterial": portrait_path or "None",
                "BackgroundType": "Material" if portrait_path else "Texture",
            }],
        })
    return rows


def check_links(dialogue_rows, overhead_rows, speakers, npc_refs):
    """Broken links: NextRow / option targets, speakers, and the rows NPCs point at. npc_refs: [(who, kind, row)]."""
    dialogue = {row["Name"] for row in dialogue_rows}
    overhead = {row["Name"] for row in overhead_rows}
    problems = []
    for row in dialogue_rows:
        targets = [row["NextRow"]] + [option["NextRow"] for option in row["Option"]]
        problems += ["%s -> %s" % (row["Name"], t) for t in targets if t != "None" and t not in dialogue]
        if row["Speaker_Row"] != "None" and row["Speaker_Row"] not in speakers:
            problems.append("%s: speaker %s" % (row["Name"], row["Speaker_Row"]))
    for row in overhead_rows:
        if row["NextRow"] != "None" and row["NextRow"] not in overhead:
            problems.append("%s -> %s" % (row["Name"], row["NextRow"]))
    for who, kind, start in npc_refs:
        if start not in (dialogue if kind == "conversation" else overhead):
            problems.append("%s: %s %s" % (who, kind, start))
    return problems


def merge_rows(table, new_rows, rewrite=False):
    """Adds the rows missing from a data table (all of them again with rewrite). Returns (added, replaced, missing names)."""
    import unreal
    dtl = unreal.DataTableFunctionLibrary
    existing = json.loads(dtl.export_data_table_to_json_string(table) or "[]")
    by_name = {row["Name"]: index for index, row in enumerate(existing)}
    added = replaced = 0
    for row in new_rows:
        if row["Name"] in by_name:
            if rewrite:
                existing[by_name[row["Name"]]] = row
                replaced += 1
        else:
            by_name[row["Name"]] = len(existing)
            existing.append(row)
            added += 1
    if not added and not replaced:
        return 0, 0, []
    if not dtl.fill_data_table_from_json_string(table, json.dumps(existing, ensure_ascii=False)):
        return -1, 0, [row["Name"] for row in new_rows]
    names = {str(n) for n in dtl.get_data_table_row_names(table)}
    return added, replaced, [row["Name"] for row in new_rows if row["Name"] not in names]
