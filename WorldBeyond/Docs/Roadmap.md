# Worlds Beyond — roadmap

The combat prototype is done: GAS abilities for both demigods, the AI buddy, the duo super move and its Bond meter,
Angel's aiming and spikes (see [GAS_Prototype.md](GAS_Prototype.md)). The next layer is the RPG around it. Each system
gets its own plan in [Plans/](Plans/), built one at a time in this order:

| # | Plan | Status | Why this order |
|---|---|---|---|
| **1A** | [Leveling and stats in GAS](Plans/01_Leveling_SkillTree.md#plan-1a--leveling-and-stats-done) | **Built** (pass 7) | Armor, enemies and loot all build on stats / EXP |
| **1B** | [Skill tree + duo powers](Plans/01_Leveling_SkillTree.md#plan-1b--skill-tree-and-duo-powers-done) | **Built** (pass 8) | Uses the skill points from 1A and the pushed `SkillTreeSystem` |
| **2** | [Items, inventory, **armor tiers + set effects**, loot drops](Plans/02_Items_Armor_Loot.md) | **Built** (pass 9) | Uses Plan 1 stats; bosses (3) need loot |
| **3** | [Enemies, mini-bosses, main bosses](Plans/03_Enemies_Bosses.md) (phases, mechanics, twists, loot) | **Built** (passes 10–11) | Uses EXP (1) and loot (2) |
| **4** | [Dialogue: NPCs, village chatter, demigod banter](Plans/04_Dialogue.md) | **Built** (pass 13) | Villages (5) get populated with it |
| **5** | [Open world](Plans/05_Open_World.md): regions, villages, POIs, boss arenas, map / fast travel | **5A + 5B built** (passes 14–16: systems, greybox world); villages, nature, polish next | Biggest and content-heavy; uses all of the above |

## Free art to collect (Fab)

*Infinity Blade: Adversaries / Grass Lands / Ice Lands / Fire Lands are no longer on Fab*, so they are out. These are
the replacements. Fab itself couldn't be opened from the build machine, so check each listing still says **Free**
before claiming; add them to the project from the launcher's Fab tab or the editor's Fab window.

**Enemies and bosses (Plan 3)** — Epic's Paragon characters are free, realistic, fully animated and come with
their own ability FX, which fits the MetaHuman demigods:

| Pack | Use it for |
|---|---|
| [Paragon: Rampage](https://www.fab.com/listings/0807cf74-08fd-4a33-8c8d-f33c9439fb1f) | The **rock troll** boss (reference image 3): a huge stone brute that rips up boulders. Re-tint his skin icy blue-grey for the snowy forest |
| [Paragon: Sevarog](https://www.fab.com/listings/a4882b5e-cfad-4830-a3dd-46a6c31a79b2) | The **fiery demon** colossus (reference image 2): scale him up 2–3×, add fire materials / Niagara; his hammer and soul-siphon moves become boss mechanics |
| [Paragon: Grux](https://www.fab.com/listings/8c4bac2c-f7f7-4632-a644-47f4e104f5d8) | Ogre / brute **mini-boss** (charges, double-axe slams) and an elite variant for the corrupted woods |
| Paragon: Minions (Dawn / Dusk minions, melee + caster + siege) | The **regular roaming enemies**: tint them dark and corrupted for the haunted woods |
| [Paragon: Serath](https://www.fab.com/listings/522b6160-15ab-492b-a2b0-c09f9bb5f6e6), [Paragon: Phase](https://www.fab.com/listings/b2c95d5c-a805-460b-a01b-db6da3a778f0) | Fallen-angel and caster **mini-boss** options |

Also worth a look: Fab's **Free** filter for 3D models (`fab.com/category/3d-model?is_free=1`, search *creature*,
*werewolf*, *troll*, *golem*) and Fab's **limited-time free** picks each month. Anything claimed stays yours.
Paid packs that match the references if a budget appears later: *Monster Pack* (troll, mountain troll, cyclops,
werewolf on the UE5 skeleton), *Monster Boss*, *Modular Orcs & Troll – MetaHuman*, *Demon Fantasy Creature Variations*.

**World (Plan 5):**

| Pack | Use it for |
|---|---|
| Quixel **Megaplants** (free on Fab, work with UE 5.8's Procedural Vegetation Editor) | Trees and plants for the start forest; make dead / twisted variants for the corrupted woods |
| **Electric Dreams** environment sample (free, Unreal-only licence) | Megascans rocks, cliffs, ground cover and a PCG setup to scatter them: lakes, caves and cliff edges |
| **Medieval Village Megascans Sample** | Houses and props for the starting village and the small villages; boarded-up copies for the empty villages |
| **Dark Ruins** Fab sample scene (Megascans) | The corrupted woods' ruins and shrines |
| Megascans you claimed while they were free (2024) | Still yours under the Fab licence — check *My Library* first |
| Megascans *Nordic forest* assets | The snowy forest around the troll boss |

## Plans 2–5 at a glance

Each gets a full write-up in `Plans/` when it starts.

- **2 · Armor and loot (built):** see [Plans/02_Items_Armor_Loot.md](Plans/02_Items_Armor_Loot.md): tiers Common →
  Legendary, a shared bag, four armor slots + weapon, **Venomweave** / **Stormforged** / **Sunforged** sets, loot per
  rank (bosses always drop a set piece and a weapon), F to pick up, the I screen, saved.
- **3 · Enemies and bosses (built):** see [Plans/03_Enemies_Bosses.md](Plans/03_Enemies_Bosses.md): the regional roster on
  Paragon stand-ins, 8 elite affixes, telegraphed attacks and a C++ enemy brain; bosses with health-floor phases, twists
  and arenas: Kael'thar, the Molten Colossus, Hrimgar, the Frost Troll King, mini-bosses Gorehide and Veyla. The
  original outline: each boss can list its own drops under *Loot → Guaranteed Loot* (Plan 2). A region roster (forest: wolves, raiders, treants; corrupted woods: hollow wraiths,
  cursed knights, corrupted beasts) with elite affixes; a C++ boss framework with health-threshold phases, mechanics, a
  **twist** per boss (arena change, enrage, summons), the existing boss bar and loot; mini-bosses use it scaled down.
  First main bosses: the molten demon colossus and the frost troll king.
- **4 · Dialogue (built):** see [Plans/04_Dialogue.md](Plans/04_Dialogue.md): on the Advanced Dialogue System pack (its
  Blueprints untouched, driven from C++), `BP_NPC_Base` villagers you talk to with F, greetings and multi-speaker
  over-head chatter, Angel and Ji-Woong's **bottom-subtitle banter** (regions, low health, boss victories, idle,
  level-ups, revives), saved **story flags** set by rows and checked by conversations, starter lines for Mossbrook.
- **5 · Open world (5A + 5B built):** see [Plans/05_Open_World.md](Plans/05_Open_World.md): regions with banners,
  discovery, weather, music and level bands, waystones (attune, rest, fast travel from the M map), places, hazards,
  streaming-safe camps and arenas, save v6; the greybox world `Dominion` (a 4 km World Partition map, its terrain built
  by script, 22 waystones, 30 camps, 10 arenas, the painted map). Next: villages, nature, polish. The original
  outline, FF7 Rebirth style: World Partition map; region volumes (name banner, music, weather, level band,
  discovery); the start **forest** with lakes, caves and the **starting village** in its middle; the **dark-magic
  corrupted woods** with empty villages and roaming monsters; later regions from *The Land of the Wandering Dominion*
  map. Every region: 2–3 small villages and a main village, mini-boss arenas and a story-boss arena; waystones (fast
  travel), a world map from the map image, enemy camps and spawners.
