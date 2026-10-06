# Plan 1 — Leveling, stats, skill tree and duo unlocks

Part of the [roadmap](../Roadmap.md). Two halves: **1A** (leveling in GAS) is built; **1B** (the skill tree on the
`SkillTreeSystem` pack, and duo powers behind it) is next.

## Plan 1A — Leveling and stats (done)

Built in C++ with GAS, configured by `migrate_pass7.py`, tested by `WorldsBeyond.Prototype.Progression`.
How to play with it and tune it: *Leveling* in [GAS_Prototype.md](../GAS_Prototype.md#leveling).

| Piece | Where | Notes |
|---|---|---|
| **Strength, Arcana, Defense, Level** | `UCharacterAttributeSet` | On every character, enemies too |
| Damage maths | `UBeyondCombatLibrary::ApplyDamage` (attacker), `UCharacterAttributeSet::HandleIncomingDamage` (target) | Melee × (1 + Strength / 100); projectile / explosion × (1 + Arcana / 100); taken × 100 / (100 + Defense) |
| **Experience, SkillPoints**, `IncomingExperience` (meta) | `Progression/BeyondProgressionAttributeSet` | Added at runtime to Player-team characters only. EXP over the threshold carries into the next level; several levels at once work |
| Rules | `UBeyondProgressionSettings` (Project Settings → Game) | Level L→L+1 needs 100 × L^1.5; max 50; 1 skill point per level; EXP per rank 25 / 60 / 300 / 1500, +10 % per enemy level |
| EXP in | `UBeyondGE_GrantExperience` (SetByCaller `SetByCaller.Experience`) | `ABeyondCharacterBase::GrantExperience`, `UBeyondPartyComponent::AwardExperience` |
| Stats from levels | `UBeyondGE_LevelStats` (infinite, SetByCaller per stat) | *Stat Growth* × (Level − 1), re-applied on each level-up |
| Who pays | `EBeyondEnemyRank` + *Experience Reward* on the character, kill feed `UBeyondCombatSubsystem::OnCharacterKilled` | The party pays both demigods for any kill one of them (or their projectile / brand / duo move) made |
| Level-up | `ABeyondCharacterBase::HandleLevelUp` | Refill, *Level Up FX*, `Event.Progression.LevelUp`, `OnLevelUp` (Blueprint), `OnCharacterLevelUp`, party `OnMemberLevelUp` |
| HUD | `UBeyondProgressWidget` via `ABeyondPlayerController::AttachToHUD` | Badge + EXP bar + skill points bottom left, "+EXP" pop-ups, LEVEL UP banner (one banner when both level together) |
| Save | `UBeyondSaveGame`, slot `BeyondProgress` | Level / EXP / skill points per demigod; on level-up, checkpoints and end of play; loaded when the party forms. `Beyond.ResetProgress`, `Beyond.GiveExperience`, `Beyond.SaveProgress` |

Decisions made on the way:
- EXP tuning sits in Project Settings rather than on `BP_PC`, so enemies, tests and the party all read one place.
- Enemies don't get the progression set (no EXP or skill points), only a Level; an enemy placed with *Starting Level* 5
  gets 4 × its *Stat Growth* and is worth +40 % EXP.
- A downed buddy still receives EXP but isn't refilled by a level-up (reviving does that).

## Plan 1B — Skill tree and duo powers (next)

The pushed `Content/SkillTreeSystem` pack is a Blueprint skill tree: `AC_SkillTreeSystem` (the component),
`WB_SkillTreeMain` / `WB_SkillTreeWindow` / `WB_SkillTrees` with three example trees (`WB_SkillTree01–03` on
`WB_SkillTreeBase`), slots (`WB_SkillTreeSlot`, `S_SkillTreeSlotDetails`, `S_SkillStages`), connection lines,
tooltips with required skills (`DT_RTRequiredSkillsTooltip`), a reset prompt and its own save (`SG_Skilltree`,
`S_SaveData`) with demo game mode / controller / game instance.

1. **Keep its UI, back it with GAS.**
   - `UBeyondSkillTreeAsset` (data asset), one per demigod plus a **Duo tree**. Nodes: id, cost, level requirement,
     prerequisites, ranks, and effects:
     - a stat gameplay effect (+MaxHealth, +Strength, stamina regen…);
     - grant an ability / raise an ability's level (stronger Lightning Strike, Sunbrand…);
     - unlock a **duo power**;
     - Bond modifiers (gain rate, max).
   - `UBeyondSkillTreeComponent` on each demigod: `CanUnlock`, `Unlock` (spends `SkillPoints` from 1A), `Respec`,
     applied effects tracked by handle. Saved in `UBeyondSaveGame` (the pack's own `SG_Skilltree` is replaced).
   - The pack's slot widgets read node state from the component through `BPI_STController`-style calls, so the
     look stays the pack's. The demo map, controller and game mode aren't used.
2. **Duo powers behind the tree.**
   - Heaven's Judgment stays the starter.
   - Duo nodes cost **Bond Points**: a shared party currency, 1 every 3 levels and 1 per main boss.
   - A **duo loadout** picks which unlocked duo power the G slot fires; the Bond meter shows its medallion.
   - First new duo powers (each a `UBeyondGA_DuoStrike`-style ability):
     - *Eclipse Brand*: Sunbrand marks every enemy nearby and Angel's lightning chains between the marks;
     - *Tempest Aegis*: a shared storm shield that reflects damage while it lasts.
3. **Trees** — node lists written with the pack open, roughly:
   - Angel: Arcana / health / stamina nodes, Arcane Spikes (more spikes, wider), Blink (charges), Heal (stronger, HoT);
   - Ji-Woong: Strength / health / Defense nodes, sword combo (extra hit, finisher), Gilded Step (charges, longer),
     Sunbrand (bigger detonation, drain);
   - Duo: Bond gain, Bond max, the duo powers.
4. **Script and tests** — `migrate_pass8.py` builds the tree data assets and wires `WB_SkillTreeMain` into the HUD
   (a key to open it, game paused); tests: unlock spends points and applies the effect, prerequisites and level gates
   hold, respec refunds, the duo loadout switches G, unlocks survive a save / load.
