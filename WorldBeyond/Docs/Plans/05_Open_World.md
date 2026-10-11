# Plan 5 — Open world: regions, villages, places, boss arenas, map and fast travel (in progress)

Part of the [roadmap](../Roadmap.md). In parts: **5A** (the world systems; built in pass 14, test
`WorldsBeyond.Prototype.World`), **5B** (the world itself: the greybox World Partition map `Dominion`, its terrain
built by script, everything placed; built in passes 15–16, tests `WorldsBeyond.Prototype.WorldBuilder` /
`.OpenWorld`), then villages, nature and polish. How to play with it: *Open world* in
[GAS_Prototype.md](../GAS_Prototype.md#open-world).

Your picks: **all four regions dressed** (the Elderwood with Mossbrook, the Blightwood east, the Rimewood Reach
north-east, the Cinderlands north-west), the **script builds a blockout landscape you refine**, about **4 × 4 km**,
**fast travel from the map (M) to any attuned waystone** outside fights, **the script assembles houses** from the modular
pieces on hand, **the four existing bosses get arenas** and the other arena sites are built empty for later bosses, and
the **world map is painted from the terrain**. Placeholder names and lines are fine (all editable data).

## Plan 5A — the world systems (built)

| Piece | Where | Notes |
|---|---|---|
| Region data | `World/BeyondRegionDefinition.*` | `UBeyondRegionDefinition` (`DA_Region_<id>` in `/Game/WorldsBeyond/World/Regions/`): id (= banter context unless *Banter Context* says otherwise), name, the small line over it, kind (region / village / area), parent, region tag, **level band**, discovery EXP, music, ambience, `FBeyondWeatherPreset` (sun, sky light, fog, saturation, tint, exposure, precipitation), map colour. A village keeps its region's weather and music unless it has its own |
| Region volume | `World/BeyondRegionVolume.*` | A closed spline drawn on the ground (X / Y only), or a circle of *Radius*; *Priority* decides overlaps (villages 10, areas 5, regions 0) |
| Hub | `World/BeyondWorldSubsystem.*` | Four times a second (*Scan Interval*): the region and the village / area under the leader, places and waystones within their discovery radius, a sealed boss arena, the last safe ground. A new region / village: discovered the first time (saved, EXP for both, *Discovered* on the banner), its banner (not again within 30 s), its banter (`RegionEntered` with its context; the village's first), weather (blended over 6 s), music (crossfaded over 3 s) and ambience. Also the level band anywhere (`GetLevelBandAt`), waystone use, fast travel and the streaming hold below, the map's markers, navigation invokers |
| Streaming hold | same | Every big move of the party goes through it (fast travel, a respawn, resuming a save, the first moments in an open-world map, deep water): fade out, move both demigods (`UBeyondPartyComponent::TeleportPartyTo`), hold them until World Partition has streamed the destination in, there is ground under each of them and (up to 3 s) navmesh, snap them onto the ground, fade in. 20 s without ground: the player start; non-partitioned maps don't wait |
| World actor | `World/BeyondWorldInfo.*`, `World/BeyondWorldMapData.h` | One per open-world map: turns the start hold, weather and music on; the map's picture (`DA_WorldMap`: texture, the world rectangle it covers, a fog mask per region), the lights the weather drives (found in the level when empty), the region grading (an unbound post-process), two music players and an ambience player (they keep playing in menus), precipitation that follows the camera |
| Waystone | `World/BeyondWaystone.*` | Seen within 80 m (on the map). **F** attunes it (travel target, respawn point, the last waystone, saved, it lights up); F again **rests**: everyone healed (the downed get up), respawn point, save, camps with the *On Rest* rule fill up. The party arrives at its *Arrival Point*. *Start Attuned* for the starting village |
| Place | `World/BeyondPointOfInterest.*` | Cave, shrine, ruins, vista, lake, camp, landmark or arena site: discovered within its radius (a toast, EXP, saved), then on the map; *Visible Before Discovery* shows a "?" |
| Hazard | `World/BeyondHazardVolume.*` | A box: **lava** burns once a second inside and lingers 2 s after; **deep water** (there's no swimming) sends the party back to the last safe ground, 10 % hurt |
| Navigation | `World/BeyondOpenWorldNavigationSystem.*` | The open world builds its navmesh only around the two demigods, at runtime (set as the map's *Navigation System Class*; its RecastNavMesh *Dynamic*), so re-sculpting never needs a navigation rebuild. Other maps keep their built navmesh |
| Settings | `World/BeyondOpenWorldSettings.*` | *Project Settings → Game → Worlds Beyond World*: scan interval, discovery EXP (region 120, village 40, place 30), banner time / repeat, fast-travel combat radius, fades, streaming timeout, navmesh wait, invoker radii, weather / music blend |
| Banner | `UI/BeyondRegionBannerWidget.*` | The place's name high in the middle (the line over it, thin gold rules, the level band, "Discovered · +120 EXP"), toasts on the left ("Waystone attuned", "Discovered: Hollowroot Cave", "Rested"). Faded from timestamps, so it works without ticking |
| World map | `UI/BeyondWorldMapWidget.*` | **M** (or the gamepad's View): the map's picture (plain parchment fitted to everything when the map has none), fog over regions not found yet, region names and level bands, villages, places, boss arenas (crossed swords; a tick once beaten), waystones (bright once attuned), the party. Wheel / Q E zoom, drag pans, arrows / WASD / the d-pad hop between markers; F / Enter / a click on an attuned waystone asks, again travels. Paused; I and K switch screens, M / Esc close |
| Party and save | `UBeyondPartyComponent`, `UBeyondSaveGame` v6 | Discovered regions, discovered places (and waystones), attuned waystones, the last waystone; `HasStoryFlag` answers `Discovered.<id>` and `Waystone.<id>`. `TeleportPartyTo(Transform, bHeal)`, `HealParty`; `RespawnPartyAt` is the healing teleport. *Party Classes* on `BP_PC` are now set (Angel, Ji-Woong): a demigod not placed in the map is spawned beside the leader |
| Controller | `ABeyondPlayerController` | `WorldMapAction` (M), the map and banner widgets; F: talk, else a waystone ("[F] Attune / Rest - Mossbrook Waystone"), else loot; `IsAnyMenuOpen` (talking, the prompt and banter treat the map like the other menus) |
| Spawners | `ABeyondEnemySpawner` | Entries with **Level 0** take the region's band (+ *Level Offset*, kept inside the band). With a *Deactivation Radius* the camp's enemies not in a fight are put away once the party is that far and come back when it returns (the fallen per the respawn rule); new rule **On Rest** (back when the party rests or wipes). Enemies only spawn once there is ground under them. Always loaded |
| Arenas | `ABeyondBossArena` | *Boss Spawn Radius*: the boss only exists while the party is that close (and there is ground); past *Boss Despawn Radius* an unfought boss goes away. *Remember Defeat*: a mini-boss stays beaten once its `Boss.<id>` flag is set. *Arena Id*, `IsDefeated`; an arena without a boss is a marked site. Registered with the hub (map markers, boss music while sealed: the definition's *Music*, which was unused) |
| Respawn | `ABeyondGameMode` | In an open-world map the respawn waits for the ground at the respawn point (the streaming hold) |
| Tags | `BeyondGameplayTags` | `Region.Frost`, `Region.Molten` |
| Demo | `World/BeyondWorldDemo.*` | Builds these actors at runtime (the World test and `Beyond.WorldDemo`) |
| Cheats | console | `Beyond.Travel <waystone>` (attunes it too), `Beyond.Discover [all / id]`, `Beyond.ResetWorld`, `Beyond.Region`, `Beyond.Map`, `Beyond.Weather <region>`, `Beyond.WorldDemo` |

**Regions** (in `world_content.py`; `DA_Region_<id>`; centre in metres east / north of the world's centre)

| Region | Id | Band | Look / sound | Villages (band) |
|---|---|---|---|---|
| **The Elderwood** (start forest, centre / south-west) | `region_forest` | 1–6 | warm sun, light mist; forest theme, crickets | **Mossbrook** (1–3; banter `region_village`), Fernhollow (2–4), Brackenford (1–3), Willowmere (4–6) |
| **The Blightwood** (corrupted, east) | `region_corrupted_woods` | 6–10 | dim violet fog, washed-out colours; dissonant pads, wind | **Duskwatch** (6–8, an outpost), Greyhallow, Marrowfield, Sallow End (abandoned) |
| **The Rimewood Reach** (frost, north-east) | `region_frost` | 10–13 | pale cold light, blue fog; atonal sustain, wind | **Frostholm** (10–11), Pinecrest, Hearthfall |
| **The Cinderlands** (molten, north-west) | `region_molten` | 13–16 | red haze, embers falling; dissonant percussion, fire | **Cinderhold** (13–14), Slagford, Emberrest |
| Kingsfork Crossroads (area, north of the gate) | `area_kingsfork` | 9–11 | — | — |

Music and ambience are the village sample's loops (their *Looping* switched on) until the real tracks arrive.

**New enemies** (copies of the Plan 3 roster, in `/Game/WorldsBeyond/Enemies/Roster/`, ×1.3–1.35 health, tinted)

| Region | Enemies |
|---|---|
| Rimewood Reach | Frost Wolf (`frost_wolf`), Frost Reaver (`frost_reaver`), Rime Slinger (`frost_slinger`), Rime Treant (`frost_treant`) |
| Cinderlands | Cinder Wraith (`cinder_wraith`), Ashen Knight (`ashen_knight`, elite), Magma Beast (`magma_beast`), Cinder Slinger (`cinder_slinger`) |

Kael'thar is now `Region.Molten` and Hrimgar `Region.Frost` (pass 11 used the Plan 3 roster regions; fixed there too).

## Plan 5B — the greybox world (built)

`/Game/WorldsBeyond/Maps/Dominion`, a World Partition map, about 4 × 4 km. Open it in the editor to look around and
sculpt; play it from the editor (the main menu still opens `MAP_Demo_Main`, see *Left for you*).

| Piece | Where | Notes |
|---|---|---|
| Editor module | `Source/WorldBeyondEditor` | Editor-only tools for the scripts (Landscape, material editing, image writing); listed in the `.uproject` and the editor target |
| Layout | `World/BeyondWorldLayout.*` (`DA_WorldLayout` in `/Game/WorldsBeyond/World/`) | The world's shape in metres east / north: regions (terrain style, polygon, base / hill height and width, map colour), ridges and their passes, peaks, the volcano, lakes, roads, flat pads; *Border Warp* lets borders and ridges wander. Seeded from `world_content.py` |
| Terrain | `World/BeyondHeightfieldGenerator.*` | Pure C++, the same for a seed: regions blended into each other (rolling forest, low blight, ridged frost and fire), ridges with passes, peaks, the world's mountain border, the volcano (cone, crater, a breach to the south-east, a lava floor), lake bowls with muddy shores, roads pressed to a smoothed profile at most 12 % steep, flat pads; then 11 ground layers by region, slope (rock), height (snow), shore (mud), road (path / cobble) and pad. 4033 × 4033 in about 5 s |
| Builder | `World/BeyondWorldBuilderLibrary.*` | The landscape (import into an edit layer called *Generated*, an empty *Sculpt* layer above it for your sculpting, split into 256 streaming proxies), the greybox material (a flat colour per layer and soft blotches), the layer infos, ground heights for placing things, the painted world map, the open world's navigation (`BeyondOpenWorldNavigationSystem`, a dynamic RecastNavMesh), saving a partitioned map |
| Map art | same | `T_WorldMap` (2048 px): ground colours by layer, a region tint, hillshade from the north-west, 25 m contours, water and ice, lava, roads, forest stipple, paper grain; a fog mask per region; `DA_WorldMap` with the world rectangle (`/Game/WorldsBeyond/World/Map/`) |
| Placed | `migrate_pass16.py` | The world info, 19 region volumes (4 regions with winding borders that follow the ridges, 14 villages, Kingsfork), 22 waystones (Mossbrook's attuned), 17 places, 10 arenas (Gorehide's Den, the Weeping Grove, the Frozen Crown, the Molten Heart; six empty sites) with checkpoints at the bosses' entrances, 30 camps on the region rosters (Level 0: the region's band; they come out within 55 m and go away past 150 m; fallen enemies return when the party rests), the player start in Mossbrook, greybox house blocks in every village |

**The land**, from the dialogue's geography: the Elderwood fills the south-west and centre with Mossbrook in its middle
and the King's Road running north to the **Northgate Pass**; a ridge east of it opens at the **Willowmere Gap** into the
Blightwood; north of the gate, Kingsfork sits where the roads split, north-west to the **Cinderlands** (Kael'thar's
caldera, its rim broken to the south-east) and north-east to the **Rimewood Reach** (Glassmere, frozen, and the peaks
around Hrimgar's Frozen Crown); the **Greyspine** divides fire from frost and the **Rimegate** joins the Rimewood to the
Blightwood. Lakes: Mirrormere and Reedpool (Elderwood), Blackmere (Blightwood), Glassmere (frozen). A preview of the
generated terrain is written to `Saved/WorldBuilder/preview.png` on every pass 15.

## Still to come

1. **Villages:** houses assembled from the modular pieces as packed prefabs (fix one and every village updates), boarded
   copies for the empty villages, Mossbrook's seven villagers and new placeholder villagers and banter per village.
3. **Nature:** an 11-layer landscape material (forest ground, moss, grass, dirt, path, cobble, mud, rock, snow, ash,
   corruption), lakes (a frozen one you can walk on), lava channels, caves, trees and rocks per region with PCG.
4. **Polish:** weather and music per place, map art, HLODs.

## Decisions made on the way

- **Logic is always loaded, art streams.** Region volumes, waystones, places, spawners, arenas and the world actor
  never stream out, so the map and fast travel know every one of them and a camp or boss is never spawned twice when
  its ground comes back. Camps and bosses only exist near the party (activation / spawn radius), which also keeps the
  number of live enemies small.
- **Discoveries are saved in the party's save** (v6) rather than as plain story flags, but answer as flags
  (`Discovered.<id>`, `Waystone.<id>`) so conversations and banter can use them.
- **Region detection is a polygon test on a timer**, not overlap events: regions are huge, a teleport counts, and the
  outline is easy to draw and to give the map.
- **Lava burns once a second** rather than through a damage-over-time refreshed four times a second (that would never
  tick); the lingering burn starts when you step out.
- **No Water or Soundscape plugin:** lakes will be planes with a water material (deep water is a hazard, there's no
  swimming), and ambience is plain loops.
- **Kept off for now:** a minimap and compass, a day / night cycle, swimming.
- **The terrain is generated in C++ and imported**, not sculpted by script: deterministic, testable, 5 s for the whole
  world; the import lands in a *Generated* edit layer so your sculpting on *Sculpt* stays separate.
- **Navigation is built around the demigods at runtime** (navigation invokers) instead of a navmesh built ahead: the
  terrain will keep changing while you sculpt, and nothing has to be rebuilt.
- **Region borders wind and the ridges follow them**: the four regions share border lines (`world_content.py`,
  `BORDER_*`), the ridges run along them with the passes where the roads cross, and *Border Warp* adds a wander.
- **Heights for placing things** come from the landscape where it's loaded, checked against the generated terrain
  (more than 30 m apart: the terrain's height), so placing never needs the landscape's GPU merge.
- **Deleted World Partition actors**: the builder keeps the loading references until the save and removes any file the
  save leaves behind, so rebuilding the terrain never leaves the old landscape on disk.

## Left for you

- **The main menu:** `WB_MainMenu` opens `MAP_Demo_Main` (a Blueprint literal); point its *Open Level* at
  `/Game/WorldsBeyond/Maps/Dominion` when you want the game to start in the open world.
- **Sculpt** on the landscape's *Sculpt* edit layer (Landscape mode → Edit Layers). Re-running pass 15 never touches the
  terrain; `BEYOND_REBUILD_TERRAIN=1` rebuilds it (and loses the sculpting). After sculpting, re-run pass 16 to put the
  gameplay back on the ground (its world map is painted from the generated terrain until the nature pass).
- Move waystones, camps or arenas by editing `world_content.py` and re-running pass 16 with
  `BEYOND_REBUILD_GAMEPLAY=1`, or move the actors by hand (pass 16 only puts them back on the ground).
- Try the systems in any map: `Beyond.WorldDemo` puts a region, a village, two waystones, a shrine, lava and deep water
  next to you (M for the map).

### Later
- Real music per region and village (*Music* / *Ambience* on each `DA_Region_*`).
- A snow particle system for the Rimewood Reach (*Weather → Precipitation*).
- A second, larger navigation agent for Kael'thar (wider than the navmesh's agent).
