# Plan 6 — UI overhaul to the mockup look, and the systems the mockups show (in progress)

Part of the [roadmap](../Roadmap.md). The three mockups (Armor, Overview, Skill Tree) set the look: deep-navy panels,
thin gold filigree and corner ornaments, Cinzel small caps over EB Garamond, skewed ANGEL / JI-WOONG / DUO tabs (Q / E),
a left sidebar, rarity-framed painted icons, a radial skill graph and key-hint bars.

Your picks: **everything** gets the look (the character menu, every C++ HUD piece, the Blueprint UMG widgets); the
**missing systems are built** (currencies, Accessory + Charm slots, upgrades +1 .. +15 from the menu, critical hits,
elemental mastery and affinity, Bond levels, lock / junk / salvage, quests, playtime, records); sidebar **Skill Tree,
Armor, Overview, Quests**; art **generated with Higgsfield** (the mockups as style references); fonts **Cinzel +
EB Garamond** (SIL OFL, in `Art/Fonts/`).

Status: phases 1–5 built (passes 20, 21, 23; tests below). Next: the HUD (6), theming the Blueprint widgets (7), the 3D
models (inspect view, loot drops, main-menu relic), docs and captures (8).

## The look (phase 1, pass 20)

| Piece | Where | Notes |
|---|---|---|
| Style | `UI/BeyondUIStyle.*` | Palette, fonts (`Font(EBeyondFont, Size)`), text measuring / wrapping / fitting, number / percent / playtime formats, the 1920 × 1080 design space (uniform fit, centred), and `FPainter`: boxes, gradients, text with shadow, diamonds, sparkles, panels with corner ornaments, skew tabs, key caps, rarity item frames (+N, lock, junk badges), bars, rune circles, line glyphs |
| Art set | `UI/BeyondUIArt.*` → `DA_UIArt` | Backdrops, rune circle, emblems, currency / element / ability icons |
| Profiles | `UI/BeyondCharacterProfile.h` → `DA_Profile_{Angel,JiWoong,Duo}` | Class title, motto, element, accent, emblem, card portrait, full art, silhouette, medallion |
| Settings | `UI/BeyondUIStyleSettings.*` | Font, art and sound defaults |
| Glyphs | `UI/BeyondUIGlyphs.*` | Line icons for everything without a picture |
| Swatch | cheat `Beyond.UI.Swatch` | Every painter piece on one screen |

## The character menu (phase 2)

Every page is its own top-level menu on `UBeyondMenuWidget` (frame: backdrop, emblem + title, header tabs with Q / E,
counter, sidebar, key bar, message line). **C** Overview, **K** Skill Tree, **I** Armor, **J** Quests, **P** / Start
pause; in a menu **1–4** or the sidebar jump pages, **Q / E** switch character (the quest log: its filter).

| Page | Keys |
|---|---|
| Skill Tree (`UBeyondSkillTreeWidget`) | F (hold) upgrade, R twice reset, G put a duo power on the duo slot |
| Armor (`UBeyondInventoryWidget`) | F equip / take off, **U (hold) upgrade**, **L lock**, **X junk**, **V (hold) salvage** (the selected bag item, or all the junk), R compare, T details. Duo tab: both loadouts over the shared bag |
| Overview (`UBeyondOverviewWidget`) | Party cards, class header, vitals, main stats, affinity diamond, active passives, Bond status, duo abilities, key resources, loadout, quest progress, playtime & record |
| Quests (`UBeyondQuestLogWidget`) | Filters All / Main / Side; **T** tracks the selected quest |
| Pause (`UBeyondPauseMenuWidget`) | Resume, Character, Quests, World Map, Settings, Return to Title / Quit (asks twice) |

Cheat `Beyond.UI.DemoState` (`UI/BeyondUIDemo.*`) sets up a mid-game party for screenshots (set `Beyond.SaveProgress 0`
first).

## The art (phase 3, pass 21)

Generated in a separate session into `Art/UI/Source/<Category>/` (manifest: `Art/UI/Source/manifest.json`, prompts:
`Scripts/Art/ui_art_manifest.py`): 42 item icons, 40 skill / ability icons, 21 emblems (elements, currencies, sets,
classes, Bond, character), 2 backdrops, 2 ornaments, 10 quest pictures, 11 painted portraits (from the in-engine
captures in `Art/UI/Source/Captures/`), the main-menu relic, and 43 GLB models. Pass 21 imports the PNGs as UI textures
(BC7 icons, plain compression for the opaque pictures, no mips, never streamed) into `/Game/WorldsBeyond/UI/` and wires
them by name (items, skill nodes, abilities, `DA_UIArt`, set emblems, profiles). Re-run it whenever art lands.

