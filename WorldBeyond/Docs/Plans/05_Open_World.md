# Plan 5 — Open world: regions, villages, places, boss arenas, map and fast travel (in progress)

Part of the [roadmap](../Roadmap.md). In parts: **5A** (the world systems; built in pass 14, test
`WorldsBeyond.Prototype.World`), **5B** (the world itself: the greybox World Partition map `Dominion`, its terrain
built by script, everything placed; built in passes 15–16, tests `WorldsBeyond.Prototype.WorldBuilder` /
`.OpenWorld`), **5C** (villages, pass 17), **5D** (the textured ground, grass, water and lava, pass 18), **5E**
(trees, shrubs and rocks, pass 19), then caves and polish. How to play with it: *Open world* in
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

## Plan 5C — the villages (built)

Pass 17 (`migrate_pass17.py`) replaces pass 16's grey blocks with real villages, from `village_content.py`.

| Piece | Where | Notes |
|---|---|---|
| House assembler | `Scripts/Migration/village_kit.py` | Houses from the modular pieces on hand: Megascans medieval walls, doors and corners round the footprint (a door in the front, windows mixed by a seed), a second storey, Gable4 on the ends, the House07 thatch roof, a stone chimney, a porch awning, a lean-to shed, a stone footing. Built at 1.25 × the kit (2.5 m storeys). Seven types: `cottage_small`, `cottage`, `gable_house`, `town_house` (two storeys), `longhouse`, `workshop` (barn door and shed), `tower_house`; boarded-up copies for the empty villages. Also the stall, lantern post, palisade run, gates, yard clutter and garden fences |
| Prefabs | `/Game/WorldsBeyond/World/Prefabs/` | `LI_*`: one small level per house type (17 in all, second looks for the three common houses), packed into `BPP_*` Packed Level Actors (one actor of instanced meshes each). Pictures of each in `Saved/WorldBuilder/prefabs/` |
| Village builder | `Source/WorldBeyondEditor/World/BeyondVillageBuilderLibrary.*` | Prefab levels with external actors, packing them without the editor's dialogs, placing an instance, and `CaptureView` (renders the editor world to a PNG, so a headless pass can be looked at) |
| Villages | `migrate_pass17.py` | Houses on one or two rings round an open square with their doors to it, clear of the roads, waystones, places, camps and arenas, only where the ground is flat enough; yard clutter by the walls, fenced gardens behind some houses, a well, market stalls and lantern posts (with lights) round the square, a gate with a lantern where each road comes in, palisades round Duskwatch, Frostholm and Cinderhold; the three empty villages boarded up, their fences broken, no lights. Kingsfork gets its inn and stable. Pictures of each village in `Saved/WorldBuilder/villages/` |
| People | same, `village_content.py` | Mossbrook's seven from Plan 4 on its square; in each other lived-in village a talker (Hunter Garrick, Ferrywoman Hild, Healer Ansel, Sergeant Corvin, Jarl Sigrun, Woodcutter Eskil, Keeper Thora, Forgemaster Brann, Foreman Rusk, Pilgrim Ashka) and a pair who chat; a few lines change once a region's boss falls. Placed as `BP_NPC_Base` with their own settings (no new Blueprints); they wait for the ground to stream in before they stand |
| Dialogue | DT_Dialogue / DT_TextOverHead / DT_Speakers, DA_Banter | The villagers' conversations, chatter and greetings; the duo's banter on entering the Rimewood, the Cinderlands, Kingsfork and each village (the empty ones share one) |

