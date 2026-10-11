# Plan 4 — Dialogue: NPCs, village chatter, demigod banter (done)

Part of the [roadmap](../Roadmap.md). Built in pass 13 (`migrate_pass13.py`, test `WorldsBeyond.Prototype.Dialogue`) on
top of the **Advanced Dialogue System (Replicated)** pack in `Content/DialogueSystem`. How to play with it and add
content: *Dialogue* in [GAS_Prototype.md](../GAS_Prototype.md#dialogue).

Your picks: **bottom subtitles** for the demigods' banter (FF7 Rebirth style, the pack's *free movement* rows),
**starter content** (placeholder lines in theme, yours to rewrite), the first NPCs in a **TestArena village corner**
(real villages are Plan 5) and **story flags only** (no quest log yet).

## The pack, and how we drive it

The pack's Blueprint graphs are untouched: graph nodes can't be scripted, and keeping them means a pack update or your
own edits in its widgets still work. C++ calls its functions and reads its variables by name (`Dialogue/BeyondDialogueBridge`,
like `BeyondLegacyDamageBridge` for the old damage component); `CheckPackBindings` lists anything that goes missing and
the Dialogue test fails on it.

| Pack piece | What it does | How Plan 4 uses it |
|---|---|---|
| `BP_AC_Dialogue` | `StartDialogue(RowName, Actor)` runs a `DT_Dialogue` chain in `WBP_MainDialogue`. *Face to face*: UI-only input, both turn to each other, focus camera per row. *Free movement*: box at the bottom, you keep playing, lines advance on their own | Both demigods get one at runtime; the **leader's** runs talks and banter |
| `BP_AC_DialogueOverHead` | `StartDialogueReplicated(RowName, DialogueActors)` runs `DT_TextOverHead` rows over heads, each said by the starter or `DialogueActors[DialogueActorIndex]` for *Duration* s | Villager chatter and greetings |
| `BP_I_Dialogue` + `BP_ExampleCharacter` | The NPC side: *Row Name*, *Is in Dialogue*, *Selected Option*, *Dialogue Anim* | `BP_NPC_Base` is a copy of `BP_ExampleCharacter` reparented to `ABeyondNPCCharacter` |
| Over-head text + focus camera | A `WidgetComponent` (`WBP_TextOverHead`) and a `ChildActorComponent` (`BP_CameraActor`), both tagged `dialogue` | Added to the demigods at runtime; placed over the head / in front of the face on every speaker |
| `DT_Dialogue`, `DT_TextOverHead`, `DT_Speakers` | Read directly by the pack's Blueprints | Our rows go into these tables (the demo rows stay) |
| `WBP_Dialogue.UpdatedCurrentRow` | Fires on every row | Runs the row's **Special Event** (our hook for story flags) |

## Pieces

| Piece | Where | Notes |
|---|---|---|
| Settings | `Dialogue/BeyondDialogueSettings.*` | *Project Settings → Game → Worlds Beyond Dialogue*: the pack's classes and tables, talk range / angle, the no-talking-in-a-fight radius, chatter and greeting radii and timing, the banter set, banter cooldown, idle time, low-health share, boss-banter delay, over-head height and focus-camera offset |
| Bridge | `Dialogue/BeyondDialogueBridge.*` | Start / close / skip / choose a choice, read the current row, rows as C++ structs, give a character the pack's parts (`EnsureParticipant`). After a start it points the dialogue widget at the component running it (each demigod owns one; the widget would otherwise keep the one it saw first) |
| Hub | `Dialogue/BeyondDialogueSubsystem.*` | One conversation at a time (a talk ends banter); every row's *Special Event*: `Flag.X` / `Unflag.X` set and clear story flag X, several joined with `;`, anything else is broadcast as **On Dialogue Event** for Blueprints; **On Conversation Started / Ended**, **On Dialogue Row**; a boss falling (clones and summons don't count) sets `Boss.<boss id>` and fires **On Boss Defeated** |
| NPC | `Dialogue/BeyondNPCCharacter.*` | Look (mesh, anim Blueprint, materials, scale) applied from its properties; **Conversations** in order, the first whose flags pass is used (*Once* ones are remembered as `Talked.<NPC Id>.<Row>`); **Chatter** with its *Chatter Partners* (placed NPCs) taken in turn while the leader is within *Chatter Radius*; **Greet Rows** when the party walks up; optional wandering. No team (enemies and the buddy ignore it), no ability system (it can't be hurt) |
| Talking | `ABeyondPlayerController` | **F** talks to the nearest NPC in front of the leader (talking wins over loot), not while an enemy near the leader is fighting; `FindTalkTarget`, `TalkTo`. The HUD (health, ability bar, Bond meter, crosshair, plates) hides while talking face to face (*Hide HUD While Talking*) |
| Prompt | `UI/BeyondInteractPromptWidget.*` | "[F] Talk - Elder Maren" / "[F] Pick up", drawn in C++, refreshed on a timer |
| Banter | `Dialogue/BeyondBanter.*`, `Dialogue/BeyondBanterComponent.*` | `DA_Banter` (`UBeyondBanterSet`): lines with a trigger, a context, flags, once, cooldown, weight, required leader. Triggers: **Banter Volume** (region; Plan 5's region volumes call the same `TriggerBanter`), the leader under 30 % in a fight, a boss falling (context = boss id), 40 s idle, a level-up, a revive. Lines for the exact context win, and among them a first-time line beats repeatable ones. One per 45 s; a new region or a boss victory doesn't wait and cuts a running quip short. The box is lifted above the ability bar (*Banter Box Offset*). Never during a talk, a menu, a cutscene or with a demigod down; a Tab swap ends it |
| Story flags | `UBeyondPartyComponent`, `UBeyondSaveGame` v5 | `HasStoryFlag` / `SetStoryFlag` / `On Story Flag Changed`, saved; `Boss.<id>` also answers for story bosses beaten in older saves |
| Body montages | `ABeyondCharacterBase::PlayAnimMontage` | Plays on the MetaHuman's **Body** like `StopAnimMontage` (the pack's *Dialogue Anim* rows use it) |
| Cheats | console | `Beyond.Flag <name> [0/1]`, `Beyond.ListFlags`, `Beyond.ResetFlags`, `Beyond.Banter <Id or Trigger> [Context]` |

## Content (placeholder, in `Scripts/Migration/dialogue_content.py`)

Mossbrook, the starting village. Angel (he) is quick, restless and cheeky; Ji-Woong is calm, dry and few-worded.

| Who | What they say | Flags |
|---|---|---|
| **Elder Maren** (`elder_maren`) | Intro with choices: hunt the Pale Alpha / ask about the corruption (lore, back to the choices) / leave · waiting · after Gorehide (once; points east at Veyla) · default | sets `Quest_Gorehide`, `Quest_Veyla`; reads `Boss.gorehide` |
| **Bram**, merchant (`merchant_bram`) | Intro (once: raiders took his cart, no shop yet) · after Gorehide · default | sets `Met_Bram` |
| **Captain Ysolde**, north gate (`guard_ysolde`) | Gate warning with choices (the two giants / "we can handle it") · after Kael'thar · after Hrimgar · after both | reads `Boss.kaelthar`, `Boss.hrimgar` |
| **Tilda & Osk** | Wolves at night (until Gorehide falls) · the demigods · the alpha's dead | `Boss.gorehide` |
| **Pell & Wren** | Blighted crops (until Veyla falls) · Bram's sulking · the hollow voice (after the elder's quest) · the woods gone quiet | `Quest_Veyla`, `Boss.veyla` |
| Greetings | One line each when the party walks up | — |
| **Banter** (21) | Village first look; forest ×2 and corrupted woods ×2 (first looks once); low health (Angel leading, Ji-Woong leading, either); each boss's fall + a general one; idle ×4; level-up ×2; revive ×2 | `Banter.<Id>` for once-only lines |

