# Worlds Beyond — GAS prototype

GAS (Gameplay Ability System) now owns health, damage and death for both demigods and every enemy.
The old Blueprint combat (`BPC_DamageSystem`, `BPC_Attacks`, `BPI_Damagable`) still works on top of it.
Ji-Woong's new powers and the duo super move are designed in [Powers_Design.md](Powers_Design.md).

## How it fits together

| Piece | Where | What it does |
|---|---|---|
| `UCharacterAttributeSet` | `Source/WorldBeyond/CharacterAttributeSet.*` | Health/Stamina, `IncomingDamage`/`IncomingHeal`; blocking, parrying, invincibility, brands, death, no friendly fire |
| `ABeyondCharacterBase` | `Characters/BeyondCharacterBase.*` | Parent of `BP_Angel`, `BP_Ji-Woong`, `BP_Enemy_Base`. Grants abilities once, team, ability input slots, death/revive, attack tokens. `GetCombatMesh()` is the mesh that animates (`Body` on MetaHumans); `Suppressed Abilities`; `Duo Role`; `Boss Bar Widget Class`; enemy AI perception detects hostile teams |
| Legacy bridge | `Characters/BeyondLegacyDamageBridge.*` | Mirrors GAS health into `BPC_DamageSystem` and fires its `OnDamageResponse` / `OnBlocked` / `OnDeath`, so existing hit-react, death and health-bar Blueprints keep working |
| `UBeyondCombatLibrary` | `AbilitySystem/BeyondCombatLibrary.*` | The one damage API: `ApplyDamage`, `ApplyDamageInfo` (takes `S_DamageInfo`), `HealActor`, `IsActorDead`, attack tokens… |
| `UBeyondCombatSubsystem` | `Game/BeyondCombatSubsystem.*` | World-wide damage feed (`OnDamageDealt`, used by the Bond meter) and brands (Sunbrand) |
| `UBeyondGameplayAbility` | `AbilitySystem/BeyondGameplayAbility.*` | Base for all abilities: `InputTag`, AI hints (range, weight, heal threshold), `GetAimRotation` for player **and** AI, `Cooldown Duration` + `Cooldown Tags` (no effect asset needed), `FindHostilesInRadius`, `PlayMontageOnAvatar` |
| Ability classes | `AbilitySystem/Abilities/` | `MeleeCombo`, `Projectile`, `GroundStrike` (aimed AoE), `Dash`, `Brand`, `EquipWeapon`, `DuoStrike` — configured as the `GA_*` assets in `/Game/WorldsBeyond/Abilities/` |
| `FBeyondFX` | `AbilitySystem/BeyondFX.*` | One visual beat on an ability: Niagara/Cascade system, sound, camera shake, optional colour |
| `ABeyondWeapon` | `Weapons/BeyondWeapon.*` | Parent of both `BP_Weapon_Base`s; melee hit-scan (`HitScanStart` / `HitScanEnd`) |
| Party | `Player/BeyondPlayerController.*`, `Player/BeyondPartyComponent.*` | Tab swaps demigods, the other one is the AI buddy, auto-swap on death, stand next to a downed buddy for 3 s to revive, HUD follows the controlled demigod, **Bond meter**, **boss health bar** |
| `UBeyondBondMeterWidget` | `UI/BeyondBondMeterWidget.*` | The duo meter, built in C++ on three UI materials (`/Game/WorldsBeyond/UI/DuoMeter/`): an animated arc over the ability bar with Angel's and Ji-Woong's medallions on its ends; when full the Heaven's Judgment medallion appears with circling flames. Falls back to a plain bar without the materials; swap `Bond Widget Class` on `BP_PC` for a designed one |
| Aiming | `Characters/BeyondAimComponent.*`, `UI/BeyondCrosshairWidget.*` | Created on characters whose **Aim Settings** are on (Angel): shoulder camera with the weapon out, crosshair (shown by the player controller), the enemy under it glows, hold RMB to aim, casts face the crosshair |
| `ABeyondSpikeBurst` | `AbilitySystem/BeyondSpikeBurst.*` | Crystal spikes bursting out of the ground in a wave (Angel's E, `BP_Beyond_ArcaneSpikes`); `GroundStrike` spawns it via *Spike Burst Class* |
| `ABeyondCompanionController` | `AI/BeyondCompanionController.*` | Buddy AI: follow (with walk animation), defend the leader, use abilities by their AI hints, regroup; stands still while a cutscene (level sequence) has it or its leader |
| `ABeyondGameMode`, `ABeyondCheckpoint` | `Game/` | Party wipe → respawn at the last checkpoint (or player start) |
| `BTTask_BeyondActivateAbility` | `AI/` | Behavior tree task to run any GAS ability on an enemy |

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
| **G** | **Heaven's Judgment** (duo) when the Bond meter is full — its own slot left of the meter | same |
| Tab | Swap | Swap |

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
  Its position is *Bond Meter Offset* on `BP_PC` (default 50 px up from the bottom centre so the arc ends flank the
  ability icons). The look itself (noise, flames, frame) is the Custom node in each `M_UI_*` material.

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

## Level checklist (`MAP_Demo_Main`)

- Place a **BeyondCheckpoint** before each encounter (the arrow is the respawn point).
- Press **P** to check the navmesh covers every arena and the boss platforms.
- Starter Content materials used by `SM_Axe` are missing: *Add → Add Feature or Content Pack → Starter Content*.

## Scripts and test

Run with the editor closed (build the C++ first):

```
UnrealEditor-Cmd.exe WorldBeyond.uproject -run=pythonscript -script=<repo>/WorldBeyond/Scripts/Migration/migrate_pass6.py -unattended -nosplash -NullRHI
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
  three materials compile, it is the arc, full shows the duo medallion and flames, spent hides them),
  Gilded Step, Sunbrand, the buddy's sword combo, ability bar refresh on swap, Lightning Strike, Bond meter + Heaven's
  Judgment, boss bar; plus the hip holster and idle switch, quick-draw when the combo starts, animated sheathe, auto draw /
  sheathe, Stop Anim Montage reaching Body (the combo window), the ability bar list, key 2 and Angel's heal montage, no
  swapping during the duo and no effects left on Angel after it. (It skips the intro cutscene, which pins the demigods in
  place while it plays.)
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
