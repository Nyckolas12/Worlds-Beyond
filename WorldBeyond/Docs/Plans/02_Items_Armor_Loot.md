# Plan 2 — Items, armor sets and loot (done)

Part of the [roadmap](../Roadmap.md). Built in C++ on GAS, configured by `migrate_pass9.py`, tested by
`WorldsBeyond.Prototype.Items`. How to play with it and tune it: *Items and loot* in
[GAS_Prototype.md](../GAS_Prototype.md#items-and-loot).

Your picks: **stats + icons now** (no armor meshes fit the MetaHumans yet; every item keeps an unused *Visual Mesh*
slot for later), **four armor slots** (Helm, Chest, Gauntlets, Boots) plus a weapon, loot on the ground picked up
with **F**, and an **equipment & inventory screen on I** in the skill tree's style.

| Piece | Where | Notes |
|---|---|---|
| Item data | `Items/BeyondItemTypes.h` | `UBeyondItemDefinition` (name, description, optional icon, slot, weapon type, base stats, set, min / max tier, drop weight), `UBeyondArmorSet` (2-piece stats, full-set effect + its numbers and FX), `UBeyondItemDatabase` (the loot pool), `FBeyondItemInstance` (id, definition, tier, item level, bonus stats; saved) |
| Item maths | `Items/BeyondItemLibrary.*` | Final stat = base × tier (1 / 1.2 / 1.45 / 1.75 / 2.1) × (1 + 8 % per item level above 1), rounded, + bonus stats (Epic 1, Legendary 2); who can use what; loot rolls |
| Rules | `UBeyondLootSettings` (Project Settings → Game → Worlds Beyond Loot) | Item database, tier scales, per-rank loot rules, drop glow per tier, pickup range (2.5 m), starter kit |
| The bag | `UBeyondInventoryComponent` on the player controller | Shared by the party, 60 items; `OnItemAdded` feeds the HUD toasts |
| Equipment | `UBeyondEquipmentComponent`, created on each demigod at runtime | Helm / Chest / Gauntlets / Boots / Weapon; item stats and 2-piece bonuses as one infinite `UBeyondGE_EquipmentStats`; swapping returns the old item to the bag; weapons only fit their own demigod (`Weapon.Melee.Sword` Ji-Woong, `Weapon.Ranged.Staff` Angel) |
| Shared stat code | `Progression/BeyondStatEffects.*` | The skill tree and equipment both put their bonuses on through `BeyondStats::ApplyStatBonuses` (new effect on before the old comes off, so health never dips) |
| Set effects | `UBeyondEquipmentComponent`, `UBeyondCombatSubsystem::OnHitLanded`, `UBeyondGE_DamageOverTime` | Every landed hit is reported with its damage tags; set damage carries `DamageType.Proc.*` and never sets off another effect. Poison / burn are a periodic effect (1 s ticks) that a new hit refreshes. Set damage grows +10 % per wearer level above 1 |
| Loot | `UBeyondPartyComponent::DropLoot` (on every party kill), `Items/BeyondLootDrop.*` | Rolls the rank's rule plus the enemy's own *Guaranteed Loot*; drops lie in a ring around the body, glowing in the tier's colour (Paragon Minions ground-pickup FX) with a pulsing light |
| Pickup | `ABeyondPlayerController` (`InteractAction`, F) | The nearest drop within range of the leader goes into the bag; a full bag says so |
| HUD | `UBeyondProgressWidget` | "F  Venomweave Helm (Rare)" prompt in the tier colour, pickup toasts on the right (4 s), notices |
| Screen | `UI/BeyondInventoryWidget.*` (I) | Paper doll, stats with the equipment's share, worn sets (2 / 4), the bag sorted by slot then tier, tooltips comparing with what's worn, upgrade arrows / can't-use crosses |
| Menus | `ABeyondPlayerController::OpenMenuWidget` / `CloseMenuWidget` | The skill tree and the inventory share pause / input handling; only one is open at a time (K and I switch between them) |
| Save | `UBeyondSaveGame` v3 | The bag, each demigod's equipped items, *starter kit given*. Saved (once per frame at most) on pickups, equipping, discarding |

## Armor sets

| Set | Colour | 2 pieces | Full set (4 pieces) |
|---|---|---|---|
| **Venomweave** | green | +5 Strength, +5 Arcana | **Venom Infusion:** using Q / E / R coats weapon and spells in venom for 6 s (12 s cooldown, green aura `P_CarriedBuff_GreenBuff`); infused hits poison for 4 s at 8 damage a second (Sevarog's green flames on the target) |
| **Stormforged** | blue | +8 Arcana | **Chain Lightning:** hits have a 25 % chance to arc to up to 3 enemies within 6 m for 20 damage each (0.5 s cooldown, FX pack lightning) |
| **Sunforged** | gold | +10 Defense | **Sunfire:** hits set enemies ablaze for 3 s at 6 damage a second (Serath's burning FX); above half health the wearer takes 15 % less damage |

Set pieces drop at **Rare or better**. All numbers and FX live on the set assets in `/Game/WorldsBeyond/Items/Sets/`.

## Items (as built; tune them in the editor)

Base stats at Common, item level 1. Armor in `/Game/WorldsBeyond/Items/Armor/`, weapons in `.../Weapons/`.

| Line | Helm | Chest | Gauntlets | Boots | Tiers |
|---|---|---|---|---|---|
| Wanderer's (balanced) | +10 health, +2 Def | +20 health, +3 Def | +2 Str, +2 Arc | +10 stamina, +1 Def | Common – Rare |
| Ironbound (Defense / health) | +4 Def, +12 health | +7 Def, +25 health | +3 Def, +2 Str | +3 Def, +10 health | Common – Epic |
| Acolyte's (Arcana / stamina) | +4 Arc, +6 stamina | +5 Arc, +12 stamina, +10 health | +4 Arc | +12 stamina, +2 Arc | Common – Epic |
| Venomweave | +2 Str, +2 Arc, +8 health | +3 Str, +3 Arc, +18 health | +4 Str, +2 Arc | +3 Arc, +10 stamina | Rare – Legendary |
| Stormforged | +4 Arc, +8 health | +5 Arc, +15 health, +2 Def | +5 Arc | +3 Arc, +12 stamina | Rare – Legendary |
| Sunforged | +4 Def, +12 health | +6 Def, +25 health | +4 Str, +2 Def | +3 Def, +8 stamina | Rare – Legendary |

| Weapon | For | Stats | Tiers |
|---|---|---|---|
| Traveler's Blade | Ji-Woong | +4 Str | Common – Rare (starter) |
| Gilded Saber | Ji-Woong | +6 Str, +2 Def | Uncommon – Epic |
| Dawnbreaker | Ji-Woong | +9 Str, +15 health | Rare – Legendary |
| Ashwood Staff | Angel | +4 Arc | Common – Rare (starter) |
| Stormcaller's Rod | Angel | +6 Arc, +8 stamina | Uncommon – Epic |
| Eclipse Scepter | Angel | +9 Arc, +15 health | Rare – Legendary |

**Starter kit** (a fresh game or a save from before items): each demigod gets their Common weapon plus a Wanderer's
Chest and Boots, worn.

## Loot table

| Enemy rank | Drop chance | Items | Tier weights C / U / R / E / L | Guaranteed |
|---|---|---|---|---|
| Regular | 12 % | 1 | 60 / 30 / 9 / 1 / 0 | — |
| Elite | 35 % | 1 | 30 / 40 / 24 / 5 / 1 | — |
| Mini-boss | 100 % | 2 | 0 / 30 / 45 / 20 / 5 | a set piece |
| Boss | 100 % | 3 | 0 / 0 / 45 / 40 / 15 | a set piece **and** a weapon |

Item level = the enemy's level. An item's tier is clamped to its own min / max (a Legendary roll on a Wanderer's
piece comes out Rare). Each character also has **Loot → Guaranteed Loot**: items it always drops on top of its rank's
roll, for the hand-made boss drops of Plan 3.

Decisions made on the way:
- One bag for the party rather than one per demigod: loot is picked up by whoever leads, and the screen equips it on
  either demigod.
- Items show drawn slot glyphs in the tier colour; there was no armor icon art in the project. Set an item's **Icon**
  and the screens use it instead.
- Ji-Woong's old Blueprint F event (it fired Blink, which Gilded Step on Q replaced) is switched off by pass 9 so F
  only picks loot up.