Boss ids are the definitions' *Boss Id*: `gorehide`, `veyla`, `kaelthar`, `hrimgar`.

## Test arena village

North of the start (centre 0, -2150): the elder by a campfire with a warm light, Bram at a stall, Ysolde between two
gate posts to the north, Tilda & Osk and Pell & Wren chatting. Banter volumes over the village (`region_village`) and the
two camps (`region_forest`, `region_corrupted_woods`). `BEYOND_REBUILD_VILLAGE=1` places it again.

## Decisions made on the way

- **Rows go into the pack's tables**, not our own: its Blueprints read `DT_Dialogue` / `DT_TextOverHead` /
  `DT_Speakers` directly, and pointing them elsewhere would mean editing graphs. Our rows have their own names
  (`Elder_*`, `Chat_*`, `Banter_*`...); the demo's `1`, `2`... rows stay so its demo map still works.
- **Re-running pass 13 only adds missing rows**, so lines you rewrite in the table editor survive;
  `BEYOND_REWRITE_DIALOGUE=1` writes every row (and banter line) from the content file again.
- **See-through portraits:** the pack draws a 512 px portrait for every speaker and a white square without a texture,
  so speakers point at `M_Beyond_NoPortrait` until you set real portraits in `DT_Speakers`.
- **Skip keys:** Space and the gamepad's A (the pack's), plus F and Enter.
- **Conditions are per conversation**, not per row: the pack's choices can't be hidden by flags, so an NPC picks a
  whole conversation by flags. Branching on flags inside a conversation would need the widget changed.
- **Special events fire when a row starts** (the pack's *Special Event Trigger* column is ignored by C++).
- **Banter runs on the leader's own dialogue component** (no extra widget), so a Tab swap ends it.
- **The pack's layout stays**; C++ only lifts the box during banter (it would cover the ability bar) and hides our HUD
  during talks. The Dialogue test saves screenshots of a talk, the choices, chatter and banter to
  `Saved/Screenshots/Dialogue/` when it runs with rendering (without `-nullrhi`).

## Left for you

- **Portraits:** in `DT_Speakers`, set *Background Type* Texture and a *Background Texture* (Angel, Ji-Woong, the
  villagers). Portrait side per row: *Speaker Image Position*.
- **Voices:** *Special Sound* per row in both tables.
- **Look:** `WBP_Dialogue` / `WBP_TextOverHead` are the pack's; restyle them to match the HUD if you like.
- **Gestures:** *Anim Montage* per row (*Dialogue Anim* says who plays it).

### Later
- Villages in the open world (Plan 5): place `BP_NPC_*` children, give region volumes `TriggerBanter`.
- Real villager meshes (MetaHumans or a medieval pack) instead of the Manny / Quinn stand-ins.
- A quest log on top of the story flags, shops for Bram.
