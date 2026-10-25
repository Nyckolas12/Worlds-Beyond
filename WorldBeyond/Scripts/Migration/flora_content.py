"""
Worlds Beyond - Plan 5E: trees, shrubs and rocks (read by migrate_pass19.py). Edit freely, then run pass 19 again with
BEYOND_REBUILD_FLORA=1 (everything) or BEYOND_REBUILD_FLORA=forest,frost (only those regions: forest, frost, molten,
blight). A rebuild clears what pass 19 placed there, and anything you painted with its FT_ foliage types.

Sizes are metres. Regions are shares in the order (Elderwood, Rimewood, Cinderlands, Blightwood), like ground_content.
Meshes: a list of (paths, weight): every path found is used, each with that weight. Static meshes go into the World
Partition foliage (paint more of them by hand in Foliage mode with /Game/WorldsBeyond/World/Flora/FT_*); the skeletal
Megaplants (dynamic wind) go into flora tiles (WB_Flora_<x>_<y>), which the Foliage tool can't hold.

Measured (pass 19's first run; the report lists them again): elder 2-2.8 m (scaled up to small trees here), Baltic pine
saplings 0.9-5 m, dead pines 7.8-22.7 m, heather 0.1-0.34 m (scaled up), reeds about 2 m; spruce full 10-12 m, half
15-17 m, small 0.5-7.7 m; Icelandic boulder 1.6 m tall, the desert cliff 25.7 x 13.6 x 14.9 m, the sandstone terrain
577 x 365 x 107 m, the snow hill 15 x 7 x 1.7 m, the rocky snow pile 7.3 x 4.2 x 1 m.
"""

MEGA = "/Game/Megaplant_Library/"
ELDER = [MEGA + "Tree_Elder/Tree_Elder_01/Tree_Elder_01_%s" % c for c in "ABCD"]
SAPLINGS_TALL = [MEGA + "Tree_Baltic_Pine/Tree_Baltic_Pine_Saplings_01/Tree_Baltic_Pine_Sapling_01_%s" % c for c in "AB"]
SAPLINGS_SMALL = [MEGA + "Tree_Baltic_Pine/Tree_Baltic_Pine_Saplings_01/Tree_Baltic_Pine_Sapling_01_%s" % c for c in "CD"]
DEAD_PINES_TALL = [MEGA + "Tree_Dead_Pine/Tree_Dead_Pine_01/Tree_Dead_Pine_%s" % c for c in "ABCD"]
DEAD_PINE_SNAG = [MEGA + "Tree_Dead_Pine/Tree_Dead_Pine_01/Tree_Dead_Pine_E"]
HEATHER = [MEGA + "Shrub_Heather/Shrub_Heather_01/Shrub_Heather_01_%s" % c for c in "ABCE"]
REEDS = [MEGA + "Plant_Dry_Common_Reeds/Plant_Dry_Common_Reeds_01/Plant_Dry_Common_Reed_%s" % c for c in "ABCD"]

SPRUCE = "/Game/ThirdParty/PN_interactiveSpruceForest/Meshes/"
SPRUCE_FULL = [SPRUCE + "full/high/spruce_full_%02d" % i for i in (1, 2, 3)]
SPRUCE_HALF = [SPRUCE + "half/high/spruce_half_%02d" % i for i in (1, 2, 3, 4)]
SPRUCE_YOUNG = [SPRUCE + "small/spruce_small_%02d" % i for i in (5, 6, 7)]

