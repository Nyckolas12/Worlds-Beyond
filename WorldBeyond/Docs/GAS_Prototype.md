# Worlds Beyond — GAS prototype

GAS (Gameplay Ability System) now owns health, damage and death for both demigods and every enemy.
The old Blueprint combat (`BPC_DamageSystem`, `BPC_Attacks`, `BPI_Damagable`) still works on top of it.

## How it fits together

| Piece | Where | What it does |
|---|---|---|
| `UCharacterAttributeSet` | `Source/WorldBeyond/CharacterAttributeSet.*` | Health/Stamina, `IncomingDamage`/`IncomingHeal`; blocking, parrying, invincibility, death, no friendly fire |
| `ABeyondCharacterBase` | `Characters/BeyondCharacterBase.*` | Parent of `BP_Angel`, `BP_Ji-Woong`, `BP_Enemy_Base`. Grants abilities once, team, ability input slots, death/revive, attack tokens |
| Legacy bridge | `Characters/BeyondLegacyDamageBridge.*` | Mirrors GAS health into `BPC_DamageSystem` and fires its `OnDamageResponse` / `OnBlocked` / `OnDeath`, so existing hit-react, death and health-bar Blueprints keep working |
| `UBeyondCombatLibrary` | `AbilitySystem/BeyondCombatLibrary.*` | The one damage API: `ApplyDamage`, `ApplyDamageInfo` (takes `S_DamageInfo`), `HealActor`, `IsActorDead`, attack tokens… |
| `UBeyondGameplayAbility` | `AbilitySystem/BeyondGameplayAbility.*` | Base for all abilities: `InputTag`, AI hints (range, weight, heal threshold), `GetAimRotation` for player **and** AI |
| `UBeyondGA_MeleeCombo`, `UBeyondGA_Projectile` | `AbilitySystem/Abilities/` | Reusable melee combo and projectile abilities (configured as `GA_JiWoong_SwordCombo`, `GA_Angel_ArcaneBolt`) |
| `ABeyondWeapon` | `Weapons/BeyondWeapon.*` | Parent of both `BP_Weapon_Base`s; melee hit-scan (`HitScanStart` / `HitScanEnd`) |
| Party | `Player/BeyondPlayerController.*`, `Player/BeyondPartyComponent.*` | Tab swaps demigods, the other one is the AI buddy, auto-swap on death, stand next to a downed buddy for 3 s to revive, HUD follows the controlled demigod |
| `ABeyondCompanionController` | `AI/BeyondCompanionController.*` | Buddy AI: follow, defend the leader, use abilities by their AI hints, regroup |
| `ABeyondGameMode`, `ABeyondCheckpoint` | `Game/` | Party wipe → respawn at the last checkpoint (or player start) |
| `BTTask_BeyondActivateAbility` | `AI/` | Behavior tree task to run any GAS ability on an enemy |

## Controls

| Key | Angel | Ji-Woong |
|---|---|---|
| LMB | Magic spell (Blueprint) | Sword combo (Blueprint) |
| 1 / 2 | Equip staff / Heal (Blueprint) | Equip sword |
| **Q** | Blink | Blink |
| **E** | Lightning Strike — hold to aim, release to cast (RMB cancels) | Dash |
| **R** | Heal | Heal |
| Tab | Swap | Swap |

Ability slots are set per character in **Input → Ability Input Bindings**, abilities in **AbilitySystem → Ability Set**
(`/Game/WorldsBeyond/Abilities/DA_AbilitySet_*`). Once LMB attacks are GAS abilities, tick **Disable Legacy Key Input**.

## Blueprint edits left to do (in the editor)

C++ and scripts did everything else; these need a new node, which can't be scripted.

