# Plan 1 — Leveling, stats, skill tree and duo unlocks

Part of the [roadmap](../Roadmap.md). Two halves, both built: **1A** (leveling in GAS) and **1B** (the skill trees
in the `SkillTreeSystem` pack's style, and the duo powers behind them).

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

## Plan 1B — Skill tree and duo powers (done)

Built in C++ on GAS with the SkillTreeSystem pack's look, configured by `migrate_pass8.py`, tested by
`WorldsBeyond.Prototype.SkillTree`. How to use it: *Skill trees* in [GAS_Prototype.md](../GAS_Prototype.md#skill-trees).

**Why not the pack's own widgets:** its trees aren't data. Each is a widget (`WB_SkillTree01`) with ~40
`WB_SkillTreeSlot`s placed by hand in the UMG designer, each slot carrying its name, cost and links, and
`AC_SkillTreeSystem` gathers them with *Get All Widgets Of Class*. Keeping it would have meant laying out three trees
by hand and editing its `ApplySkill` Blueprint. Instead (your pick: *pack look, C++ brain*) the trees are data
assets, the rules are C++ / GAS, and the screen is drawn in C++ with the pack's art (`T_background`,
`T_WhiteIconBase` slot frames, `T_Lock`, `T_RadialGradient` glow, `T_Sword` / `T_MagicWand` / `T_PersonSimpleRun`
stat icons) and its hold-to-acquire interaction. The pack's Blueprints, demo map and save aren't used.

| Piece | Where | Notes |
|---|---|---|
| Tree data | `Progression/BeyondSkillTree.*` (`UBeyondSkillTreeAsset`) | Nodes: id, name, description, icon, grid position, ranks, cost per rank, level gate, prerequisites, effects. `ValidateTree()` catches duplicate ids, missing prerequisites and loops |
| Effects | `FBeyondSkillEffect` | **Stat** (+MaxHealth / MaxStamina / Strength / Arcana / Defense per rank, one infinite `UBeyondGE_SkillStats`), **Ability Rank** (+1 GAS level per rank on that ability's spec), **Grant Ability**, **Duo Power**, **Bond Gain**, **Bond Echo** |
| Ability ranks | `UBeyondGameplayAbility` *Upgrades* | Each level above 1: +15 % damage (`DamagePerLevel`) and −8 % cooldown (`CooldownReductionPerLevel`, floor 40 %). Covers `ApplyDamageToTarget` (combo, Gilded Step, Arcane Spikes), projectiles, Sunbrand detonations and the duo moves |
| Personal trees | `UBeyondSkillTreeComponent` on each demigod (from *Progression → Skill Tree*) | Spends the demigod's skill points; `Unlock`, `CanUnlock` (with the reason), `ResetTree` (full refund), `RestoreRanks` |
| Duo tree | `UBeyondDuoSkillTreeComponent` on the player controller (`BP_PC → Duo Skill Tree Asset`) | Spends the party's **Bond Points** (1 every 3 party levels, 1 per Boss-ranked kill); grants unlocked duo powers to both demigods, sets the **duo loadout** (the power on G; the others sit on `Ability.Input.Unbound`) |
| Screen | `UI/BeyondSkillTreeWidget.*` | K opens it (paused): Angel / Ji-Woong / Duo tabs, hover for details, hold LMB to unlock, RMB to put a duo power on G, R twice to reset, K / Esc to close |
| Save | `UBeyondSaveGame` v2 | Per demigod `SkillRanks`; party `BondPoints`, `DuoRanks`, `DuoLoadout`. Saved on every tree change |
| New duo powers | `GA_Duo_EclipseBrand`, `GA_Duo_TempestAegis` (copies of Heaven's Judgment with a variant on) | see [Powers_Design.md](../Powers_Design.md#duo-powers-from-the-duo-tree) |

Also fixed on the way: projectiles from `UBeyondGA_Projectile` (Angel's arcane bolt) didn't get Arcana, because the
projectile applies its own damage spec; they now do.

### The trees (as built; tune them in the editor)

**Angel — Stormcaller** (skill points)

| Node | Ranks × cost | Level | Needs | Per rank |
|---|---|---|---|---|
| Storm Affinity | 3 × 1 | 1 | — | +4 Arcana |
| Thunder-Tempered | 3 × 1 | 1 | — | +12 max health |
| Crystal Eruption | 3 × 1 | 3 | Storm Affinity | Arcane Spikes +1 rank |
| Arcane Volley | 2 × 1 | 4 | Storm Affinity | Arcane Bolt (companion casts) +1 rank |
| Second Wind | 2 × 1 | 4 | Thunder-Tempered | +10 max stamina |
| Static Ward | 3 × 1 | 6 | Thunder-Tempered | +4 Defense |
| Eye of the Storm | 2 × 2 | 9 | Crystal Eruption | +8 Arcana |
| Unbroken Spirit | 1 × 2 | 12 | Static Ward, Second Wind | +30 max health, +5 Defense |
| Overcharge | 2 × 2 | 14 | Eye of the Storm | Arcane Spikes +1 rank, +4 Arcana |
| Tempest Incarnate | 1 × 3 | 18 | Eye of the Storm, Static Ward | +12 Arcana, +20 max health |

**Ji-Woong — Radiant Hexblade** (skill points)

| Node | Ranks × cost | Level | Needs | Per rank |
|---|---|---|---|---|
| Gilded Edge | 3 × 1 | 1 | — | +4 Strength |
| Sunlit Vigor | 3 × 1 | 1 | — | +15 max health |
| Sun-Forged Combo | 3 × 1 | 3 | Gilded Edge | Sword combo +1 rank |
| Deeper Brand | 3 × 1 | 4 | Gilded Edge | Sunbrand +1 rank |
| Relentless | 2 × 1 | 4 | Sunlit Vigor | +10 max stamina |
| Radiant Stride | 2 × 1 | 6 | Sun-Forged Combo | Gilded Step +1 rank |
| Bulwark | 3 × 1 | 6 | Sunlit Vigor | +6 Defense |
| Hex Scholar | 2 × 1 | 7 | Deeper Brand | +5 Arcana (brand blasts and the duo shockwave) |
| Radiant Might | 2 × 2 | 10 | Sun-Forged Combo | +8 Strength |
| Unyielding | 1 × 2 | 12 | Bulwark, Relentless | +35 max health, +6 Defense |
| Radiant Judgment | 1 × 3 | 18 | Radiant Might, Hex Scholar | +12 Strength, +8 Defense, Sunbrand +1 rank |

**Duo — Bond of Heaven** (Bond Points)

| Node | Ranks × cost | Party level | Needs | Per rank |
|---|---|---|---|---|
| Kindred Spirits | 3 × 1 | 1 | — | Bond meter +15 % faster |
| Heaven's Wrath | 3 × 1 | 3 | Kindred Spirits | Heaven's Judgment +1 rank |
| Eclipse Brand | 1 × 2 | 6 | Kindred Spirits | new duo power |
| Tempest Aegis | 1 × 2 | 9 | Kindred Spirits | new duo power |
| Lingering Bond | 2 × 1 | 10 | Heaven's Wrath | meter keeps 15 % after a duo move |
| Total Eclipse | 2 × 1 | 12 | Eclipse Brand | Eclipse Brand +1 rank |
| Eye of the Tempest | 2 × 1 | 12 | Tempest Aegis | Tempest Aegis +1 rank |
| Soul-Bound | 1 × 2 | 15 | Lingering Bond | Bond meter +25 % faster |

### Later
- Gamepad navigation on the tree screen (it works with mouse + keyboard only for now).
- Heal and Blink are Blueprint abilities: their ranks would need the Blueprints to read the ability level, so the
  trees don't rank them yet.
- More duo powers and grant-ability nodes come with new abilities (Plan 3 bosses drop some).