ASSETS = "/Game/Megascans/3D_Assets/"
BOULDER = ASSETS + "IcelandicBoulder/SM_IcelandicBoulder"
BEACH_ROCK = ASSETS + "HovsBeachRock/SM_HovsBeachRock"
MOSSY_ROCK = ASSETS + "IcelandicMossyRock/SM_IcelandicMossyRock"
MOSSY_ROCKS = [ASSETS + "MossyRocksA/SM_MossyRocksA", ASSETS + "MossyRocksB/SM_MossyRocksB"]
STUMPS = [ASSETS + "OldStump/SM_OldStump", ASSETS + "BrokenTreeStump/SM_BrokenTreeStump"]
PINE_STUMP = ASSETS + "BrokenPineStump/SM_BrokenPineStump"
FALLEN_PINE = ASSETS + "BrokenPineAssembly/SM_BrokenPineAssembly"
ROOTS = ASSETS + "ForestRootsA/SM_ForestRootsA"
EMBANKMENTS = [ASSETS + "MossyEmbankmentA/SM_MossyEmbankmentA_01", ASSETS + "MossyEmbankmentB/SM_MossyEmbankmentB",
               ASSETS + "MossyEmbankmentC/SM_MossyEmbankmentC", ASSETS + "MossyEmbankmentE/SM_MossyEmbankmentE"]
ICELANDIC_TERRAIN = ASSETS + "IcelandicTerrain/SM_IcelandicTerrain_High"

FAB_3D = "/Game/Fab/Megascans/3D/"
DESERT_CLIFF = FAB_3D + "Desert_Western_Cliff_Layered_XL_14_vjrmfb1ab/Medium/vjrmfb1ab_tier_2/StaticMeshes/vjrmfb1ab_tier_2"
DESERT_ROCKS = FAB_3D + "Desert_Western_Pile_Rock_Small_01_ul4lehiga/Medium/ul4lehiga_tier_2/StaticMeshes/ul4lehiga_tier_2"
SANDSTONE = FAB_3D + "Gigantic_Sandstone_Terrain_wldhdhxva/Medium/wldhdhxva_tier_2/StaticMeshes/wldhdhxva_tier_2"
SNOW_HILL = FAB_3D + "Huge_Snow_Hill_vdspdazqa/Medium/vdspdazqa_tier_2/StaticMeshes/vdspdazqa_tier_2"
SNOW_PILE = FAB_3D + "Rocky_Snow_Pile_xfwgbg0qx/Medium/xfwgbg0qx_tier_2/StaticMeshes/xfwgbg0qx_tier_2"

# ---------------------------------------------------------------- shared
SEED = 1977
# Width (m) of the groves and the clearings between them (one noise for the whole world; each planting picks a band of
# it: 0..0.5 is the thickest half of the land, 0.5..0.65 the groves' edges, 0.65..1 the open ground)
GROVE_SCALE = 220.0
# Roads, village yards and camp floors: nothing grows where these weigh more than ROAD_WEIGHT (beside each road's own
# clearance)
ROAD_LAYERS = ["Path", "Cobble", "Dirt"]
ROAD_WEIGHT = 0.3

# Kept clear (m): from world_content's villages, waystones, camps, arenas, places and the player start
CLEARANCE = dict(village=12.0, area=10.0, waystone=20.0, camp=30.0, arena=15.0, place_share=0.6, player_start=40.0)
# Shrubs (no collision) may grow at camps, arenas and places (not in villages or round waystones)
SHRUBS_AT = ("camp", "arena", "place")

# Streaming: the flora's own World Partition grid, loaded further out than the main grid's 256 m
FLORA_GRID = dict(name="Flora", cell=512.0, range=768.0)
TILE_SIZE = 256.0

# The Megaplants' sway: the Procedural Vegetation plugin's wind (its transform provider, else our own; and its global
# wind actor, which drives the wind and the foliage materials' wind parameters)
WIND_DATA = ["/ProceduralVegetationEditor/SampleAssets/Materials/GlobalFoliageActor/Wind_TransformProvider"]
WIND_ACTOR = "/ProceduralVegetationEditor/SampleAssets/Materials/GlobalFoliageActor/BP_GlobalFoliageActor_UE5"