1. **Route `TakeDamage` into GAS** — `BP_Angel`, `BP_Ji-Woong`, `BP_Enemy_Base`
   *My Blueprint → Interfaces → TakeDamage*: replace **BPC Damage System → Take Damage** with **Apply Damage Info**
   (Beyond | Combat | Legacy). Wire *Damage Info → Damage Info*, *Damage Causer → Damage Causer*, *Return Value → Was Damaged*.
   On `BP_Enemy_Base` keep the *TryToBlock* branch in front of it.
   *Why:* enemy attacks and the demigods' Blueprint attacks still subtract health on the old component only.
2. **Attack tokens on the demigods** — `BP_Angel`, `BP_Ji-Woong`
   *Interfaces → ReserveAttackToken*: add **Reserve Attack Tokens** (Amount → Amount, Return Value → Success).
   Right-click → *Event Return Attack Token* → **Return Attack Tokens** (Amount → Amount).
   *Why:* these were empty, so `BTT_MeleeAttack` always failed — melee enemies never attacked you.
3. **Enemy heal** — `BP_Enemy_Base` *Interfaces → Heal*: replace **BPC Damage System → Heal** with **Heal Actor**.
4. **Death handler** — `BP_Angel`, `BP_Ji-Woong` *On Death (BPC_DamageSystem)*: delete **Disable Input**
   (it targets the player controller even when the AI buddy dies; C++ already stops movement and blocks abilities on death).
5. **Remove test logic** — `BP_Angel`: delete the *Testing AI* section (X key, J key → *AISwapStates*),
   then run the cleanup script (it still references the old `WorldsBeyond/Enemy` copy).
6. *(Optional)* Delete the *Character Follow* section (Event Unpossessed → Spawn Default Controller → Follow Player) in both demigods.
   The buddy controller ignores it, but it spawns a stray AI controller on every swap.
7. *(Optional)* Add `AN_HitScanStart/End` and `AN_ContinueComboStart/End` notifies to `Montage_SwordCombo`
   so the buddy's sword hits line up exactly (otherwise timing fallbacks are used).

## Level checklist (`MAP_Demo_Main`)

- Place a **BeyondCheckpoint** before each encounter (the arrow is the respawn point).
- Press **P** to check the navmesh covers every arena and the boss platforms.
- Starter Content materials used by `SM_Axe` are missing: *Add → Add Feature or Content Pack → Starter Content*.

## Scripts and test

Run with the editor closed:

```
UnrealEditor-Cmd.exe WorldBeyond.uproject -run=pythonscript -script=<repo>/WorldBeyond/Scripts/Migration/migrate_prototype.py -unattended -nosplash -NullRHI
UnrealEditor-Cmd.exe WorldBeyond.uproject -run=pythonscript -script=<repo>/WorldBeyond/Scripts/Migration/cleanup_legacy.py -unattended -nosplash -NullRHI
UnrealEditor-Cmd.exe WorldBeyond.uproject -ExecCmds="Automation RunTests WorldsBeyond.Prototype;Quit" -unattended -nullrhi -nosplash -TestExit="Automation Test Queue Empty"
```

- `migrate_prototype.py` — already applied. Idempotent; backs up every asset it saves to `Saved/MigrationBackups/<timestamp>/`.
- `cleanup_legacy.py` — dry run by default. To delete, in PowerShell run ` = "1"` first (then `Remove-Item Env:BEYOND_CLEANUP_APPLY`).
  Never deletes anything still referenced; report in `Saved/MigrationBackups/cleanup_report.txt`.
  Afterwards, in the editor: right-click the `Content` folder → **Fix Up Redirectors**.
- `WorldsBeyond.Prototype.Smoke` — plays `MAP_Demo_Main` headless: party + buddy, swaps without duplicate abilities,
  GAS damage on enemies mirrored into the old component, no friendly fire, attack tokens, death, revive.
  Also in the editor: *Tools → Test Automation*, filter WorldsBeyond.

In play: `showdebug abilitysystem` (PageUp/PageDown cycles actors), and the Gameplay Debugger (`'`) for enemy BT/perception/EQS.