| Village | Houses | People |
|---|---|---|
| Mossbrook | 12, market stalls, the well | Elder Maren, Bram, Captain Ysolde, Tilda and Osk, Pell and Wren |
| Fernhollow / Brackenford / Willowmere | 6 / 6 / 5 | Garrick (raiders west), Hild (the Old Quarry), Ansel (the blight, Duskwatch) |
| Duskwatch | 7, palisade | Sergeant Corvin and two privates (the Weeping Grove) |
| Greyhallow / Marrowfield / Sallow End | 6 / 5 / 4, boarded up | — |
| Frostholm | 9, palisade | Jarl Sigrun (Hrimgar), Ulf the smith, Runa |
| Pinecrest / Hearthfall | 5 / 5 | Eskil (frost wolves), Thora (the warm spring) |
| Cinderhold | 8, palisade | Forgemaster Brann (Kael'thar), Kestra, Dorn |
| Slagford / Emberrest | 5 / 5 | Rusk (the Slag Mines), Ashka (the Shrine of the Pyre) |
| Kingsfork Crossroads | the inn and its stable | — |

90 houses, some 800 props and lanterns, 37 people. Houses go on one or two rings, with more rings when roads leave too
little room.

## Plan 5D — the ground (built)

Pass 18 (`migrate_pass18.py`, data in `ground_content.py`) dresses the land the greybox left flat. The paint layers
pass 15 generated are kept as they are; only the material changes, plus surfaces on the lakes and in the caldera.

| Piece | Where | Notes |
|---|---|---|
| Ground library | `Source/WorldBeyondEditor/World/BeyondGroundLibrary.*` | Builds the landscape material from per-layer *looks*, the region mask, grass types; puts a material on the landscape and all its streaming proxies; grows grass around a camera for headless pictures |
| The material | `/Game/WorldsBeyond/World/Landscape/M_Beyond_Landscape`, instance `MI_Beyond_Landscape` (on the landscape) | Three height blends (colour, normal, roughness) over the 11 layers. Each layer is a set of texture parameters at world scale, with a far sample mixed in to hide repeats, colour controls and its own height contrast; rock is projected from three sides. After the blend: large-scale brightness and dry / lush variation, then a grade per region |
| Region mask | `T_Beyond_RegionMask` (512 px) | Red the Rimewood, green the Cinderlands, blue the Blightwood, from the layout's region weights; the Elderwood is what's left. Laid over the landscape's own coordinates |
| Grass | `/Game/WorldsBeyond/World/Landscape/Grass/LGT_*` | Meadow (green tufts, kidney weed, ground cover), forest floor (ferns, ground cover, twigs, branches, mossy stones), moss mats, the blight's dead weeds and broom, frost's snow clumps and straw, the ash's charred branches; each limited to its regions. The Persepolis tufts are red in their pack: `MI_Beyond_GrassGreen` / `Dead` / `Straw` recolour them |
| Water | `/Game/WorldsBeyond/World/Water/` | `M_Beyond_Water` (Single Layer Water, two panning wave normals; `MI_Water_Elderwood`, `MI_Water_Blight`), `M_Beyond_Ice` (glossy, cracked, a dusting of snow), `M_Beyond_Lava` (flowing glow under a dark crust) |
| Surfaces and hazards | Dominion, folder *Ground* | `WB_Water_*` on Mirrormere, Reedpool and Blackmere with `WB_Hazard_*` deep water under their middles (0.6 m down, round: wading at the edge is fine); `WB_Ice_glassmere` (walkable); `WB_Lava_molten_heart` just under Kael'thar's floor, so it shows as pools in the crater floor's low spots, and its burn. All always loaded |

| Layer | Texture set | In the regions |
|---|---|---|
| ForestGround | Megascans Forest Ground | snow in the Rimewood, corrupted in the Blightwood |
| Moss | Megascans Mossy Ground | the same |
| Grass | **Fab Wild Grass** (4K, height map) | — |
| Dirt (village yards, camps) | village sample Dry Soil, darker | grass patches in the Elderwood; trampled snow, ash, corruption |
| Path | village sample Trampled Mud | trampled snow, ash, corruption |
| Cobble (King's Road, waystones) | Megascans Cobblestone | snow between the stones |
| Mud (shores) | village sample Wet Soil | frozen, corrupted |
| Rock (slopes, cliffs) | Paragon Rampage rock tile, three-sided | snow on the faces that point up |
| Snow | **Fab Fresh Windswept Snow** | — (paths: **Fab Trampled Snow**) |
| Ash | Paragon gravel, dark | faint ember specks |
| Corrupt | Megascans Heavy Mud, violet-black, wet | faint violet veins |

Pictures with the grass grown, at eye height and from above, are in `Saved/WorldBuilder/ground/`.

## Plan 5E — trees, shrubs and rocks (built)

Pass 19 (`migrate_pass19.py`, data in `flora_content.py`) scatters the forests, shrubs, rocks and cliffs over Dominion.
Your picks: **scripted placement into Foliage** (so you can paint and erase by hand afterwards), **medium density**
(about 50 trees a hectare in the groves), plus rocks and stumps everywhere, pine saplings at the forest edges, cliffs on
the steep rock and a few charred trees in the Cinderlands.

| Piece | Where | Notes |
|---|---|---|
| Flora library | `Source/WorldBeyondEditor/World/BeyondFloraLibrary.*` | The scatter (from the generated terrain: layers, slopes, roads, lakes, regions, one grove noise for forests and clearings; clear of the gameplay sites; footprints kept apart, cliffs first, then trees, rocks, shrubs), foliage types, trunk / triangle collision on meshes, flora tiles for the Megaplants, clearing a region, the Flora streaming grid |
| Flora tiles | `Source/WorldBeyond/World/BeyondFloraTile.*` | One actor per 256 m square (`WB_Flora_<x>_<y>`, folder *Flora*): an instanced skinned mesh component per Megaplant (elders, saplings, dead pines, heather, reeds) in the dynamic wind, and invisible instanced cylinders round the trees' trunks (the Megaplants have no physics asset) that block and carve the navmesh |
| Foliage types | `/Game/WorldsBeyond/World/Flora/FT_*` | One per static mesh (spruce, rocks, stumps, logs, cliffs, the sandstone and snow hills), with their cull distances, collision and a brush set up for painting more by hand |
| Wind | `WB_Flora_Wind` (always loaded) | The Procedural Vegetation plugin's `BP_GlobalFoliageActor_UE5`: drives the Megaplants' dynamic wind and their materials' wind |
| Streaming | the map's *World Partition → Runtime Partitions* | A **Flora** grid (512 m cells, loaded within 768 m, with the main grid's HLOD layers) the foliage actors and tiles stream on, so the forest isn't cut off at the main grid's 256 m |

| Region | Trees | Shrubs | Rocks and cliffs |
|---|---|---|---|
| Elderwood | spruce stands (14–24 m) in the groves, elders (scaled to 4–6.5 m small trees) through them, young spruce and Baltic pine saplings at their edges | heather on the open grass, pine seedlings, reeds on Mirrormere's and Reedpool's muddy shores | boulders, mossy rocks and roots, mossy banks on the slopes, stumps; grey crags on the steep rock |
| Blightwood | dead pines, thick in the groves, a few out in the open | reeds at Blackmere | fallen pines and broken stumps, boulders, crags |
| Rimewood Reach | spruce stands, young spruce and saplings at their edges | seedlings | rocky snow piles and boulders, long snow drifts on the open snow, big boulders on the steep rock |
| Cinderlands | a few charred dead pines (bark darkened) | — | layered desert cliffs on the steep rock (the crater's outer wall, the Greyspine, the border), four sandstone mesas (at the Greyspine's foot, behind Emberrest, south of the Slagford path, by the Slag Mines), scattered desert rocks |

About 42,700 in all (the report `last_run_pass19.txt` lists each planting by region):

| | Elderwood | Blightwood | Rimewood | Cinderlands |
|---|---|---|---|---|
| Trees (≈ 22,000) | 7,800 spruce, 4,600 elders, 760 young spruce, 1,000 saplings | 4,000 dead pines | 2,900 spruce, 440 young spruce and saplings | 190 charred pines |
| Shrubs (≈ 16,300) | 12,000 heather, 2,200 seedlings, 1,400 reeds | 200 reeds | 390 seedlings | — |
| Rocks, stumps, logs (≈ 4,000) | 1,000 boulders, 800 mossy rocks, 490 stumps, banks | 270 boulders, 560 fallen pines and stumps | 360 snow piles and boulders, 90 stumps | 430 desert rocks |
| Cliffs and landmarks (≈ 470) | 76 crags | 58 crags | 155 crags, 9 snow drifts | 151 desert cliffs, 4 sandstone mesas (placed by name in `flora_content.LANDMARKS`) |

Pictures at eye height and from above are in `Saved/WorldBuilder/flora/`.

## Still to come

1. **Caves** (Hollowroot, Rotroot Hollow, the Rime Grotto, the Ashen Tube are places without a cave yet).
2. **Polish:** weather and music per place, map art (forests on the map from the flora), HLODs (houses pop in at the
   streaming range until then; the flora streams further on its own grid), snow on the Rimewood's roofs and spruce,
   burnt houses in the Cinderlands.

## Playtest fixes

- **M crashed the editor:** the world map drew each region's outline by adding its first point to the end of the point
  array (`Points.Add(Points[0])`), which asserts when the array grows. The point is copied first now; the World and
  OpenWorld tests keep the map open for a second so a run with rendering draws it.
- **F at a waystone** rested again on every press (held or hammered F): one use a second at most.

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
- **Houses are prefabs packed into Packed Level Actors**: one actor of instanced meshes per house in the open world (it
  streams with the land and draws cheaply), and the level behind it is the one place to fix a house type.
- **Houses aren't entered**: the Megascans walls are one-sided (they're only seen from outside) and the doors are closed
  panels. Interiors would be their own small levels later.
