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
| `ABeyondCompanionController` | `AI/BeyondCompanionController.*` | Buddy AI: follow (with walk animation), defend the leader, use abilities by their AI hints, regroup |
| `ABeyondGameMode`, `ABeyondCheckpoint` | `Game/` | Party wipe → respawn at the last checkpoint (or player start) |
| `BTTask_BeyondActivateAbility` | `AI/` | Behavior tree task to run any GAS ability on an enemy |

## Controls

| Key | Angel | Ji-Woong |
|---|---|---|
| LMB | Magic spell (Blueprint) | Sword combo (Blueprint) |
| 1 / 2 | Equip staff / Heal (Blueprint) | Sheathe / draw the sword (animated; he spawns holding it) |
| **Q** | Blink (lightning dash) | **Gilded Step** — golden dash through enemies, they're hit a moment later |
| **E** | **Lightning Strike** — hold to aim, release to cast (RMB cancels) | **Sunbrand** — brand an enemy; your next sword hit detonates it |
| **R** | Heal | Heal |
| **G** | **Heaven's Judgment** (duo) when the Bond meter is full | same |
| Tab | Swap | Swap |

The Bond meter (bottom centre) fills as the demigods deal and take damage, faster when both hit the same enemy.
Heaven's Judgment needs a full meter and both demigods alive within 15 m of each other.

Ability slots are set per character in **Input → Ability Input Bindings**, abilities in **AbilitySystem → Ability Set**
(`/Game/WorldsBeyond/Abilities/DA_AbilitySet_*`). Once LMB attacks are GAS abilities, tick **Disable Legacy Key Input**.
Ability bar icons come from `/Game/WorldsBeyond/Blueprints/Widgets/Data/DT_AbilityMetaData` (row name = ability class, e.g.
`GA_JiWoong_Sunbrand_C`); the new abilities use placeholder icons for now.

## Blueprint edits left to do (in the editor)

C++ and scripts did everything else; these need node changes, which can't be scripted.

1. **Orange hair after Blink / Dash** — `GC_Blink` (`WorldsBeyond/Blueprints/Gameplay_Abilites/Cues`) and `GC_Dash`
   (`GameplayAbilitySystem/Cues`): replace both **Set Visibility** nodes with **Set Hidden In Game**
   (Target *CharacterRef → Mesh*, **New Hidden** ticked on *On Active*, unticked on *On Remove*, **Propagate to Children** ticked).
   *Why:* showing the mesh with Set Visibility + propagate also shows the groom's hidden hair-physics debug view, which draws red over the hair.
2. **Sword grip** — open the MetaHuman body skeleton, select `melee_equipped_socket` (on `hand_r`), add `SM_Sword` as a
   preview asset and rotate/offset the socket until the sword sits in Ji-Woong's hand.
3. *(Optional)* `AIC_Enemy_Boss`: delete the *DoOnce → add BossHealthbarWidgetRef to PlayerHUDRef* chain. The player
   controller shows the boss bar now (for either demigod, across swaps, following GAS damage).
4. *(Optional)* `BP_Ji-Woong`: delete the **F** key dash section (it activates Blink, which Gilded Step replaced on Q).
5. *(Optional)* `BP_BossFightCutscence`: only start the cutscene when the overlapping actor is the player-controlled pawn
   (the buddy walking in first triggers it today).
6. *(Optional)* Delete the *Character Follow* section (Event Unpossessed → Spawn Default Controller → Follow Player) in both
   demigods. The buddy controller ignores it, but it spawns a stray AI controller on every swap.

Done earlier: TakeDamage routed into GAS, attack tokens, enemy heal, death handler, test logic removed.

## Level checklist (`MAP_Demo_Main`)

- Place a **BeyondCheckpoint** before each encounter (the arrow is the respawn point).
- Press **P** to check the navmesh covers every arena and the boss platforms.
- Starter Content materials used by `SM_Axe` are missing: *Add → Add Feature or Content Pack → Starter Content*.

## Scripts and test

Run with the editor closed (build the C++ first):

```
UnrealEditor-Cmd.exe WorldBeyond.uproject -run=pythonscript -script=<repo>/WorldBeyond/Scripts/Migration/migrate_pass2.py -unattended -nosplash -NullRHI
UnrealEditor-Cmd.exe WorldBeyond.uproject -run=pythonscript -script=<repo>/WorldBeyond/Scripts/Migration/cleanup_legacy.py -unattended -nosplash -NullRHI
UnrealEditor-Cmd.exe WorldBeyond.uproject -ExecCmds="Automation RunTests WorldsBeyond.Prototype;Quit" -unattended -nullrhi -nosplash -TestExit="Automation Test Queue Empty"
```

- `migrate_prototype.py` — pass 1 (GAS migration), already applied.
- `migrate_pass2.py` — pass 2, already applied: Lightning Strike, Gilded Step, Sunbrand, sword equip, Heaven's Judgment,
  ability sets, `IA_Duo` on G, duo roles, boss bar class, `Montage_SwordCombo` hit-scan / combo-window notifies,
  ability bar rows.
- Both are idempotent and back up every asset they save to `Saved/MigrationBackups/<timestamp>/`; shared helpers live in
  `migration_common.py`. Pass 1 no longer overwrites ability sets that already exist.
- `cleanup_legacy.py` — dry run by default. To delete, in PowerShell run `$env:BEYOND_CLEANUP_APPLY = "1"` first
  (then `Remove-Item Env:BEYOND_CLEANUP_APPLY`). Never deletes anything still referenced; report in
  `Saved/MigrationBackups/cleanup_report.txt`. Afterwards, in the editor: right-click `Content` → **Fix Up Redirectors**.
  `GA_AOEAttack` and `GA_Dash` are unused now and can go in a later cleanup.
- `WorldsBeyond.Prototype.Smoke` — party + buddy, swaps without duplicate abilities, GAS damage mirrored into the old
  component, no friendly fire, attack tokens, death, revive.
- `WorldsBeyond.Prototype.Powers` — MetaHuman combat mesh, buddy walk fix, enemy perception, Ji-Woong's sword,
  Gilded Step, Sunbrand, the buddy's sword combo, ability bar refresh on swap, Lightning Strike, Bond meter + Heaven's
  Judgment, boss bar. (It skips the intro cutscene, which pins the demigods in place while it plays.)
- Also in the editor: *Tools → Test Automation*, filter WorldsBeyond.

In play: `showdebug abilitysystem` (PageUp/PageDown cycles actors), the Gameplay Debugger (`'`) for enemy
BT/perception/EQS, and `log LogBeyond Verbose` for dash / strike traces.

## Known limits

- Montage root motion doesn't move MetaHumans: character movement reads root motion from `CharacterMesh0`, which is
  empty; montages play on `Body`. Leaps and lunges animate in place (the dash moves the character itself).
- The ability bar lists every granted ability, including the buddy-only `GA_JiWoong_SwordCombo` / `GA_Angel_ArcaneBolt`
  (empty frames — they have no icon row).