# Material instances made in /Game/WorldsBeyond/World/Flora/ (parent, vector parameters)
DEAD_PINE_MATERIALS = MEGA + "Tree_Dead_Pine/Materials/"
CHARRED = (0.09, 0.075, 0.065)
MATERIALS = {
    "MI_Beyond_DeadPine_Charred_Bark": dict(parent=DEAD_PINE_MATERIALS + "MI_Dead_Pine_Bark_01", vectors={"BaseColor Tint": CHARRED}),
    "MI_Beyond_DeadPine_Charred_Plates": dict(parent=DEAD_PINE_MATERIALS + "MI_Dead_Pine_Barkplates_01", vectors={"BaseColor Tint": CHARRED}),
    "MI_Beyond_DeadPine_Charred_Branch": dict(parent=DEAD_PINE_MATERIALS + "MI_Dead_Pine_BrokenBranch_01", vectors={"BaseColor Tint": CHARRED}),
}
CHARRED_SLOTS = ["MI_Beyond_DeadPine_Charred_Bark", "MI_Beyond_DeadPine_Charred_Plates", "MI_Beyond_DeadPine_Charred_Branch"]

# Tints for a planting's meshes (tint=<name>): a child of each of the mesh's materials that has these vector parameters,
# in /Game/WorldsBeyond/World/Flora/Tints/ (the foliage types wear them). The spruce pack's needles are a golden summer
# green; the Elderwood and the Rimewood want them darker
TINTS = {
    "spruce_green": {"Leaf Tint": (0.42, 0.62, 0.36), "Branch Tint": (0.55, 0.68, 0.5)},
}

# ---------------------------------------------------------------- plantings
# Placed in this order by kind (cliff, tree, rock, shrub): what comes first claims its ground.
# kind: cliff / tree / rock / shrub. meshes: [(paths, weight)]. regions: shares (Elderwood, Rimewood, Cinderlands, Blightwood).
# layers / min_layer: grows where these layers together weigh at least min_layer (none: any ground).
# avoid / max_avoid: never where these weigh more (default: the road layers, ROAD_WEIGHT).
# slope: (min, max) degrees. grove: (min, max) band of the grove noise. shore: (min, max) m above the water of lakes
# (only those in `lakes`, else any unfrozen lake). altitude: (min, max) m.
# density: per hectare. size: (min, max) height in m (else scale: (min, max)). sink: share of the height pushed into the
# ground, sink_m: plus metres. footprint: m kept free round it (0: from the mesh's width). align: tilt with the ground (0..1).
# tilt: random lean (degrees). downhill: face down the slope (+ yaw degrees) instead of a random turn. road: m from a road's edge.
# cull: (start, end) m (0, 0: never). shadow. collision: None, ("trunk", radius, height) at the mesh's own scale, "mesh"
# (its own simple collision, else its triangles) or "complex" (its triangles). materials: overrides (MATERIALS names).
# tint: a TINTS entry for the meshes' own materials (static meshes: the first planting to use a mesh sets its foliage type).
# paint: (per 10 x 10 m, (scale min, max)) for painting the foliage type by hand.
ELDERWOOD, RIMEWOOD, CINDERLANDS, BLIGHTWOOD = (1, 0, 0, 0), (0, 1, 0, 0), (0, 0, 1, 0), (0, 0, 0, 1)

# The Persepolis kit's tall grass for the open meadows
PERSEPOLIS_FOLIAGE = "/Game/SICKA_PERSEPOLIS/Mesh/Foliage/"
MEADOW_GRASS = [PERSEPOLIS_FOLIAGE + "SM_Grasspatch_%d" % i for i in range(1, 9)]
MEADOW_PROPS = [PERSEPOLIS_FOLIAGE + "SM_FolliageProp_%s" % n for n in ("2", "3", "4", "5", "6")]