- **The prefabs are made without the editor's dialogs**: *Create Level Instance* insists on a Save As window and the
  packer opens the Content Browser, neither of which a headless pass has, so the village builder makes the level and
  the Blueprint itself and lets the engine fill them.
- **Villagers are `BP_NPC_Base` with their own settings** (name, look, lines) rather than a Blueprint each.
- **The ground is a material swap, not a repaint**: pass 15's layer weights already said where grass, rock, snow and
  roads go, so pass 18 only replaces the material; the terrain, its paint and your *Sculpt* layer are untouched.
- **Height blending** (each layer's height map decides where it shows through its neighbours) instead of plain weights:
  grass between cobbles and snow in rock cracks rather than a soft fade.
- **One region mask instead of more layers**: region looks (dirt that is ash in the Cinderlands, snow on the Rimewood's
  paths, the violet grade) come from a baked mask, so the layers and the generator stay at 11.
- **An instance on the landscape**: every value is a parameter of `MI_Beyond_Landscape`, so tuning or swapping a set
  never needs the script; rebuilding the master keeps the instance.
- **No new corruption textures**: as you asked, the Blightwood's ground is the heavy mud set recoloured, with veins.
- **The Megaplants wait for the trees pass**: they are skeletal meshes (dynamic wind) and landscape grass only takes
  static meshes.
- **Water, ice and lava are flat discs at their levels**, always loaded (five actors), with round hazards (*Radius* on
  `ABeyondHazardVolume`). The crater floor never reaches the layout's `lava_level`, so the lava sits just under
  Kael'thar's floor and shows in its low spots.
- **Ground textures are capped at 2048 px** (most were 4K–8K): world-scale landscape textures barely stream, and 2K over
  a 3 m repeat is still about 1.5 mm a texel.
- **Flora is scattered by script into Foliage, not PCG** (your pick): the scatter reads the generated terrain directly
  (the same every run, about a second for the whole world) and the result is plain foliage you can paint and erase.
  The skeletal Megaplants can't be foliage, so they live in flora tiles of instanced skinned meshes.
- **The Megaplants block through invisible cylinders**: they have no physics asset, so each tree gets a hidden instanced
  cylinder round its trunk (BlockAll, carves the navmesh); shrubs, reeds and seedlings don't block. The spruce got one
  capsule round the trunk instead of the pack's hull, so the party walks under the branches.
- **One grove noise for the whole world**: every planting picks a band of it (spruce in the groves' hearts, elders through
  them, saplings and young spruce at their edges, heather out in the open), so forests have clearings and edges.
- **The flora streams on its own grid** (*Flora*, 512 m cells, loaded within 768 m) rather than the main grid's 256 m,
  so the forest isn't cut off a few hundred metres out. It has the main grid's HLOD layers (Instanced, Merged) on
  coarser grids of its own, so an HLOD build later covers the forests too; none are built yet.
- **Headless pictures show the Megaplants at rest, and only roughly**: the dynamic wind doesn't feed a headless scene
  capture (they'd be invisible), so pass 19 renders them in their rest pose (`r.InstancedSkinnedMeshes.ForceRefPose`).
  In that mode the dead pines' branch pieces float above their trunks and the saplings lose their needles; that's
  the capture only. In an editor viewport they draw whole (checked on 2026-10-11 with screenshots of the Blightwood,
  the Elderwood and Mirrormere).
- **Spruce needles tinted darker** (`Flora/Tints/`): the pack's summer needles read golden next to the Elderwood's grass.

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
- **Refine a house type:** select any house in Dominion → *Level Instance → Edit* (or open its `LI_House_*` level in
  `/Game/WorldsBeyond/World/Prefabs/`), change it, *Commit*: the packed Blueprint is repacked and every copy in every
  village follows. Re-running pass 17 never touches a prefab that exists (`BEYOND_REBUILD_PREFABS=1` assembles them all
  again and loses your edits).
- **Re-lay a village** after editing `village_content.py`: `BEYOND_REBUILD_VILLAGES=mossbrook` (or `=1` for all);
  anything you placed by hand with that village's `WB_VIL_` label prefix goes with it. Moving houses by hand is fine.
- Portraits for the new speakers (`DT_Speakers`: Garrick, Hild, Ansel, Corvin, Sigrun, Eskil, Thora, Brann, Rusk,
  Ashka) and real names and lines where the placeholders don't fit.
- **Tune the ground** in `/Game/WorldsBeyond/World/Landscape/MI_Beyond_Landscape` (parameters grouped by layer: tile
  size, tint, saturation, brightness, roughness, height contrast / offset, region amounts; *Regions* for the grades,
  *Variation* for the large-scale changes). To swap a layer's textures, set them there with textures of the same kind
  (virtual or not), or put the new set first in its list in `ground_content.py` and run pass 18 with
  `BEYOND_REBUILD_GROUND=1`.
- **Paint** on the landscape with the 11 layers as before: the textured ground follows (Landscape mode → Paint, on the
  *Sculpt* edit layer).
- The corruption is recoloured mud; a real dark-forest ground set from Fab can replace it in the *Corrupt* group.
- **Paint or erase trees and rocks** in Foliage mode: the `FT_*` types in `/Game/WorldsBeyond/World/Flora/` are set up
  as brushes (density, scale, slope). Megaplants: select a `WB_Flora_<x>_<y>` tile (folder *Flora*) and remove
  instances from its components, or edit `flora_content.py` and re-run pass 19 for that region
  (`BEYOND_REBUILD_FLORA=forest`; it clears what you painted there with the `FT_` types too).
- **After sculpting**, foliage doesn't follow the ground: re-run pass 19 for the regions you changed (or use Foliage
  mode's *Snap to ground* on the instances).
- **Check the Megaplants sway** in the editor or in play (elders, saplings, dead pines, heather, reeds): headless
  pictures can only show them at rest.
- **Before packaging**: the Megaplants' master material (`MA_Foliage_Trees` in the Procedural Vegetation plugin's sample
  content) gets *Used with Instanced Skinned Mesh* only in memory while the editor runs; a cooked game needs it saved
  (or a copy of the master in the project).

### Later
- Real music per region and village (*Music* / *Ambience* on each `DA_Region_*`).
- A snow particle system for the Rimewood Reach (*Weather → Precipitation*).
- A second, larger navigation agent for Kael'thar (wider than the navmesh's agent).
