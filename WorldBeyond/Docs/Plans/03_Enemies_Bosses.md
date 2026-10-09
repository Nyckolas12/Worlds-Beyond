# Plan 3 — Enemies, mini-bosses and main bosses (done)

Part of the [roadmap](../Roadmap.md). Two halves, both built: **3A** (the enemy layer: a C++ enemy and brain,
telegraphed attacks, the regional roster and elite affixes; `migrate_pass10.py`, test `WorldsBeyond.Prototype.Enemies`)
and **3B** (the boss framework, two main bosses and two mini-bosses; `migrate_pass11.py`, test
`WorldsBeyond.Prototype.Bosses`). How to play with it and tune it: *Enemies* and *Bosses* in
[GAS_Prototype.md](../GAS_Prototype.md#enemies).

Your picks: split like Plan 1; the roster keeps the roadmap names on the **closest Paragon stand-in** with a tint per
region (real art is one data-asset edit away: *Looks* on the enemy's `DA_Enemy_*`); mini-bosses **Khaimera (Grux-pelt
skin)** and **Phase**; a script-built **test arena map** plus the **`Beyond.Spawn`** console command. The legacy
`BP_Enemy_*` Blueprints are untouched (the earlier tests use them in `MAP_Demo_Main`).

## Plan 3A — enemies

| Piece | Where | Notes |
|---|---|---|
| Roster entry | `Enemies/BeyondEnemyDefinition.h` (`UBeyondEnemyDefinition`, `UBeyondEnemyRoster`) | Id, name, region, rank, pack size; mesh, anim, locomotion blendspace, tint, scale, capsule; stats and growth; ability set; hit-react / death / spawn montages; AI ranges; allowed affixes; guaranteed loot. `DA_EnemyRoster` lists every enemy and affix |
| Enemy | `Enemies/BeyondEnemyCharacter.*` | Applies its definition in `PostInitializeComponents` before `Super` (abilities and stats are read once on possession). Elite multipliers and affix stats on top. Death: a death montage held on its last frame, ragdoll only without one. Summoned / split / cloned enemies drop nothing and are worth no EXP |
| Brain | `AI/BeyondEnemyController.*` | C++ think loop (0.25 s): idle / wander → combat → returning. Sight cone + close sense, being hit and pack alerts start fights; abilities picked by their AI hints through `BeyondAI::SelectAbility` (new filter overload); melee swings take **attack tokens** from the target (`AIAttackTokenCost` on every ability; `MeleeCombo` defaults to 1), the rest circle at *Strafe Radius*; casters keep *Keep Away Distance*; past *Leash Radius* they walk home with `State.Resetting` (no damage) and refill |
| Hit reactions | `AbilitySystem/Abilities/BeyondGA_HitReact.*` | Granted to every enemy that isn't *Uninterruptible*. Stagger / stun / knock-back interrupt; a light hit only flinches an enemy that isn't attacking, at most once per *Light Hit React Cooldown*; stun holds `State.Stunned` |
| Telegraphs | `AbilitySystem/BeyondAreaStrike.*` (`ABeyondAreaStrike`) | Circle, ring, cone and line markers (`M_Beyond_Telegraph` decal) that fill over the wind-up, then hit everything hostile inside; optional falling meteor / boulder and lingering damage (lava, poison). Its own actor: lands even when its ability ended, vanishes if its caster dies first |
| Enemy attack | `AbilitySystem/Abilities/BeyondGA_AreaAttack.*` | Locks the spot (at the target, on itself, or a cone / line from itself toward the target), turns, plays the montage, puts down one or more strikes (waves, scatter, growing rings) |
| Projectile | `AbilitySystem/BeyondProjectile.*` | Plain C++ projectile for `UBeyondGA_Projectile` (same `Speed` / `TargetLocation` / `EffectSpecHandle` names as `BP_Projectile_GABase`); trail and impact FX on Blueprint children |
| Animation | `Enemies/BeyondEnemyAnimInstance.*` | The Paragon Minions / jungle creatures ship no anim Blueprint: a C++ anim instance whose proxy builds its own node chain (locomotion blendspace or idle loop → `DefaultSlot`), like the engine's sequencer instance. Blendspace axes are found by name |
| Elite affixes | `Enemies/BeyondAffixDefinition.h`, `Enemies/BeyondAffixComponent.*` | Elites roll 1 affix (2 from level 10): tint and aura, stats, tags, granted abilities, on-hit DoT, lifesteal, damage-taken multipliers (with the Warded ward), lightning pulses, death actions |
| Hooks | `ABeyondCharacterBase` (`ModifyDamageTaken`, `CanDropLoot`, `GetHealthFloor`, `GetOutgoingDamageScale`), `UBeyondCombatSubsystem::ModifyIncomingDamage`, `UBeyondCombatLibrary::ApplyDamageOverTime` / `IsBoss`, `UBeyondPartyComponent` | Affixes and bosses change damage through these; the party skips loot for summons; tokens reset on revive |
| Spawning | `Enemies/BeyondEnemySubsystem.*`, `Enemies/BeyondEnemySpawner.*`, `Enemies/BeyondEnemySettings.*` | `SpawnEnemy` / `SpawnPack` (deferred spawn, ground / navmesh snap), live registry, pack alerts, wipe reset; `Beyond.Spawn`, `Beyond.KillEnemies`, `Beyond.ListEnemies`. Spawner actor: entries, radius, elite chance, activation radius, respawn rule. *Project Settings → Game → Worlds Beyond Enemies* |
| Plates | `UI/BeyondEnemyPlatesWidget.*` | Health, level and the elite's name over enemies in a fight or hit lately (not bosses: they have the boss bar) |
| Wipe | `UBeyondCombatSubsystem::OnPartyWiped` | Broadcast by the party: enemies teleport home refilled, summoned ones vanish, spawners refill, arenas reset |
| Editor helpers | `Tools/BeyondEditorLibrary.*` | For the scripts: montage slots, nav mesh bounds with a real box brush, building navigation inside a commandlet (lifts the editor's build locks) |

**Roster** (stand-ins; tune them in `/Game/WorldsBeyond/Enemies/Roster/`, abilities in `.../Abilities/<Enemy>/`)

| Region | Enemy (id) | Stand-in | Health / growth | Moves |
|---|---|---|---|---|
| Forest | Timber Wolf (`forest_wolf`), packs of 3 | Buff_Black ×0.75, grey | 90 / +11 | Bite combo, pounce (dash) |
| Forest | Raider (`forest_raider`), pairs | Lane Melee Dawn | 140 / +15 | Three-hit combo, 100° cleave |
| Forest | Raider Slinger (`forest_slinger`) | Lane Ranged Dawn | 100 / +11 | Shots (`BP_Beyond_SlingerShot`) from 1100, backs off inside 600, shove |
| Forest | Treant (`forest_treant`) | Buff_Red, bark green | 320 / +30 | Fists, 430 ground smash, uppercut cone |
| Corrupted woods | Hollow Wraith (`woods_wraith`), pairs | Buff_Blue, violet | 110 / +12 | Dark bolts (`BP_Beyond_WraithBolt`), soul burst under you |
| Corrupted woods | Cursed Knight (`woods_knight`), Elite | Lane Super Dusk | 380 / +34 | Combo, 120° cleave, 700-long overhead line |
| Corrupted woods | Corrupted Beast (`woods_beast`), pairs | Buff_Black ×1.1, toxic | 160 / +17 | Bites, poison spit that pools |

**Elite affixes** (`/Game/WorldsBeyond/Enemies/Affixes/`; elites get ×1.8 health, +30 Strength / Arcana, ×1.12 size)

| Affix | Effect | Counter |
|---|---|---|
| Molten | Hits burn (6 / s, 3 s); lava pool when it dies (15 / s, 5 s) | Don't stand in it |
| Venomous | Hits poison (6 / s, 4 s); gains a venom spit | — |
| Stormcharged | A telegraphed lightning ring every 8 s (25 damage) | Step away |
| Warded | Spells deal 40 % until a melee hit breaks the ward for 5 s | Ji-Woong opens it for Angel |
| Juggernaut | Can't be interrupted, ×1.5 health, +40 Defense, ×1.2 size | Angel kites it |
| Swift | ×1.45 speed and a lunge | — |
| Vampiric | Heals 30 % of the damage it deals | Burst it |
| Brood | Splits into two copies (35 % health, ×0.7 size) on death | Area damage |

## Plan 3B — bosses

| Piece | Where | Notes |
|---|---|---|
| Boss data | `Enemies/BeyondBossDefinition.h` | `UBeyondBossDefinition` (title, boss id, story boss, phases, bar radius), `FBeyondBossPhase` (health threshold, transition montage + time + FX, phase abilities, twists), `FBeyondBossTwist` (hazards, summon, enrage, shadow clones, darkness; repeats) |
| Boss | `Enemies/BeyondBossCharacter.*` | The next threshold is a **health floor** (a hit can't skip a phase); at a threshold: invulnerable, brain off, transition, `Boss.Phase.N`, the phase's abilities, its twists. Summons can shield the boss; enrage raises Strength / Arcana and speed and tints it; clones are weak copies (no phases, no bar, 30 % damage via `GetOutgoingDamageScale`). On death the boss's adds die and its hazards go; `Beyond.BossPhase <n>` |
| Hero Blueprints | Paragon `*PlayerCharacter` (reparented), `/Game/WorldsBeyond/Enemies/Bosses/BP_Boss_*` | The heroes' anim Blueprints cast to their own `*PlayerCharacter`, so those were reparented from Character to `ABeyondBossCharacter` and the bosses are children; boss montages sit on the heroes' **FullBody** slot. A definition mesh on a Blueprint that has one is a skin (anim Blueprint and placement kept) |
| New abilities | `BeyondGA_Leap`, `BeyondGA_Beam`, `BeyondGA_Summon`, `BeyondGA_Teleport`, `BeyondGA_Pull` | Leap: marked landing, ballistic arc, root motion ignored. Beam: lane marker, then a sweep capped by *Turn Rate*, optional drain. Summon: up to *Max Alive*. Teleport: away from / behind the target. Pull: line marker, then yank |
| Arena | `Game/BeyondBossArena.*` | Spawns the boss (unless a story boss is beaten), wakes it and seals the ring when a demigod comes within *Engage Radius* (or it's hit), gives hazards their centre, darkens for a Darkness twist, resets on a party wipe, unseals and records the defeat when it falls |
| Boss bar | `UI/BeyondBossBarWidget.*`, `ABeyondPlayerController` | Name and title, health with a trailing chip, notches at the later phases, phase pips, a shimmer while invulnerable, the twist's announcement; two bars stack. Ranked bosses (`UBeyondCombatLibrary::IsBoss`) use it; a Blueprint *Boss Bar Widget Class* (the legacy `BP_Enemy_Boss`) still gets its own widget. The duo move uses the same `IsBoss` |
| Save | `UBeyondSaveGame` v4 | `DefeatedBosses` (story bosses); `UBeyondPartyComponent::IsBossDefeated` / `MarkBossDefeated`; `Beyond.ResetBosses` |

| Boss | Phases | Moves | Twist | Signature drop |
|---|---|---|---|---|
| **Kael'thar, the Molten Colossus** (`boss_kaelthar`; Sevarog Bloodred ×2.5, Boss) | 100 / 70 / 40 % | Molten Cleave combo, Hammerfall 120° cone; *70 %:* Soul Siphon beam (drains), Comet Rain (6 meteors); *40 %:* Subjugate nova | At 40 % the arena's edge turns to lava (a ring 950–1600 out) and lava pools fall on the demigods every 12 s | Emberheart (sword) |
| **Hrimgar, the Frost Troll King** (`boss_hrimgar`; Rampage Elemental ×1.7, Boss) | 100 / 65 / 30 % | Glacier Fists, Rip 'n' Toss boulder, Ground Smash (two waves); *65 %:* Avalanche Charge, Frost Roar (stun cone) | 65 %: summons 3 wolves / raiders; 30 %: enrage (+40 Str / Arc, ×1.3 speed) and 2 wolves every 25 s (max 4) | Glacierheart Rod (staff) |
| **Gorehide, the Pale Alpha** (`boss_gorehide`; Khaimera Grux-pelt ×1.3, mini-boss, forest) | 100 / 50 % | Rending Claws, Pounce (leap); *50 %:* slam | 50 %: howls in 3 wolves, takes half damage while they live, and frenzies | Gruxfang (sword) |
| **Veyla, the Hollow Voice** (`boss_veyla`; Phase ×1.1, mini-boss, corrupted woods) | 100 / 60 / 25 % | Void Bolts, Blink when crowded, Severing Beam; *60 %:* Soul-Link Pull, Dark Nova | 60 %: 2 shadow clones; 25 %: the arena darkens and clones come back every 25 s | Hollow Voice Scepter (staff) |

Rank loot comes on top (bosses: 3 items with a set piece and a weapon; mini-bosses: 2 with a set piece). Main bosses
also give a Bond Point. The signature weapons (Epic – Legendary) never drop at random (drop weight 0).

## Test arena

`/Game/WorldsBeyond/Maps/TestArena` (built by the passes): a 120 m floor with a navmesh, both demigods and a checkpoint
at the start, a forest camp and a corrupted-woods camp (pass 10), and four boss arenas with a checkpoint at each
entrance (pass 11): Gorehide (level 5, south-west), Veyla (9, south-east), Kael'thar (15, north-west), Hrimgar (12,
north-east). The Enemies and Bosses tests run here.

Decisions made on the way:
- Roster entries are data assets on one C++ class (Python can fill every field; swapping art is one asset edit); only
  the bosses need Blueprints, because their anim Blueprints cast to their hero class.
- The enemy brain is a C++ think loop like the buddy's, not a behaviour tree (graph nodes can't be scripted).
- `BeyondGA_GroundStrike` stays the players' (it aims with a reticle); enemies use `BeyondGA_AreaAttack`.
- The Enemies test runs in the test arena: `MAP_Demo_Main` has ledges where a spawn could land on a lower level.
- All PIE tests now switch saving back on only after PIE has ended (before, the party's end-of-play save could
  write the save slot).

### Playtest fixes (pass 12)
- **Hit voice spam:** `BP_Angel` / `BP_Ji-Woong` play their hit cue (`PlaySound2D`) on every `OnDamageResponse`, and the
  cues had default concurrency (16 at once, no retrigger), so a wolf pack or a multi-hit attack stacked lines. Both
  cues now use `/Game/WorldsBeyond/Sounds/SC_Beyond_HitVoice`: 1 voice for the party, *Prevent New* (a line is never cut
  off), *Retrigger Time* 4 s. No graph edit; tune it in the asset. The Enemies test checks it. Note: new concurrency
  assets in 5.8 enable *MaxCount platform scaling*, which ignores *Max Count*; the pass turns it off.
- **Ji-Woong's LMB ran two combos:** `BP_Ji-Woong` had lost Left Mouse Button from *Legacy Keys To Disable* (pass 4 set
  it; it was already gone when pass 9 added F), so LMB fired the GAS sword combo *and* the old Blueprint combo with its
  voice line on every press. Pass 12 switches LMB (and F) off again.

### Test fixes (the suite was red before Plan 3)
- **Items** (failed only in some runs): `UBeyondEquipmentComponent` kept world times in floats. `GetTimeSeconds()` is a
  double, and ending Venom Infusion stored "now" in a float, which can round up, so the infusion still counted as running
  after a set piece came off. The times are doubles now and ending the infusion clears it outright.
- **Progression:** the level display took a new level on its next tick, and widgets don't tick under `-nullrhi`; it now
  re-reads the leader when the level-up event arrives.
- **SkillTree:** PIE ended while a legacy enemy was mid hit-reaction; `BP_Enemy_Base`'s montage callback then called its
  already-destroyed AI controller (a Blueprint runtime error). The test lets enemy montages finish (up to 3 s) first.
- **Powers:** expects LMB and F off on Ji-Woong (F since pass 9), and lists the bound keys when the check fails.

### Checked against the Unreal skills (unreal-blueprints, -cpp-gameplay, -behavior-trees, -enhanced-input, -niagara)

- **Blueprints:** communication is by multicast delegates (`OnCharacterKilled`, `OnPhaseChanged`, the combat
  subsystem's feeds), not cast chains; listeners unbind (`ABeyondEnemyController::OnUnPossess`, the arena and affix
  component on end play). Tick is only used where something changes every frame (the telegraph fill, the HUD widgets).
  Open point: a definition's *Character Class* is a soft reference loaded synchronously when the boss spawns; an
  arena could async-load it ahead if the spawn ever hitches.
- **C++:** every UObject the new classes keep is a `UPROPERTY` / `TObjectPtr` or a weak pointer (the anim proxy reports
  its blendspace through `AddReferencedObjects`); runtime components (seal walls) are registered; overrides call Super.
- **AI:** the enemy brain is a small C++ state machine (idle, combat, returning), as the skill suggests for AI that is
  a few clear modes; no behaviour tree was added next to the legacy ones. If designers want to author enemy logic
  visually later, the enabled **StateTree** plugin fits these modes better than a new behaviour tree. `MoveTo` failing
  off the navmesh (the skill's top pitfall) is exactly what MAP_Demo_Main's ledges did; failed moves now log at
  Verbose (`log LogBeyond Verbose`).
- **Enhanced Input:** Plan 3 adds no player input (console commands only), so there was nothing to bind.
- **Niagara:** the Paragon packs' effects are **Cascade**, which UE5 deprecates. Every looping effect Plan 3 spawns is
  stopped by its owner (auras, enrage, beam, lingering hazards, falling trails, the seal), and `FBeyondFX` takes
  either kind, so replacing a stand-in effect with a Niagara system is a data change. Prefer Niagara for new effects.

### Later
- Real art for the stand-ins (the *Paid packs* in the roadmap), and boss arenas in the open world (Plan 5).
- Check the stand-ins' capsule sizes and the telegraph colours in play; a cone or line pointing the wrong way is fixed
  by setting *ForwardSign* to -1 in `M_Beyond_Telegraph`.
- Region tints are fresnel overlays; proper material instances per region would look better.
