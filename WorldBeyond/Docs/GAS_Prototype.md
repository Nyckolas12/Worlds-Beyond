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
| `UBeyondBondMeterWidget` | `UI/BeyondBondMeterWidget.*` | Bond meter HUD built in C++ (blue → purple → gold); swap `Bond Widget Class` on `BP_PC` for a designed one |
| `ABeyondCompanionController` | `AI/BeyondCompanionController.*` | Buddy AI: follow (with walk animation), defend the leader, use abilities by their AI hints, regroup; stands still while a cutscene (level sequence) has it or its leader |
| `ABeyondGameMode`, `ABeyondCheckpoint` | `Game/` | Party wipe → respawn at the last checkpoint (or player start) |
| `BTTask_BeyondActivateAbility` | `AI/` | Behavior tree task to run any GAS ability on an enemy |

## Controls

| Key | Angel | Ji-Woong |
|---|---|---|
| LMB | Magic spell (Blueprint) | **Sword combo** (`GA_JiWoong_SwordCombo`) — press during each swing to chain the next |
| 1 | Draw / stow the staff (it rests on his back) | Draw / sheathe the sword (it rests on his left hip) |
| 2 | Heal (same as R) | Heal (same as R) |
| **Q** | Blink (lightning dash) | **Gilded Step** — golden dash through enemies, they're hit a moment later |
| **E** | **Lightning Strike** — hold to aim, release to cast (RMB cancels) | **Sunbrand** — brand an enemy; your next sword hit detonates it |
| **R** | Heal | Heal |
| **G** | **Heaven's Judgment** (duo) when the Bond meter is full — its own slot left of the meter | same |
| Tab | Swap | Swap |

The Bond meter (bottom centre) fills as the demigods deal and take damage, faster when both hit the same enemy.
Heaven's Judgment needs a full meter and both demigods alive within 15 m of each other; you can't Tab-swap while it plays.
Its icon is greyed out until the meter is full. To use your own picture, import the PNG and set it as **Icon** on
`/Game/WorldsBeyond/Abilities/Duo/GA_Duo_HeavensJudgment` (until then it shows the empty frame).

Ji-Woong walks around relaxed with the sword on his left hip. He draws it when an enemy comes within 10 m or the moment
his sword combo starts, and puts it away after 8 s without enemies within 15 m. Tune it on
`/Game/WorldsBeyond/Abilities/JiWoong/GA_JiWoong_EquipWeapon` (*Auto* section; 0 turns a behaviour off).

Angel does the same with his staff: it rests diagonally on his back, he draws it near enemies or the moment he casts
(LMB spell, Lightning Strike, Arcane Bolt, Heaven's Judgment) and stows it after a quiet spell; "1" still toggles it.
His idle switches between the relaxed `MM_Idle` and the staff stance `UE5_WZ_Idle_Seq`. Tune it on
`/Game/WorldsBeyond/Abilities/Angel/GA_Angel_EquipStaff`.

Ji-Woong's sword combo (`GA_JiWoong_SwordCombo`):
- **Window:** each combo window stays open 15 % longer than the montage's notifies say (*Combo Window Extension*).
- **Voice:** his attack line plays once when the combo starts, never again while you chain (*Combo Voice Line*).
- **Moving:** while he walks or runs the swings play on the upper body and his legs keep moving; standing still he
  keeps the full-body footwork, and starting / stopping mid-combo blends between the two (*Combo → Movement*).

Ability slots are set per character in **Input → Ability Input Bindings**, abilities in **AbilitySystem → Ability Set**
(`/Game/WorldsBeyond/Abilities/DA_AbilitySet_*`). Once LMB attacks are GAS abilities, tick **Disable Legacy Key Input**
(done for Ji-Woong: LMB → `IA_PrimaryAttack` → `Ability.Input.Primary`; Angel's LMB spell is still Blueprint).
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
UnrealEditor-Cmd.exe WorldBeyond.uproject -run=pythonscript -script=<repo>/WorldBeyond/Scripts/Migration/migrate_pass4.py -unattended -nosplash -NullRHI
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
  upper body while walking, Angel's staff (back, idle, drawn when casting),
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

- cloth further than 1.5 cm from the skin is pulled in, keeping 40 % of the extra looseness (max 5 cm per vertex), so the
  garment still drapes and its folds survive; cloth that clips into the body is pushed out to 0.6 cm;
- anything more than 10–15 cm away (the hood, drawstrings) and vertices skinned to the neck / head are left alone;
- the movement is smoothed across the mesh, every LOD is fitted, normals are recomputed.

It writes `<Outfit>_Fitted` next to each original (never touched) and puts it on `BP_Angel` / `BP_Ji-Woong`.
Look at both demigods up close in a T-pose and in motion. Too tight → raise `KEEP` or `SNUG`; still loose → lower `KEEP`;
a part that shouldn't move → raise `FREE_FROM` / `FREE_BEYOND` or add its bone to `PROTECT_BONES`. Re-running always
starts from the originals. To go back: `$env:BEYOND_FIT_REVERT = "1"`, run it again, `Remove-Item Env:BEYOND_FIT_REVERT`.

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
- Angel's LMB spell is still Blueprint and plays its voice line on every cast.
- The outfit fit is geometric: the cloth follows the body's skinning, it isn't simulated.