PLANTINGS = {
    # ------------------------------------------------ landmarks and cliffs
    # The desert cliff on the Cinderlands' steep rock (the crater's outer wall, the Greyspine, the border mountains)
    "cinder_cliffs": dict(kind="cliff", meshes=[([DESERT_CLIFF], 1.0)], regions=CINDERLANDS, layers=["Rock"], min_layer=0.35,
                          slope=(30, 48), density=1.6, scale=(0.8, 1.6), sink=0.4, align=0.6, downhill=True, yaw=90.0,
                          road=8.0, cull=(0, 0), collision="complex", paint=(0.05, (0.5, 1.1))),
    # Grey boulders and slabs on the Elderwood's and Blightwood's steep rock
    "forest_crags": dict(kind="cliff", meshes=[([BOULDER, BEACH_ROCK], 1.0), ([ICELANDIC_TERRAIN], 0.5)],
                         regions=(1, 0, 0, 1), layers=["Rock"], min_layer=0.35, slope=(26, 55), density=2.0,
                         size=(4.0, 8.0), sink=0.25, align=0.5, road=6.0, cull=(0, 0), collision="mesh", paint=(0.05, (2.5, 5.0))),
    # The Rimewood's crags: big boulders and rocky snow piles
    "frost_crags": dict(kind="cliff", meshes=[([BOULDER], 1.0), ([SNOW_PILE], 1.0)], regions=RIMEWOOD, layers=["Rock", "Snow"],
                        min_layer=0.4, slope=(26, 55), density=2.0, scale=(1.6, 4.0), sink=0.25, align=0.7, road=6.0,
                        cull=(0, 0), collision="mesh", paint=(0.05, (1.5, 3.5))),
    # Long drifts on the Rimewood's open snow
    "snow_hills": dict(kind="cliff", meshes=[([SNOW_HILL], 1.0)], regions=RIMEWOOD, layers=["Snow"], min_layer=0.5,
                       slope=(0, 18), grove=(0.5, 1.0), density=0.25, scale=(1.0, 2.4), sink=0.15, align=1.0, road=6.0,
                       cull=(0, 0), collision="complex", paint=(0.05, (1.0, 2.4))),

    # ------------------------------------------------ trees
    # The Elderwood's spruce stands (the groves' hearts)
    "elder_spruce": dict(kind="tree", altitude=(0, 320), meshes=[(SPRUCE_FULL, 1.0), (SPRUCE_HALF, 0.25)], regions=ELDERWOOD,
                         avoid=ROAD_LAYERS + ["Rock", "Mud"], max_avoid=0.4, slope=(0, 30), grove=(0.0, 0.5), density=30.0,
                         size=(14.0, 24.0), sink_m=0.3, footprint=2.2, road=5.0, cull=(0, 0),
                         tint="spruce_green", collision=("trunk", 0.16, 4.0), paint=(0.25, (1.2, 1.8))),
    # Elders: small trees through the groves and thicker at their edges
    "elders": dict(kind="tree", altitude=(0, 300), meshes=[(ELDER, 1.0)], regions=ELDERWOOD, avoid=ROAD_LAYERS + ["Rock", "Mud"], max_avoid=0.4,
                   slope=(0, 28), grove=(0.08, 0.6), density=18.0, size=(3.8, 6.5), sink_m=0.1, footprint=1.6, road=4.0,
                   cull=(0, 0), collision=("trunk", 0.07, 1.6)),
    # Young spruce at the groves' edges (the Elderwood and the Rimewood)
    "young_spruce": dict(kind="tree", altitude=(0, 330), meshes=[(SPRUCE_YOUNG, 1.0)], regions=(1, 0.8, 0, 0), avoid=ROAD_LAYERS + ["Rock", "Mud"],
                         max_avoid=0.4, slope=(0, 30), grove=(0.42, 0.64), density=6.0, size=(3.5, 7.5), sink_m=0.1,
                         footprint=1.4, road=4.0, cull=(0, 0), tint="spruce_green", collision=("trunk", 0.08, 2.5), paint=(0.3, (0.8, 1.3))),
    # Baltic pine saplings at the edges
    "pine_saplings": dict(kind="tree", altitude=(0, 300), meshes=[(SAPLINGS_TALL, 1.0)], regions=(1, 0.6, 0, 0), avoid=ROAD_LAYERS + ["Rock", "Mud"],
                          max_avoid=0.4, slope=(0, 30), grove=(0.45, 0.68), density=8.0, size=(3.0, 5.5), sink_m=0.05,
                          footprint=1.1, road=3.0, cull=(0, 300), collision=("trunk", 0.05, 1.8)),
    # The Blightwood: dead pines, thick and grey
    "dead_pines": dict(kind="tree", meshes=[(DEAD_PINES_TALL, 1.0), (DEAD_PINE_SNAG, 0.4)], regions=BLIGHTWOOD,
                       avoid=ROAD_LAYERS + ["Rock", "Mud"], max_avoid=0.4, slope=(0, 30), grove=(0.0, 0.62), density=28.0,
                       scale=(0.85, 1.25), sink_m=0.2, footprint=1.8, tilt=4.0, road=5.0, cull=(0, 0),
                       collision=("trunk", 0.25, 4.0)),
    # A few dead pines out in the Blightwood's open ground
    "dead_pines_open": dict(kind="tree", meshes=[(DEAD_PINES_TALL, 1.0), (DEAD_PINE_SNAG, 1.0)], regions=BLIGHTWOOD,
                            avoid=ROAD_LAYERS + ["Rock", "Mud"], max_avoid=0.4, slope=(0, 30), grove=(0.62, 1.0), density=2.5,
                            scale=(0.8, 1.15), sink_m=0.2, footprint=1.8, tilt=8.0, road=5.0, cull=(0, 0),
                            collision=("trunk", 0.25, 4.0)),
    # The Rimewood's spruce stands
    "frost_spruce": dict(kind="tree", altitude=(0, 330), meshes=[(SPRUCE_FULL, 1.0), (SPRUCE_HALF, 0.35)], regions=RIMEWOOD,
                         avoid=ROAD_LAYERS + ["Rock", "Mud"], max_avoid=0.45, slope=(0, 32), grove=(0.0, 0.45), density=34.0,
                         size=(12.0, 22.0), sink_m=0.3, footprint=2.0, road=5.0, cull=(0, 0),
                         tint="spruce_green", collision=("trunk", 0.16, 4.0), paint=(0.25, (1.1, 1.7))),
    # Charred dead pines, few and far between in the Cinderlands
    "charred_pines": dict(kind="tree", meshes=[(DEAD_PINES_TALL, 1.0), (DEAD_PINE_SNAG, 1.0)], regions=CINDERLANDS,
                          avoid=ROAD_LAYERS + ["Rock", "Mud"], max_avoid=0.4, slope=(0, 25), grove=(0.0, 0.35), density=5.0,
                          scale=(0.6, 0.95), sink_m=0.3, footprint=2.0, tilt=10.0, road=5.0, cull=(0, 0),
                          collision=("trunk", 0.25, 4.0), materials=CHARRED_SLOTS),

    # ------------------------------------------------ rocks, stumps, logs
    "forest_boulders": dict(kind="rock", meshes=[([BOULDER, BEACH_ROCK, MOSSY_ROCK], 1.0)], regions=(1, 0, 0, 0.7),
                            slope=(0, 35), density=2.0, scale=(0.6, 1.7), sink=0.15, align=0.6, tilt=8.0, road=3.0,
                            cull=(0, 500), collision="mesh", paint=(0.1, (0.6, 1.7))),
    "mossy_rocks": dict(kind="rock", meshes=[(MOSSY_ROCKS, 1.0), ([ROOTS], 0.5)], regions=ELDERWOOD, slope=(0, 30),
                        grove=(0.0, 0.7), density=2.5, scale=(0.8, 1.6), sink=0.1, align=1.0, road=2.0, cull=(0, 250),
                        collision="mesh", paint=(0.2, (0.8, 1.6))),
    "mossy_banks": dict(kind="rock", meshes=[(EMBANKMENTS, 1.0)], regions=ELDERWOOD, slope=(10, 30), density=1.0,
                        scale=(1.4, 2.6), sink=0.2, align=1.0, downhill=True, yaw=90.0, road=4.0, cull=(0, 400),
                        collision="mesh", paint=(0.05, (1.4, 2.6))),
    "stumps": dict(kind="rock", meshes=[(STUMPS, 1.0)], regions=(1, 0.5, 0, 0), avoid=ROAD_LAYERS + ["Rock", "Mud"], max_avoid=0.4,
                   slope=(0, 25), grove=(0.0, 0.65), density=1.5, scale=(0.8, 1.3), sink_m=0.05, road=2.0, cull=(0, 250),
                   collision="mesh", paint=(0.1, (0.8, 1.3))),
    "blight_logs": dict(kind="rock", meshes=[([FALLEN_PINE], 1.0), ([PINE_STUMP], 1.2)], regions=BLIGHTWOOD,
                        avoid=ROAD_LAYERS + ["Rock", "Mud"], max_avoid=0.4, slope=(0, 22), density=3.0, scale=(0.8, 1.4),
                        sink_m=0.1, align=0.8, road=3.0, cull=(0, 300), collision="mesh", paint=(0.1, (0.8, 1.4))),
    "frost_rocks": dict(kind="rock", meshes=[([SNOW_PILE], 1.0), ([BOULDER], 0.8)], regions=RIMEWOOD, slope=(0, 35),
                        density=2.2, scale=(0.6, 1.6), sink=0.15, align=0.8, road=3.0, cull=(0, 500), collision="mesh",
                        paint=(0.1, (0.6, 1.6))),
    "cinder_rocks": dict(kind="rock", meshes=[([DESERT_ROCKS], 1.0), ([BOULDER], 0.4)], regions=CINDERLANDS, slope=(0, 35),
                         density=3.0, scale=(0.8, 2.2), sink=0.15, align=0.8, tilt=6.0, road=3.0, cull=(0, 500),
                         collision="mesh", paint=(0.1, (0.8, 2.2))),

    # ------------------------------------------------ shrubs (no collision)
    # Heather on the Elderwood's open grass and moss
    "heather": dict(kind="shrub", meshes=[(HEATHER, 1.0)], regions=ELDERWOOD, layers=["Grass", "Moss", "ForestGround"],
                    min_layer=0.4, slope=(0, 30), grove=(0.5, 1.0), density=45.0, size=(0.4, 0.85), sink_m=0.02,
                    footprint=0.45, road=1.5, cull=(0, 90), shadow=False),
    # Pine seedlings among them
    "pine_seedlings": dict(kind="shrub", meshes=[(SAPLINGS_SMALL, 1.0)], regions=(1, 0.5, 0, 0), avoid=ROAD_LAYERS + ["Rock", "Mud"],
                           max_avoid=0.4, slope=(0, 30), grove=(0.35, 0.75), density=10.0, size=(0.7, 2.0), sink_m=0.02,
                           footprint=0.5, road=1.5, cull=(0, 140)),
    # Reeds on the lakes' muddy shores
    "reeds": dict(kind="shrub", meshes=[(REEDS, 1.0)], regions=(1, 0, 0, 0.6), layers=["Mud", "Grass", "Moss"], min_layer=0.3,
                  shore=(-0.5, 0.9), lakes=["mirrormere", "reedpool", "blackmere"], slope=(0, 22), density=500.0,
                  size=(1.5, 2.4), sink_m=0.05, footprint=0.4, road=2.0, cull=(0, 160)),

    # ------------------------------------------------ the redesign: valley forests, meadows, river banks
    # Spruce thick on the Elderwood's low valley floors, where the rivers and lakes run
    "valley_spruce": dict(kind="tree", meshes=[(SPRUCE_FULL, 1.0), (SPRUCE_HALF, 0.4)], regions=ELDERWOOD,
                          avoid=ROAD_LAYERS + ["Rock", "Mud"], max_avoid=0.4, slope=(0, 26), grove=(0.0, 0.75),
                          altitude=(0, 34), density=14.0, size=(14.0, 24.0), sink_m=0.3, footprint=2.2, road=5.0,
                          cull=(0, 0), tint="spruce_green", collision=("trunk", 0.16, 4.0), paint=(0.25, (1.2, 1.8))),
    # Tall grass meadows in the open, between the groves
    "meadow_grass": dict(kind="shrub", meshes=[(MEADOW_GRASS, 1.0), (MEADOW_PROPS, 0.35)], regions=(1, 0, 0, 0.3),
                         layers=["Grass", "Moss"], min_layer=0.35, avoid=ROAD_LAYERS + ["Rock", "Mud"], max_avoid=0.3,
                         slope=(0, 25), grove=(0.55, 1.0), density=40.0, scale=(0.9, 1.4), sink_m=0.03, footprint=0.6,
                         road=1.5, cull=(0, 110), shadow=False),
    # Reeds along the rivers' muddy banks
    "river_reeds": dict(kind="shrub", meshes=[(REEDS, 1.0)], regions=(1, 0.5, 0, 0.6), layers=["Mud"], min_layer=0.45,
                        slope=(0, 20), density=180.0, size=(1.5, 2.4), sink_m=0.05, footprint=0.4, road=2.0, cull=(0, 160)),
}