## The systems (phase 4)

| Part | Where | Notes |
|---|---|---|
| **4A Combat stats** | `CharacterAttributeSet`, `AbilitySystem/BeyondCombatLibrary.*`, `Game/BeyondCombatSubsystem.*`, `Progression/BeyondStatEffects.*` | Critical Rate / Damage (crits on weapon and spell hits only, `Damage.Critical`), Elemental Mastery (own element and procs, +0.25 % per point), Verdant / Shadow / Frost / Flame affinity (more damage against foes of the element, resistance to its hits). Elements: Storm (Angel), Radiant (Ji-Woong), Eclipse (duo), the regions'. `Beyond.CritMode` (-1 auto: never while automation tests run, 0 never, 1 roll, 2 always). Skill nodes: **Element Damage** and **Passives** (On Crit / On Elemental Hit / On Kill → Buff Stat for a while (`UBeyondGE_TimedStats`) / Reduce Cooldowns / Heal, with an internal cooldown) |
| **4B Gear and money** | `Items/BeyondCurrency.h`, `Items/BeyondEconomySettings.*`, `Items/BeyondInventoryComponent.*`, `Items/BeyondEquipmentComponent.*` | Wallet (Gold, Crystals, Echo Shards, Reverie Fragments) in the bag; Accessory and Charm slots; upgrades +1 .. +15 on bag **or worn** items (every stat × (1 + 6 % × N); gold climbs with N^1.5, crystals from +5, shards from +10); lock (no junk, salvage or discard), junk, salvage (the tier's share plus half the upgrades' cost). Kills pay by rank and level, discovery and waystones pay too. *Project Settings → Game → Worlds Beyond Economy*. Cheat `Beyond.GiveCurrency <Gold|Crystals|EchoShards|ReverieFragments|All> <n>` |
| **4C Bond level** | `Player/BeyondPartyComponent.*`, `Progression/BeyondProgressionSettings.*` | Levels 1–10 from synergies, duo moves, main bosses and revives (never Bond Points). While the demigods are within 15 m: more damage dealt and less taken (not from the environment). Duo nodes can need a Bond level |
| **4D Quests** | `Quests/BeyondQuestTypes.*`, `Quests/BeyondQuestComponent.*` | Quests start once their prerequisites are met and complete when every objective is (flags — the world's `Discovered.*`, `Boss.*`, `Talked.*` too — kills, items carried, level, waystones, Bond level, other quests, talking to an NPC); rewards once, on completion; tracking. `Beyond.QuestTracking` (-1: on except while automation tests run). Cheat `Beyond.Quest list | complete <id> | track <id> | reset`. Playtime on the party |

Save **version 7**: wallet, item upgrades / locks / junk, the new slots, Bond level, quests, playtime. Version 6 saves
load with the starting purse, Bond level 1, and quests caught up from their flags without rewards.

## The content (phase 5, pass 23)

Eight rings and amulets (`/Game/WorldsBeyond/Items/Accessories/`), 5 % base Critical Rate on both demigods, nine new
skill nodes (Angel: Keen Static, Storm's Embrace, Elemental Resonance, Raging Momentum; Ji-Woong: Radiant Zeal,
Executioner's Edge, Sunlit Momentum; duo: Resonance at Bond level 3, Concord at Bond level 5) with the trees laid out in
six rows (ids, prerequisites and ranks unchanged), ability names, and `DA_QuestDatabase` from
`Scripts/Migration/quest_content.py`: six chapters (Storm over Mossbrook → Fractured Skies) and eleven side quests.

## Tests

`WorldsBeyond.UI.Style`, `WorldsBeyond.Prototype.UI`, `.CombatStats`, `.Economy`, `.Bond`, `.Quests`; captures (need a
renderer: run without `-nullrhi`, e.g. `-RenderOffscreen`) `WorldsBeyond.UICapture.Swatch`, `.Menus` (with the demo
party), `.Portraits` into `Saved/UICaptures/`.
