# Worlds Beyond — GAS prototype

GAS (Gameplay Ability System) now owns health, damage and death for both demigods and every enemy.
The old Blueprint combat (`BPC_DamageSystem`, `BPC_Attacks`, `BPI_Damagable`) still works on top of it.
Ji-Woong's new powers and the duo super move are designed in [Powers_Design.md](Powers_Design.md).
What comes next (leveling and skill tree, armor and loot, bosses, dialogue, the open world) is in [Roadmap.md](Roadmap.md),
one plan per system under [Plans/](Plans/).

## How it fits together

| Piece | Where | What it does |
|---|---|---|
| `UCharacterAttributeSet` | `Source/WorldBeyond/CharacterAttributeSet.*` | Health/Stamina, **Strength / Arcana / Defense / Level**, `IncomingDamage`/`IncomingHeal`; blocking, parrying, invincibility, Defense, brands, death, no friendly fire |
| Progression | `Progression/` | `UBeyondProgressionAttributeSet` (EXP, skill points; only on the demigods), `UBeyondProgressionSettings` (*Project Settings → Game → Worlds Beyond Progression*: EXP curve, max level, EXP per enemy rank), `FBeyondStatGrowth`. See *Leveling* below |
| `UBeyondProgressWidget` | `UI/BeyondProgressWidget.*` | Level badge + EXP bar (bottom left), "+25 EXP" pop-ups and the LEVEL UP banner (top centre), the F pickup prompt and loot toasts; drawn in C++, put into `W_PlayerHud` by the player controller |
| `UBeyondSaveGame` | `Game/BeyondSaveGame.*` | Slot `BeyondProgress`: each demigod's level, EXP, skill points, skill ranks and equipped items; the party's Bond Points, duo ranks, duo loadout and bag |
| Items and loot | `Items/`, `UI/BeyondInventoryWidget.*` | Item / armor set / database data assets, `UBeyondInventoryComponent` (the party's bag, on the player controller), `UBeyondEquipmentComponent` (per demigod: slots, stats, set effects), `UBeyondLootSettings` (*Project Settings → Game → Worlds Beyond Loot*), `ABeyondLootDrop`; the I screen. See *Items and loot* below |
| Skill trees | `Progression/BeyondSkillTree*`, `UI/BeyondSkillTreeWidget.*` | Tree data assets, `UBeyondSkillTreeComponent` (per demigod, skill points) and `UBeyondDuoSkillTreeComponent` (on the player controller, Bond Points, duo loadout); the K screen. See *Skill trees* below |
| `ABeyondCharacterBase` | `Characters/BeyondCharacterBase.*` | Parent of `BP_Angel`, `BP_Ji-Woong`, `BP_Enemy_Base`. Grants abilities once, team, ability input slots, death/revive, attack tokens. `GetCombatMesh()` is the mesh that animates (`Body` on MetaHumans); `Suppressed Abilities`; `Duo Role`; `Boss Bar Widget Class`; enemy AI perception detects hostile teams |
| Legacy bridge | `Characters/BeyondLegacyDamageBridge.*` | Mirrors GAS health into `BPC_DamageSystem` and fires its `OnDamageResponse` / `OnBlocked` / `OnDeath`, so existing hit-react, death and health-bar Blueprints keep working |
| `UBeyondCombatLibrary` | `AbilitySystem/BeyondCombatLibrary.*` | The one damage API: `ApplyDamage`, `ApplyDamageInfo` (takes `S_DamageInfo`), `HealActor`, `IsActorDead`, attack tokens… |
| `UBeyondCombatSubsystem` | `Game/BeyondCombatSubsystem.*` | World-wide damage feed (`OnDamageDealt`, used by the Bond meter), kill feed (`OnCharacterKilled`, used for EXP) and brands (Sunbrand) |
| `UBeyondGameplayAbility` | `AbilitySystem/BeyondGameplayAbility.*` | Base for all abilities: `InputTag`, AI hints (range, weight, heal threshold), `GetAimRotation` for player **and** AI, `Cooldown Duration` + `Cooldown Tags` (no effect asset needed), `FindHostilesInRadius`, `PlayMontageOnAvatar` |
| Ability classes | `AbilitySystem/Abilities/` | `MeleeCombo`, `Projectile`, `GroundStrike` (aimed AoE), `Dash`, `Brand`, `EquipWeapon`, `DuoStrike` — configured as the `GA_*` assets in `/Game/WorldsBeyond/Abilities/` |
| `FBeyondFX` | `AbilitySystem/BeyondFX.*` | One visual beat on an ability: Niagara/Cascade system, sound, camera shake, optional colour |
| `ABeyondWeapon` | `Weapons/BeyondWeapon.*` | Parent of both `BP_Weapon_Base`s; melee hit-scan (`HitScanStart` / `HitScanEnd`) |
| Party | `Player/BeyondPlayerController.*`, `Player/BeyondPartyComponent.*` | Tab swaps demigods, the other one is the AI buddy, auto-swap on death, stand next to a downed buddy for 3 s to revive, HUD follows the controlled demigod, **Bond meter**, **shared EXP from kills**, **saving progress**, **boss health bar**. `AttachToHUD` puts the Bond meter and the level display into `W_PlayerHud`'s canvas (a copy placed in the HUD in the designer wins) |
| `UBeyondBondMeterWidget` | `UI/BeyondBondMeterWidget.*` | The duo meter, built in C++ on three UI materials (`/Game/WorldsBeyond/UI/DuoMeter/`): an animated arc over the ability bar with Angel's and Ji-Woong's medallions on its ends; when full the Heaven's Judgment medallion appears with circling flames. Falls back to a plain bar without the materials; swap `Bond Widget Class` on `BP_PC` for a designed one |
| Aiming | `Characters/BeyondAimComponent.*`, `UI/BeyondCrosshairWidget.*` | Created on characters whose **Aim Settings** are on (Angel): shoulder camera with the weapon out, crosshair (shown by the player controller), the enemy under it glows, hold RMB to aim, casts face the crosshair |
| `ABeyondSpikeBurst` | `AbilitySystem/BeyondSpikeBurst.*` | Crystal spikes bursting out of the ground in a wave (Angel's E, `BP_Beyond_ArcaneSpikes`); `GroundStrike` spawns it via *Spike Burst Class* |
| `ABeyondCompanionController` | `AI/BeyondCompanionController.*` | Buddy AI: follow (with walk animation), defend the leader, use abilities by their AI hints, regroup; stands still while a cutscene (level sequence) has it or its leader |
| `ABeyondGameMode`, `ABeyondCheckpoint` | `Game/` | Party wipe → respawn at the last checkpoint (or player start) |
| `BTTask_BeyondActivateAbility` | `AI/` | Behavior tree task to run any GAS ability on an enemy |
| Enemies (Plan 3A) | `Enemies/`, `AI/BeyondEnemyController.*` | `ABeyondEnemyCharacter` set up from a `UBeyondEnemyDefinition` (roster entry), its C++ brain, elite affixes, `UBeyondEnemySubsystem` (spawning, `Beyond.Spawn`), spawners, the C++ anim instance for creatures without an anim Blueprint. See *Enemies* below |
| Telegraphs | `AbilitySystem/BeyondAreaStrike.*`, `Abilities/BeyondGA_AreaAttack.*` | Ground markers that fill, then hit (circle, ring, cone, line; meteors, lingering lava / poison); the enemies' attack ability |
| Bosses (Plan 3B) | `Enemies/BeyondBoss*`, `Game/BeyondBossArena.*`, `UI/BeyondBossBarWidget.*` | Phases with a health floor, twists, arenas, the C++ boss bar, `BeyondGA_Leap` / `_Beam` / `_Summon` / `_Teleport` / `_Pull`. See *Bosses* below |

## Controls

| Key | Angel | Ji-Woong |
|---|---|---|
| LMB | Magic spell (Blueprint) — goes where the crosshair is | **Sword combo** (`GA_JiWoong_SwordCombo`) — press during each swing to chain the next |
| RMB | **Aim** (hold): zoom over the shoulder, turn with the camera; cancels E's circle while aiming E | — |
| 1 | Draw / stow the staff (it rests on his back) | Draw / sheathe the sword (it rests on his left hip) |
| 2 | Heal (same as R) | Heal (same as R) |
| **Q** | Blink (lightning dash) | **Gilded Step** — golden dash through enemies, they're hit a moment later |
| **E** | **Arcane Spikes** (`GA_Angel_LightningStrike`) — hold to aim the purple circle, release: blue / purple crystal spikes burst out of the ground (RMB cancels) | **Sunbrand** — brand an enemy; your next sword hit detonates it |
| **R** | Heal | Heal |
| **G** | The duo power in the **duo loadout** (Heaven's Judgment until you pick another in the duo tree) when the Bond meter is full | same |
| Tab | Swap | Swap |
| **K** | **Skill trees** (pauses the game) | same |
| **F** | **Pick up** the loot you're standing at | same |
| **I** | **Equipment & inventory** (pauses the game) | same |

The Bond meter fills as the demigods deal and take damage, faster when both hit the same enemy.
Heaven's Judgment needs a full meter and both demigods alive within 15 m of each other; you can't Tab-swap while it plays.

The meter (Higgsfield concept A, built in C++ + UI materials, nothing to set up in UMG):
- A curved steel arc over the ability bar. Angel's medallion (his staff glyph from the ability icons over a turning
  rune ring) sits on the left end, Ji-Woong's gold sun on the right end.
- The fill runs left → right: **blue and purple swirling together on the left, solid gold on the right**. The energy
  flows and crackles, the front flashes on every Bond gain, and each medallion lights up as its side fills.
- **Full:** the Heaven's Judgment medallion pops in above the middle with **blue, purple and gold flames circling it**
  and "READY [G]"; the arc gets a sweeping shine. Using the move empties it and everything fades away.
- The duo medallion shows the **Icon** of `/Game/WorldsBeyond/Abilities/Duo/GA_Duo_HeavensJudgment` when it has one
  (import a PNG and set it), otherwise a drawn lightning bolt.
- Tuning without code: make a Blueprint child of `BeyondBondMeterWidget` and set it as *Bond Widget Class* on `BP_PC`.
  *Bond | Arc* has the colours (*Color Blue / Purple / Gold*, *Purple Stop*, *Gold Start*), *Flow Speed*, the
  glyphs, sizes / curve (*Arc Radius*, *Band Thickness*…), *Show Duo Icon While Charging* and the animation speeds.
  The look itself (noise, flames, frame) is the Custom node in each `M_UI_*` material.
- **It lives in `W_PlayerHud`:** the player controller adds it to the HUD's canvas, anchored bottom centre like the
  ability bar, and moves it into the new HUD on every swap. *Bond Meter Offset* on `BP_PC` positions it (default
  50 px up from the bottom centre so the arc ends flank the ability icons). To place it yourself, drag *Beyond Bond
  Meter Widget* from the Palette (User Created) into `W_PlayerHud` in the designer; the controller then uses that one.
  The Output Log says where it went ("Bond meter: … in W_PlayerHud_C (added by code / placed in the designer)"), or
  that the `M_UI_*` materials are missing.

Ji-Woong walks around relaxed with the sword on his left hip. He draws it when an enemy comes within 10 m or the moment
his sword combo starts, and puts it away after 8 s without enemies within 15 m. Tune it on
`/Game/WorldsBeyond/Abilities/JiWoong/GA_JiWoong_EquipWeapon` (*Auto* section; 0 turns a behaviour off).

Angel does the same with his staff: it rests diagonally on his back, he draws it near enemies or the moment he casts
(LMB spell, Lightning Strike, Arcane Bolt, Heaven's Judgment) and stows it after a quiet spell; "1" still toggles it.
His idle switches between the relaxed `MM_Idle` and the staff stance `UE5_WZ_Idle_Seq`. Tune it on
`/Game/WorldsBeyond/Abilities/Angel/GA_Angel_EquipStaff`.
When the hand takes the sword or staff (or puts it back), it glides between holster and hand over 0.2 s instead of
jumping (*Handoff Blend Time* on both equip abilities; 0 snaps).

Angel's aiming (`BP_Angel` → *Aim → Aim Settings*):
- **Staff out = combat camera + crosshair.** The camera eases over his right shoulder (*Ready Camera Offset*, added to
  the spring arm's own offset) so the screen centre is beside him, and a crosshair appears there. Everything he casts
  goes to the crosshair (the LMB spell, Arcane Bolt and E's circle all trace from the camera centre).
- **The enemy under the crosshair** turns the crosshair purple and glows (*Aim Highlight Material*,
  `/Game/WorldsBeyond/VFX/Aim/M_Beyond_AimHighlight`, used as the enemy mesh's overlay material). Characters are
  found with a thin sweep, so it doesn't have to be pixel-perfect.
- **Hold RMB to aim:** closer (*Aim Arm Length Scale*), a little zoom (*Aim Field Of View Change*), he turns with the
  camera and walks slower (*Aim Walk Speed Scale*); it takes the staff out. While E's circle is up, RMB still cancels it.
- **Casting turns him to the crosshair** (*Face Aim Montages*: the LMB / E cast and Arcane Bolt) in 0.12 s, so he never
  casts sideways.
- *Always Ready* shows the crosshair and shoulder camera even with the staff away. Ji-Woong has no aim settings.

Angel's E (`GA_Angel_LightningStrike`, kept its name): the aiming circle is purple (*Override Target Decal Color*), and
on release a wave of faceted crystal spikes bursts out of the ground from the centre outward — tallest in the middle,
leaning outward at the edge, each blue to purple with a flash of light — and sinks back after ~0.7 s. The damage
(100 in 2.5 m, stagger) lands as the first spikes break the surface. Look and timing are on
`/Game/WorldsBeyond/VFX/ArcaneSpikes/BP_Beyond_ArcaneSpikes` (rings, heights, colours, timing, light);
`M_Beyond_ArcaneCrystal` has *Color* and *Glow* parameters.

Ji-Woong's sword combo (`GA_JiWoong_SwordCombo`):
- **One click = one swing:** without another press inside a swing's window the combo ends after that swing. The
  windows are the montage's own `ResumeComboWindow` markers (*Combo Window Notify Name*), the ones the old Blueprint
  combo used, and each stays open 15 % longer than marked (*Combo Window Extension*). In play, `log LogBeyond Verbose`
  prints every window opening / closing and every press it caught.
- **Voice:** his attack line plays once when the combo starts, never again while you chain (*Combo Voice Line*).
- **Moving:** while he walks or runs the swings play on the upper body and his legs keep moving; standing still he
  keeps the full-body footwork, and starting / stopping mid-combo blends between the two (*Combo → Movement*).

Ability slots are set per character in **Input → Ability Input Bindings**, abilities in **AbilitySystem → Ability Set**
(`/Game/WorldsBeyond/Abilities/DA_AbilitySet_*`). Once LMB attacks are GAS abilities, tick **Disable Legacy Key Input**
(done for Ji-Woong: LMB → `IA_PrimaryAttack` → `Ability.Input.Primary`; Angel's LMB spell is still Blueprint).
Angel's hold-to-aim is **Aim Settings → Aim Action** (`IA_Aim`, RMB in `IMC_Default`).
**Legacy Keys To Disable** limits it to the keys listed — Ji-Woong lists only Left Mouse Button, so his Blueprint "1"
(draw / sheathe) keeps working.
The ability bar shows only the controlled demigod's Q / E / R abilities (after edit 1 below). Icons come from
`/Game/WorldsBeyond/Blueprints/Widgets/Data/DT_AbilityMetaData` (row name = ability class, e.g. `GA_JiWoong_Sunbrand_C`);
the new abilities use placeholder icons for now.
A shared ability can look different per demigod: **AbilitySystem → Ability Montages** on the character maps an ability to
the montage it plays (Angel: `GA_HealSpell → AM_Heal`, which carries the heal voice line).

## Blueprint edits left to do (in the editor)

C++ and scripts did everything else; these need node changes, which can't be scripted.

1. **Ability bar: only the current demigod's keys** — `W_AbilitesBar` → *FillAbilitiesBar*: delete **Get All Abilities**,
   add **Get Ability Bar Abilities** (Beyond | Combat | UI). Wire the ability system component into *Ability System*,
   the exec pins through it, and *Out Ability Handles* into the For Each loop. (`MinimumSlots` is already 3.)
   *Why:* the bar used to list every granted ability, including buddy-only ones, which showed as blank or repeated icons.
2. **Draw / sheathe on the upper body only** — `/Game/WorldsBeyond/Characters/MetaHuman/Anims/ABP_JI-Woong`
   (same setup as `/Game/Characters/Mannequins/Animations/ABP_Manny`):
   1. *Window → Anim Slot Manager*: add slot **UpperBody** to *DefaultGroup*.
   2. AnimGraph: drag off **Main States** → *New Save Cached Pose*, name it **MainStates**. (This unplugs Main States
      from Slot 'DefaultSlot' — expected, step 6 fills it again.)
   3. Right-click the empty graph, type `MainStates`, pick **Use cached pose 'MainStates'**. Do it twice (two nodes).
   4. Right-click → **Layered blend per bone**.
   5. Select the existing **Slot 'DefaultSlot'**, Ctrl+C, Ctrl+V. On the copy, *Details → Slot Name* →
      `DefaultGroup.UpperBody` (its title becomes *Slot 'UpperBody'*).
   6. Wire it up — the goal:
      ```
      [Use cached pose 'MainStates'] ───────────────────────────────► Base Pose     ┐
                                                                                    ├ [Layered blend per bone] ─► Source [Slot 'DefaultSlot'] ─► Control Rig ─► Output Pose
      [Use cached pose 'MainStates'] ─► Source [Slot 'UpperBody'] ──► Blend Poses 0 ┘
      ```
      Leave the **Locomotion** cache and the Control Rig / Output Pose part as they are.
   7. Select the Layered blend per bone → *Details → Layer Setup → [0] → Branch Filters* → **+** → Bone Name `spine_01`,
      Blend Depth 1; tick *Mesh Space Rotation Blend*.
   8. Compile, save. Until this is done the draw / sheathe play full body (the log says so once).
3. *(Optional, fine tuning)* **Sword on the hip** — in `/Game/MetaHumans/Common/Female/Medium/NormalWeight/Body/metahuman_base_skel`
   add a socket named `sword_hip_socket` on `pelvis`, preview `SM_Sword` on it and place it on the left hip. When the socket
   exists it's used instead of the built-in offset.
4. *(Optional)* `AIC_Enemy_Boss`: delete the *DoOnce → add BossHealthbarWidgetRef to PlayerHUDRef* chain. The player
   controller shows the boss bar now (for either demigod, across swaps, following GAS damage).
5. *(Optional)* `BP_Ji-Woong`: delete the **F** key dash section (it activates Blink, which Gilded Step replaced on Q).
6. *(Optional)* `BP_BossFightCutscence`: only start the cutscene when the overlapping actor is the player-controlled pawn
   (the buddy walking in first triggers it today).
7. *(Optional)* Delete the *Character Follow* section (Event Unpossessed → Spawn Default Controller → Follow Player) in both
   demigods, and the *Create Widget W_PlayerHud / Add to Viewport* chain in their BeginPlay. The buddy controller ignores the
   first, the player controller owns the HUD; they only cause a stray AI controller per swap and an "already added" warning.
8. **Staff draw / stow on the upper body** — `/Game/WorldsBeyond/Characters/MetaHuman/Anims/ABP_Angel`: the same graph edit
   as step 2 (steps 2–8 there; skip step 1, the **UpperBody** slot already exists on the shared skeleton). Until this is
   done Angel stops walking while he draws or stows (the log says so once). `ABP_Angel_Staff` is no longer used.
9. *(Optional, fine tuning)* **Staff on the back** — pass 4 works out where the staff rests from the draw animation and
   writes it to the report. To move it, add a socket named `staff_back_socket` on `spine_05` in `metahuman_base_skel`,
   preview the staff on it and place it; when the socket exists it's used instead.

Done earlier: TakeDamage routed into GAS, attack tokens, enemy heal, death handler, test logic removed, `GC_Blink` hair fix
(`GC_Dash` is no longer used), sword grip socket, Ji-Woong's upper-body slot (step 2).

## Leveling

- **EXP** comes from kills by either demigod and goes to **both** (a downed buddy too). An enemy is worth its rank's
  EXP (Regular 25, Elite 60, Mini-boss 300, Boss 1500) +10 % per enemy level above 1; *Experience Reward* on an enemy
  overrides it. Ranks: `BP_Enemy_Base` / `Melee` / `Ranged` Regular, `BP_Enemy_Mage` Elite, `BP_Enemy_Boss` Boss.
- **Levels:** level L → L+1 needs 100 × L^1.5 EXP (100, 283, 520, 800 …), max level 50. Each level gives a skill
  point (spent in the skill tree, Plan 1B) and the demigod's *Stat Growth*; a level-up refills health and stamina,
  plays *Level Up FX* and sends `Event.Progression.LevelUp` (an ability can trigger on it).
- **Stats:** melee damage × (1 + Strength / 100); projectile / explosion (spell) damage × (1 + Arcana / 100); damage
  taken × 100 / (100 + Defense). Angel starts with Arcana 10, Defense 4 (+3 Arcana, +10 health per level); Ji-Woong
  with Strength 10, Defense 8 (+3 Strength, +14 health, +2 Defense per level). Enemies with a *Starting Level* above 1
  get their *Stat Growth* too.
- **Saving:** on every level-up, at each checkpoint and when play ends; loaded when the party forms. Console:
  `Beyond.GiveExperience 500`, `Beyond.ResetProgress` (deletes the save, back to level 1), `Beyond.SaveProgress 0`
  (start fresh and write nothing; the automation tests do this so they never touch your save).
- Tuning lives in *Project Settings → Game → Worlds Beyond Progression* (saved to `Config/DefaultGame.ini`) and on each
  character under *Progression* / *AbilitySystem → Attributes*.

## Skill trees

- **K** opens them (the game pauses): a tab for Angel, one for Ji-Woong and the shared **Duo** tree; Q / E switch.
  Hover a node for what it does and what it needs; **hold the left mouse button** on it to unlock the next rank;
  **R** twice resets the tree you're on (every point back); K / Esc closes.
- Personal trees cost the demigod's **skill points** (one per level). The duo tree costs **Bond Points**: one every 3
  party levels and one for each main boss (Boss rank).
- Nodes raise stats (health, stamina, Strength, Arcana, Defense) or **rank up an ability**: each rank is +15 % damage
  and −8 % cooldown on that ability. Duo nodes rank up the duo moves, change the Bond meter, or unlock **duo powers**.
- **Duo loadout:** right-click an unlocked duo power (or *Heaven's Wrath* for Heaven's Judgment) to put it on G. The
  Bond meter's medallion shows its icon.
- Trees are data: `/Game/WorldsBeyond/SkillTrees/DA_SkillTree_Angel`, `_JiWoong`, `_Duo`. Add or tune nodes there
  (keep node ids: saves use them). Which tree a demigod uses: *Progression → Skill Tree* on `BP_Angel` / `BP_Ji-Woong`;
  the duo tree: *Party → Skill Tree → Duo Skill Tree Asset* on `BP_PC`.
- Node lists: [Plans/01_Leveling_SkillTree.md](Plans/01_Leveling_SkillTree.md#the-trees-as-built-tune-them-in-the-editor).

## Items and loot

- Enemies drop loot when the party kills them: Regular 12 %, Elite 35 %, mini-bosses always 2 items (one a set
  piece), main bosses always 3 (a set piece and a weapon), at the enemy's level. Drops glow in their tier's colour:
  white Common, green Uncommon, blue Rare, purple Epic, gold Legendary.
- Walk up to a drop: the HUD shows **F  Name (Tier)**; F puts it in the party's bag (60 items) and a toast lists it.
- **I** opens the equipment screen (paused): Q / E switch demigod; click a bag item to wear it (what was in that slot
  goes back to the bag), click a worn item to take it off; hover for details: stats against what's worn (green
  better, red worse), the set and both its bonuses. A green arrow marks an upgrade, a red cross an item that demigod
  can't use (the other's weapon type). **X** twice throws the hovered bag item away. K goes to the skill tree, I / Esc
  close.
- **Armor sets** (all four pieces Rare or better): 2 pieces give a stat bonus, 4 switch on the effect —
  **Venomweave** (Q / E / R coat weapon and spells in venom for 6 s: hits poison), **Stormforged** (hits can chain
  lightning to 3 more enemies), **Sunforged** (hits burn; less damage taken above half health). Full list:
  [Plans/02_Items_Armor_Loot.md](Plans/02_Items_Armor_Loot.md).
- A fresh game (or a save from before items) starts with each demigod's Common weapon and Wanderer's Chest and Boots.
- Items are data in `/Game/WorldsBeyond/Items/` (`Sets/`, `Armor/`, `Weapons/`, `DA_ItemDatabase`); give an item an
  **Icon** and the screens show it instead of the drawn slot glyph. Drop rates, tier scaling, the drop glow, pickup
  range and starter kit: *Project Settings → Game → Worlds Beyond Loot*. A boss's own drops: *Loot → Guaranteed Loot*
  on its Blueprint.
- Saved with the rest of the progress (the bag and what each demigod wears); `Beyond.ResetProgress` empties the bag
  and hands out the starter kit again.

## Enemies

- **The roster** lives in `/Game/WorldsBeyond/Enemies/` (`DA_EnemyRoster`, `Roster/DA_Enemy_*`, `Affixes/DA_Affix_*`,
  `Abilities/<Enemy>/GA_*`); full list in [Plans/03_Enemies_Bosses.md](Plans/03_Enemies_Bosses.md). Forest: Timber
  Wolf, Raider, Raider Slinger, Treant; corrupted woods: Hollow Wraith, Cursed Knight, Corrupted Beast. They are
  Paragon stand-ins with a region tint: put real art in a definition's *Looks* (Mesh, Anim Class or Locomotion).
- **Spawning:** drop an **Enemy Spawner** in a level and fill *Entries* (enemy, count, level), or place a
  `BeyondEnemyCharacter` and set its *Definition*. Console: `Beyond.Spawn forest_wolf 3 molten x1`
  (`<id> [level] [elite | <affix id>...] [x<count>]`), `Beyond.ListEnemies`, `Beyond.KillEnemies`.
- **How they fight:** they wander near home, see you in a cone (or when you get close, or hit them), call idle
  packmates in, and use their abilities by the AI hints (range, weight, cooldown). Only *Max Attack Tokens* of them
  swing at one demigod at once; the others circle. Casters keep their distance. Drag one too far from home and it
  walks back, invulnerable, and refills. When the party wipes, every enemy is back home at full health.
- **Telegraphs:** red markers fill on the ground before an attack lands; the spot is fixed when the attack starts, so
  step out. Hazard pools stay lit while they burn. Colours: *Project Settings → Game → Worlds Beyond Enemies*.
- **Elites** (spawner *Elite Chance*, or `elite` / an affix id in `Beyond.Spawn`): ×1.8 health, +30 Strength /
  Arcana, bigger, a gold plate frame and a name like "Molten Raider". Affixes: Molten, Venomous, Stormcharged, Warded,
  Juggernaut, Swift, Vampiric, Brood.
- **Plates:** health, level and elite names float over enemies in a fight (`UBeyondEnemyPlatesWidget`; *Enemy Plates
  Widget Class* on `BP_PC`).
- **Tuning:** each `DA_Enemy_*` (stats, growth, AI ranges, hit reactions), each `GA_*` (damage, wind-up, cooldown,
  *AI Attack Token Cost*), *Project Settings → Game → Worlds Beyond Enemies* (elite numbers, affix count, telegraph
  material, plate range).

## Bosses

- **Kael'thar, the Molten Colossus** and **Hrimgar, the Frost Troll King** (main bosses), **Gorehide, the Pale
  Alpha** and **Veyla, the Hollow Voice** (mini-bosses): `/Game/WorldsBeyond/Enemies/Bosses/` (`DA_Boss_*`,
  `BP_Boss_*`). Phases, moves and twists: [Plans/03_Enemies_Bosses.md](Plans/03_Enemies_Bosses.md#plan-3b--bosses).
- **Arena:** place a **Boss Arena**, set *Boss* and *Boss Level*, *Arena Radius* (hazards and the seal ring) and
  *Engage Radius*, and put a checkpoint at its entrance. The boss waits until a demigod comes close (or hits it), then
  the ring seals. A party wipe resets the fight; a beaten **story boss** stays beaten (saved); `Beyond.ResetBosses`
  brings them back. The test arena (`/Game/WorldsBeyond/Maps/TestArena`) has all four.
- **Phases:** the boss bar's notches show where the next phase starts; no hit skips one. At each threshold the boss
  can't be hurt for a moment (the bar shimmers) while it changes phase, then the twist announces itself under the bar.
  `Beyond.BossPhase 3` jumps the nearest boss to a phase.
- **Rewards:** rank loot plus the boss's signature weapon (*Loot → Guaranteed Loot*); main bosses give a Bond Point.
- **The hero Blueprints:** the Paragon `*PlayerCharacter` Blueprints were reparented to `BeyondBossCharacter` (their
  anim Blueprints cast to them); backups are in `Saved/MigrationBackups/`. Boss montages use their **FullBody** slot.

## Level checklist (`MAP_Demo_Main`)

- Place a **BeyondCheckpoint** before each encounter (the arrow is the respawn point).
- Press **P** to check the navmesh covers every arena and the boss platforms.
- Starter Content materials used by `SM_Axe` are missing: *Add → Add Feature or Content Pack → Starter Content*.

## Scripts and test

Run with the editor closed (build the C++ first):

```
UnrealEditor-Cmd.exe WorldBeyond.uproject -run=pythonscript -script=<repo>/WorldBeyond/Scripts/Migration/migrate_pass9.py -unattended -nosplash -NullRHI
UnrealEditor-Cmd.exe WorldBeyond.uproject -run=pythonscript -script=<repo>/WorldBeyond/Scripts/Migration/fit_outfits.py -unattended -nosplash -NullRHI
UnrealEditor-Cmd.exe WorldBeyond.uproject -run=pythonscript -script=<repo>/WorldBeyond/Scripts/Migration/cleanup_legacy.py -unattended -nosplash -NullRHI
UnrealEditor-Cmd.exe WorldBeyond.uproject -ExecCmds="Automation RunTests WorldsBeyond.Prototype;Quit" -unattended -nullrhi -nosplash -TestExit="Automation Test Queue Empty"
```

- `migrate_prototype.py` — pass 1 (GAS migration), already applied.
- `migrate_pass2.py` — pass 2, already applied: Lightning Strike, Gilded Step, Sunbrand, sword equip, Heaven's Judgment,
  ability sets, `IA_Duo` on G, duo roles, boss bar class, `Montage_SwordCombo` hit-scan / combo-window notifies,
  ability bar rows.
- `migrate_pass3.py` — pass 3, already applied: key 2 → heal slot, Angel's heal montage, Ji-Woong's hip holster /
  upper-body draw / relaxed idle / auto draw & sheathe, 3-slot ability bar, effect lifetimes.
- `migrate_pass4.py` — pass 4: Ji-Woong's LMB on the GAS sword combo (`IA_PrimaryAttack` on LMB, his Blueprint LMB event
  off), combo window +15 %, voice line once per combo, upper-body swings while moving; Angel's staff (`GA_Angel_EquipStaff`:
  back holster, MagicStaff draw / stow, idle switch, auto draw / stow, old Blueprint equip suppressed). The staff's grab /
  release times and resting place come from the pack's animations; check them in `last_run_pass4.txt`.
- `migrate_pass5.py` — pass 5: Angel's aiming (`IA_Aim` on RMB, `M_Beyond_AimHighlight`, *Aim Settings* on `BP_Angel`)
  and his E as crystal spikes (`M_Beyond_ArcaneCrystal`, `SM_Beyond_CrystalSpike` made with Geometry Script — the engine
  cone if that fails —, `BP_Beyond_ArcaneSpikes`; the yellow lightning cue removed, purple circle). The materials and
  the mesh are only created when missing, so editor tweaks survive a re-run. Report: `last_run_pass5.txt`.
- `migrate_pass6.py` — pass 6: the duo meter's UI materials `M_UI_BondArc`, `M_UI_DuoFlames`, `M_UI_DuoMedallion` in
  `/Game/WorldsBeyond/UI/DuoMeter/` (procedural HLSL in Custom nodes; uses the existing `crescent-staff` glyph and the
  FX pack's `T_ky_magicCircle020`). Rebuilt on every run, so hand edits to these three are overwritten. Report:
  `last_run_pass6.txt`.
- `migrate_pass7.py` — pass 7 (Plan 1A, leveling): enemy ranks, the demigods' names, starting Strength / Arcana /
  Defense, *Stat Growth* and *Level Up FX*. The level display needs no asset. Report: `last_run_pass7.txt`.
- `migrate_pass8.py` — pass 8 (Plan 1B, skill trees): the three tree data assets, `GA_Duo_EclipseBrand` and
  `GA_Duo_TempestAegis` (copies of Heaven's Judgment with their variant on), the trees on `BP_Angel` / `BP_Ji-Woong` /
  `BP_PC`, `IA_SkillTree` on K. Trees and duo powers are only created when missing; `BEYOND_REBUILD_TREES=1` rebuilds
  the trees (in PowerShell `$env:BEYOND_REBUILD_TREES = "1"` first). Report: `last_run_pass8.txt`.
- `migrate_pass9.py` — pass 9 (Plan 2, items and loot): the three armor sets, 24 armor pieces, 6 weapons and
  `DA_ItemDatabase` in `/Game/WorldsBeyond/Items/`, `IA_Interact` on F and `IA_Inventory` on I (on `BP_PC`),
  Ji-Woong's old Blueprint F event (Blink) switched off. Items are only created when missing;
  `BEYOND_REBUILD_ITEMS=1` rebuilds them (asset names stay, so saved items still load). Report: `last_run_pass9.txt`.
- `migrate_pass10.py` — pass 10 (Plan 3A, enemies): roster montages, `M_Beyond_Telegraph` / `M_Beyond_EnemyTint`
  (rebuilt every run), two projectiles, the enemies' abilities and ability sets, 8 affixes, 7 roster enemies,
  `DA_EnemyRoster`, and the test arena map (floor, navmesh, demigods, checkpoint, two camps). Data is only created when
  missing; `BEYOND_REBUILD_ENEMIES=1` rebuilds it, `BEYOND_REBUILD_ARENA=1` rebuilds the map (the arena's navmesh is
  rebuilt every run). Report: `last_run_pass10.txt`.
- `migrate_pass11.py` — pass 11 (Plan 3B, bosses): reparents the four Paragon hero Blueprints to `BeyondBossCharacter`,
  makes `BP_Boss_*`, FullBody boss montages, boss abilities and phase sets, the four boss definitions (added to the
  roster), four signature weapons (added to `DA_ItemDatabase`) and four arenas in the test arena. Data is only created
  when missing (`BEYOND_REBUILD_BOSSES=1` rebuilds it); arenas only when the map has none. Report: `last_run_pass11.txt`.
- `migrate_pass12.py` — pass 12 (Plan 3 playtest fixes): each demigod's hit voice cue (`Angel_hitReact`,
  `JI-Woong_HitReact`, played by `BP_Angel` / `BP_Ji-Woong` on every hit) gets its own concurrency: one line at a time,
  never cut off, none within 4 s of that demigod's last (*Retrigger Time*); Ji-Woong's old Blueprint LMB event is
  switched off again (it had dropped out of *Legacy Keys To Disable*); the duo moves' *Approach* (Ji-Woong's rush to the
  locked enemy: dash animations, golden trail, blink) and the TestArena arenas' *Party Rush* (the dash in before the
  walls rise). Needs the C++ build with `UBeyondRushComponent`. Report: `last_run_pass12.txt`.
- `fit_outfits.py` — snug-fits the outfits (see *Clothing fit* below). Needs the **GeometryScripting** plugin, which
  `WorldBeyond.uproject` now enables (editor only).
- All passes are idempotent and back up every asset they save to `Saved/MigrationBackups/<timestamp>/`; shared helpers live in
  `migration_common.py`. Pass 1 no longer overwrites ability sets that already exist.
- `cleanup_legacy.py` — dry run by default. To delete, in PowerShell run `$env:BEYOND_CLEANUP_APPLY = "1"` first
  (then `Remove-Item Env:BEYOND_CLEANUP_APPLY`). Never deletes anything still referenced; report in
  `Saved/MigrationBackups/cleanup_report.txt`. Afterwards, in the editor: right-click `Content` → **Fix Up Redirectors**.
  `GA_AOEAttack` and `GA_Dash` are unused now and can go in a later cleanup.
- `WorldsBeyond.Prototype.Smoke` — party + buddy, swaps without duplicate abilities, GAS damage mirrored into the old
  component, no friendly fire, attack tokens, death, revive.
- `WorldsBeyond.Prototype.Powers` — the buddy standing still during the intro, MetaHuman combat mesh, buddy walk fix,
  enemy perception, Ji-Woong's sword, LMB on the GAS combo, the combo's voice line once / full-body footwork when standing /
  upper body while walking, Angel's staff (back, idle, drawn when casting), Angel's aiming (crosshair and shoulder
  camera with the staff out, the enemy under the crosshair targeted and glowing, a cast turning him to the crosshair,
  RMB mapped, none for Ji-Woong), E's spike burst (spawned, blue / purple only, cleaned up), the duo meter (its
  three materials exist, it is the arc, full shows the duo medallion and flames, spent hides them),
  Gilded Step, Sunbrand, the buddy's sword combo, ability bar refresh on swap, Lightning Strike, Bond meter + Heaven's
  Judgment, boss bar; plus the hip holster and idle switch, quick-draw when the combo starts, animated sheathe, auto draw /
  sheathe, Stop Anim Montage reaching Body (the combo window), the ability bar list, key 2 and Angel's heal montage, no
  swapping during the duo and no effects left on Angel after it. (It skips the intro cutscene, which pins the demigods in
  place while it plays.)
- `WorldsBeyond.Prototype.Progression` — both demigods earn EXP (enemies don't), rank rewards climb, killing a regular
  enemy gives both demigods its EXP, crossing the threshold gives a level, a skill point, Stat Growth and a refill and
  shows the banner naming both, Strength scales melee damage, environment damage ignores stats, 100 Defense halves
  damage, the save game round-trips, restoring a save sets level / EXP / points / stats, the level display is in the
  HUD.
- `WorldsBeyond.Prototype.SkillTree` — the three trees exist and are valid, level / prerequisite / point gates with
  their reasons, unlocking stats, an ability rank (spec level 2) and max health (current health follows), max rank,
  reset refunds and removes everything, Bond Points at levels 3 / 6 / 9 and from a Boss-ranked kill, Bond gain from the
  duo tree, Eclipse Brand granted to both demigods off the slot then put on G (Heaven's Judgment off it), a locked power
  refused, duo ranks on both demigods, the duo reset, the K screen (opens paused on the leader's tree, three tabs, hold
  to unlock, explains a locked node, closes unpaused), saved ranks restored without spending, Tempest Aegis halving a
  hit and reflecting it.
- `WorldsBeyond.Prototype.Items` — the item database (names, stats, weapon types, three sets with one piece per slot),
  tier / level scaling and bonus stats, set pieces never below Rare, the starter kit, equipping from the bag (stats
  up, the old item back in the bag, taking off removes the stats), a sword refused on Angel but fine on Ji-Woong,
  2-piece bonus, full sets (on test sets with every proc forced): Q starts Venom Infusion, an infused hit poisons and
  it ticks, breaking the set ends the infusion, set damage never sets off set effects, chain lightning hits a nearby
  enemy, Sunfire's −15 % above half health and its burn ticking; a Boss-ranked roll (3 items, a set piece and a
  weapon, at the enemy's level), the kill dropping them, F picking up the nearest drop (toast), a full bag refusing,
  the I screen (paused, two tabs, equip / refuse / take off / discard, sorted bag, K switching to the skill tree),
  items in the save game.
- `WorldsBeyond.Prototype.Enemies` (in the test arena) — the roster and affixes exist, every roster enemy spawns at level 5
  set up from its definition (team, health, brain, mesh, C++ anim instance, abilities and hit reaction, DefaultSlot
  montages), `Beyond.Spawn forest_wolf 3 molten`, a raider runs at Angel (anim speed follows), a wolf sees him,
  attack tokens capped with the rest circling and given back when they die, a telegraph missing when you step out and
  hitting when you stay, the treant's smash locking its marker, leash home (no damage on the way, refilled), a montage
  death without ragdoll and its EXP, Juggernaut / Swift / Venomous / Warded (40 % spells until a melee hit) / Molten
  (burn, lava) / Vampiric / Stormcharged / Brood, the plates, a runtime spawner, the wipe sending enemies home.
- `WorldsBeyond.Prototype.Bosses` (in the test arena) — the four bosses on their hero Blueprints (skin, hero anim
  Blueprint, no camera, phase 1 tag, health floor), the C++ boss bar for one and two bosses, a huge hit stopping at the
  threshold, the invulnerable transition, phase tag and abilities, Hrimgar's summons and enrage, the kill (Bond Point,
  Glacierheart Rod, adds gone), Kael'thar's lava ring and pools (gone with him), Veyla's clones (no bar, 30 % damage),
  Gorehide's pack halving damage, an arena waking and sealing, darkness, the wipe reset, a beaten story boss staying
  beaten.
- Every PIE test sets `Beyond.SaveProgress 0` while it runs and switches it back only once PIE has ended (the party
  saves when play ends), so your saved levels are never loaded or overwritten.
- Also in the editor: *Tools → Test Automation*, filter WorldsBeyond.

In play: `showdebug abilitysystem` (PageUp/PageDown cycles actors), the Gameplay Debugger (`'`) for enemy
BT/perception/EQS, and `log LogBeyond Verbose` for dash / strike / draw / sheathe traces.

## Clothing fit

The Streetwear outfits were modelled for a bigger body, so they float off the MetaHumans. `fit_outfits.py` fixes that
geometrically, for every outfit under a character's `Clothing` folder (footwear is skipped):

- cloth further than 2 cm from the skin is pulled in, keeping 60 % of the extra looseness (max 3 cm per vertex), so the
  garment still drapes and its folds survive; cloth that clips into the body is pushed out to 1 cm;
- anything more than 10–15 cm away (the hood, drawstrings) and vertices skinned to the neck / head are left alone;
- the movement is smoothed across the mesh, every LOD is fitted, normals are recomputed.

It writes `<Outfit>_Fitted` next to each original (never touched) and puts it on `BP_Angel` / `BP_Ji-Woong`.
Look at both demigods up close in a T-pose and in motion. Tune without editing the script by setting these in
PowerShell before running it (the values used are printed at the top of `last_run_fit.txt`):

| Variable | Default | Looser | Tighter |
|---|---|---|---|
| `BEYOND_FIT_KEEP` (share of the extra looseness kept) | 0.6 | 0.75 | 0.45 |
| `BEYOND_FIT_SNUG` (cm always kept) | 2.0 | 3.0 | 1.5 |
| `BEYOND_FIT_MIN_GAP` (cm from the skin, fixes poke-through) | 1.0 | 1.5 | 0.6 |
| `BEYOND_FIT_MAX_PULL` (cm a vertex may move in) | 3.0 | 2.0 | 5.0 |

e.g. `$env:BEYOND_FIT_KEEP = "0.75"`, run the script, then `Remove-Item Env:BEYOND_FIT_KEEP`. A part that shouldn't move
→ raise `FREE_FROM` / `FREE_BEYOND` or add its bone to `PROTECT_BONES` in the script. Re-running always starts from the
originals. To go back: `$env:BEYOND_FIT_REVERT = "1"`, run it again, `Remove-Item Env:BEYOND_FIT_REVERT`.

If a garment needs real re-tailoring (a different cut, sleeves that are too long), refit it in the MetaHuman tools
instead: make an **Outfit Asset** (*Physics → Outfit Asset*, *Resizable Outfit* template) from the garment's Cloth Asset,
use a **Sized Outfit Source** with *Sized Outfit* ticked so it resizes to the body, add it to the character's *Outfit
Clothing* in the MetaHuman editor and assemble again. The fitted copies can then be deleted.

## Known limits

- Montage root motion doesn't move MetaHumans: character movement reads root motion from `CharacterMesh0`, which is
  empty; montages play on `Body`. Leaps and lunges animate in place (the dash moves the character itself).
- Only Angel has a heal animation and voice; add a montage for Ji-Woong under *Ability Montages* on `BP_Ji-Woong`.
- Spawned effects stop after 5 s unless their ability manages them (*Max Lifetime* on each effect; 0 = no limit).
- Ji-Woong's LMB no longer runs his Blueprint combo, so that Blueprint's *Attacking* flag (what `BPI_Damagable → Is
  Attacking` returns to enemies) stays false; `UBeyondCombatLibrary::IsActorAttacking` reports GAS attacks correctly.
- Angel's LMB spell is still Blueprint and plays its voice line on every cast. Its trace (camera centre, Visibility)
  matches the crosshair, but unlike the GAS casts it doesn't skip the buddy if he stands in the line of fire.
- The outfit fit is geometric: the cloth follows the body's skinning, it isn't simulated.
- Items change stats, not looks: no armor or weapon meshes yet (each item has an unused *Visual Mesh* slot), and the
  weapon in hand stays the demigod's own sword / staff whatever is equipped.
- Enemies are Paragon stand-ins: no wolves, treants or wraiths are imported. Their capsules come from the mesh bounds
  (pass 10 logs them); Kael'thar at ×2.5 is wider than the navmesh's agent, so keep his arena open.
- Telegraph cones and lines assume the decal's U axis runs along the facing; if one points backwards in play, set
  *ForwardSign* to -1 in `M_Beyond_Telegraph`.