# ---------------------------------------------------------------- landmarks (placed where named, not scattered)
# Big single meshes as foliage too (move or delete them in Foliage mode). Kinds: the mesh and how it's placed (like a
# planting: collision, cull, sink share of its height, paint). Each landmark: name, kind, (m east, m north), yaw (degrees,
# its long side's bearing from east), scale. The pass checks each against the roads and gameplay sites (a warning, and
# it's skipped, when one comes too close) and keeps the scatter out of its footprint.
LANDMARK_KINDS = {
    # Layered sandstone mesas (the mesh is 577 x 365 x 107 m: at 0.2 a mesa 115 m long and 21 m tall)
    "sandstone": dict(kind="cliff", meshes=[([SANDSTONE], 1.0)], collision="complex", cull=(0, 0), sink=0.12,
                      paint=(0.01, (0.15, 0.3))),
}
LANDMARKS = [
    # At the Greyspine's west foot, between the Shrine of the Pyre and the ridge
    ("mesa_greyspine", "sandstone", (-270, 1020), 80.0, 0.2),
    # Behind Emberrest, framing the Emberfall vista
    ("mesa_emberfall", "sandstone", (-170, 1620), 100.0, 0.17),
    # South of the Slagford path, against the Northgate ridge
    ("mesa_slagford", "sandstone", (-1250, 545), 10.0, 0.18),
    # By the Slag Mines in the far west
    ("mesa_slag_mines", "sandstone", (-1720, 690), 60.0, 0.16),
]
# Kept from roads (m past the mesa's half length) and from gameplay sites' clearings
LANDMARK_ROAD_MARGIN = 8.0

# ---------------------------------------------------------------- pictures (Saved/WorldBuilder/flora/)
# name, target (a region id, ("lake", id), ("volcano",) or (m east, m north)), from (direction east / north)
VIEWS = [
    ("mossbrook", "village_mossbrook", (0.6, -0.8)),
    ("elderwood_forest", (-700, -1000), (0.7, 0.7)),
    ("elderwood_west", (-1300, -600), (0.8, -0.6)),
    ("mirrormere_reeds", ("lake", "mirrormere"), (-0.8, 0.6)),
    ("reedpool", ("lake", "reedpool"), (0.6, 0.8)),
    ("blightwood", (1350, -1050), (-0.7, 0.7)),
    ("duskwatch", "village_duskwatch", (-0.7, -0.7)),
    ("rimewood", (1150, 1150), (-0.6, -0.8)),
    ("frostholm", "village_frostholm", (0.7, -0.7)),
    ("cinderhold", "village_cinderhold", (-0.6, -0.8)),
    ("cinderlands_flats", (-1300, 750), (0.7, 0.7)),
    ("caldera", ("volcano",), (0.7, -0.7)),
    ("mesa_greyspine", (-345, 1010), (-1.0, -0.15)),
]
