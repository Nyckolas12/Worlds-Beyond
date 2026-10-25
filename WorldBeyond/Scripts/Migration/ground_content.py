"""
Worlds Beyond - Plan 5D: the open world's ground (read by migrate_pass18.py). Edit freely, then run pass 18 again:
BEYOND_REBUILD_GROUND=1 rebuilds the landscape material, the region mask and the grass types from this file,
BEYOND_REBUILD_WATER=1 the water / ice / lava materials and surfaces.

Every texture and mesh is a list of candidates: the first one that exists is used (the report says which), so a Fab set
you import later can go first in a list. Tile sizes are metres per repeat; colours are linear (r, g, b).

Fine tuning without the script: open /Game/WorldsBeyond/World/Landscape/MI_Beyond_Landscape; every value here is a
parameter there, grouped by layer (swap a layer's textures with ones of the same kind, virtual or not).
"""

FAB_GRASS = "/Game/Fab/Megascans/Surfaces/Wild_Grass_sfknaeoa/High/sfknaeoa_tier_1/Textures/T_sfknaeoa_4K_"
FAB_SNOW = "/Game/Fab/Megascans/Surfaces/Fresh_Windswept_Snow_ugspafgdy/Medium/ugspafgdy_tier_2/Textures/T_ugspafgdy_2K_"
FAB_TRAMPLED = "/Game/Fab/Megascans/Surfaces/Trampled_Snow_vcqnfdk/Medium/vcqnfdk_tier_2/Textures/T_vcqnfdk_2K_"
LAND = "/Game/Materials/Landscape/Textures/"
MEGA = "/Game/Megascans/Surfaces/"
MINIONS_ENV = "/Game/ParagonMinions/FX/Textures/Environment/"
RAMPAGE_ENV = "/Game/ParagonRampage/FX/Textures/Environment/"
RAMPAGE_ROCK = "/Game/ParagonRampage/Characters/Global/MaterialLayers/Rock/"

NOISE_GROUND = "/Game/ParagonRampage/Characters/Global/Textures/Noise/T_GroundNoise01"
NOISE_SOFT = "/Game/ParagonMinions/FX/Textures/Tile/Noise/T_Perlin_Soft_Packed"
LAVA_FLOOR = "/Game/ParagonMinions/Characters/Global/MaterialLayers/Special/T_LavaFloor_02_D"
WATER_NORMAL = "/Game/ParagonMinions/FX/Textures/Water/T_Water_N"
CRACKLE_NORMAL = "/Game/ParagonMinions/FX/Textures/Buff_Specific/Buff_Red/T_MicroCrackle_N"

# ---------------------------------------------------------------- the 11 layers
# base / normal / roughness / height: candidate textures (tileable surfaces only: a scanned rock's texture sheet repeats
# as a grid of blobs). roughness_channel: R (a roughness map), G (an ORM map), or None (use "roughness_value").
# height: a height map's red; none: the base colour's brightness.
# frost: how the Rimewood changes the layer (TrampledSnow, SnowInGaps, SnowOnTop).
# frost_layer / molten_layer / blight_layer: (layer, amount) the layer turns into where that region is.
# patch: (layer, share, tile m) patches of another layer through this one, in the Elderwood only.
# glow: Embers / Veins + its settings.
LAYERS = {
    "ForestGround": dict(
        base=[MEGA + "ForestGround/T_ForestGround_A"], normal=[MEGA + "ForestGround/T_ForestGround_N"],
        roughness=[MEGA + "ForestGround/T_ForestGround_R"], roughness_channel="R", height=[MEGA + "ForestGround/T_ForestGround_D"],
        tile=3.5, saturation=0.95, brightness=1.0, height_contrast=1.2, height_offset=-0.05,
        frost_layer=("Snow", 0.85), blight_layer=("Corrupt", 0.55)),
    "Moss": dict(
        base=[MEGA + "MossyGround/T_MossyGround_A", RAMPAGE_ENV + "T_LandscapeMoss_D"],
        normal=[MEGA + "MossyGround/T_MossyGround_N", RAMPAGE_ENV + "T_LandscapeMoss_N"],
        roughness=[], roughness_channel=None, roughness_value=0.82, height=[],
        tile=2.5, tint=(0.95, 1.0, 0.9), height_contrast=1.0, height_offset=0.05,
        frost_layer=("Snow", 0.8), blight_layer=("Corrupt", 0.5)),
    "Grass": dict(
        base=[FAB_GRASS + "B", LAND + "GrassyLayer/T_GrassyLayer_A"], normal=[FAB_GRASS + "N", LAND + "GrassyLayer/T_GrassyLayer_N"],
        roughness=[FAB_GRASS + "ORM"], roughness_channel="G", height=[FAB_GRASS + "H", LAND + "GrassyLayer/T_GrassyLayer_D"],
        tile=2.5, saturation=1.05, height_contrast=1.1, height_offset=0.1),
    "Dirt": dict(
        base=[LAND + "DrySoil/T_DrySoil_A"], normal=[LAND + "DrySoil/T_DrySoil_N"], roughness=[LAND + "DrySoil/T_DrySoil_R"],
        roughness_channel="R", height=[LAND + "DrySoil/T_DrySoil_D"], tile=3.0, tint=(0.92, 0.85, 0.78), saturation=0.7, brightness=0.55,
        frost="TrampledSnow", frost_amount=0.85, molten_layer=("Ash", 0.55), blight_layer=("Corrupt", 0.4),
        patch=("Grass", 0.5, 10.0)),
    "Path": dict(
        base=[LAND + "TrampledMud/T_TrampledMud_A"], normal=[LAND + "TrampledMud/T_TrampledMud_N"],
        roughness=[LAND + "TrampledMud/T_TrampledMud_R"], roughness_channel="R", height=[LAND + "TrampledMud/T_TrampledMud_D"],
        tile=3.0, tint=(0.95, 0.88, 0.8), saturation=0.75, brightness=0.55,
        frost="TrampledSnow", frost_amount=1.0, molten_layer=("Ash", 0.45), blight_layer=("Corrupt", 0.3)),
    "Cobble": dict(
        base=[MEGA + "Cobblestone/T_Cobblestone_A"], normal=[MEGA + "Cobblestone/T_Cobblestone_N"],
        roughness=[MEGA + "Cobblestone/T_Cobblestone_R"], roughness_channel="R", height=[],
        tile=3.5, brightness=0.85, height_contrast=1.5, height_offset=0.1, frost="SnowInGaps", frost_amount=1.0),
    # Shores: wet soil, darker; frozen in the Rimewood, corrupted round Blackmere
    "Mud": dict(
        base=[LAND + "WetSoil/T_WetSoil_A", LAND + "Mud/T_Mud_A"], normal=[LAND + "WetSoil/T_WetSoil_N", LAND + "Mud/T_Mud_N"],
        roughness=[LAND + "WetSoil/T_WetSoil_R", LAND + "Mud/T_Mud_R"], roughness_channel="R",
        height=[LAND + "WetSoil/T_WetSoil_D", LAND + "Mud/T_Mud_D"], tile=3.0, saturation=0.8, brightness=0.6, roughness_scale=0.7,
        frost_layer=("Snow", 0.75), blight_layer=("Corrupt", 0.5)),
    "Rock": dict(
        base=[RAMPAGE_ROCK + "T_RockTextureTile_000_D", LAND + "StoneyGround/T_StoneyGround_A"],
        normal=[RAMPAGE_ROCK + "T_RockTextureTile_000_N", LAND + "StoneyGround/T_StoneyGround_N"],
        roughness=[], roughness_channel=None, roughness_value=0.85, height=[],
        tile=7.0, triplanar=True, saturation=0.7, brightness=0.9, height_contrast=1.3, height_offset=0.1,
        frost="SnowOnTop", frost_amount=1.0, molten_layer=("Ash", 0.45), blight_layer=("Corrupt", 0.25)),
    "Snow": dict(
        base=[FAB_SNOW + "B"], normal=[FAB_SNOW + "N"], roughness=[FAB_SNOW + "ORM"], roughness_channel="G", height=[],
        tile=4.0, height_offset=0.05),
    "Ash": dict(
        base=[MINIONS_ENV + "T_GravelAdvanced_D", LAND + "DrySoil/T_DrySoil_A"],
        normal=[MINIONS_ENV + "T_GravelAdvanced_N", LAND + "DrySoil/T_DrySoil_N"],
        roughness=[], roughness_channel=None, roughness_value=0.92, height=[],
        tile=3.0, tint=(0.95, 0.9, 0.88), saturation=0.15, brightness=0.55,
        glow="Embers", glow_mask=[LAVA_FLOOR], glow_tile=7.0, glow_colour=(1.0, 0.3, 0.05), glow_strength=3.0, glow_sharpness=8.0),
    # No corrupted ground on hand: heavy mud turned violet-black and wet, with faint glowing veins
    "Corrupt": dict(
        base=[MEGA + "HeavyMud/T_HeavyMud_A"], normal=[MEGA + "HeavyMud/T_HeavyMud_N"], roughness=[MEGA + "HeavyMud/T_HeavyMud_R"],
        roughness_channel="R", height=[MEGA + "HeavyMud/T_HeavyMud_D"],
        tile=3.5, tint=(0.62, 0.48, 0.75), saturation=0.4, brightness=0.8, roughness_scale=0.75,
        glow="Veins", glow_mask=[NOISE_SOFT, NOISE_GROUND], glow_tile=14.0, glow_colour=(0.5, 0.12, 1.0), glow_strength=0.8,
        glow_sharpness=0.12),
}

# ---------------------------------------------------------------- shared
SETTINGS = dict(
    # The frost's paths: the trampled snow set
    trampled=dict(base=[FAB_TRAMPLED + "B"], normal=[FAB_TRAMPLED + "N"], roughness=[FAB_TRAMPLED + "ORM"], roughness_channel="G", tile=3.0),
    snow_layer="Snow",
    rock_snow_start=0.72, rock_snow_end=0.88, gap_snow_level=0.42,
    macro_noise=[NOISE_GROUND, NOISE_SOFT], macro_tile=45.0, macro_tile2=230.0, macro_strength=0.25,
    # The ground drifts between a drier and a lusher tint over the larger scale
    variation_a=(1.06, 1.0, 0.86), variation_b=(0.9, 1.04, 0.92),
    far_tile_scale=0.2, far_mix=0.55, far_blend_start=1500.0, far_blend_end=6000.0, far_normal_fade=0.5, glow_far_fade=0.85,
)

# Each region's grade over the blended ground (tint, saturation, brightness)
GRADES = {
    "Forest": dict(tint=(0.93, 1.05, 0.88), saturation=1.15, brightness=1.0),
    "Frost": dict(tint=(0.92, 0.97, 1.05), saturation=0.8, brightness=1.0),
    "Molten": dict(tint=(1.08, 0.9, 0.8), saturation=0.75, brightness=0.8),
    "Blight": dict(tint=(0.88, 0.8, 0.98), saturation=0.5, brightness=0.85),
}

# ---------------------------------------------------------------- grass (grown on a layer near the camera)
# Each type: the layer it grows on, how much in each region (Elderwood, Rimewood, Cinderlands, Blightwood) and its meshes.
# Each variety: (candidate meshes, density per 10 x 10 m, scale (min, max), cull start / end (m), shadow[, override]).
# A list of several meshes in one entry makes one variety per mesh found. Only static meshes; a mesh taller than
# MAX_COVER_HEIGHT (after scaling) is left for the trees-and-rocks pass. override: a material from MATERIAL_OVERRIDES.
MAX_COVER_HEIGHT = 1.5

PERSEPOLIS = "/Game/SICKA_PERSEPOLIS/Mesh/Foliage/"
PLANTS = "/Game/Megascans/3D_Plants/"
ASSETS = "/Game/Megascans/3D_Assets/"
FAB_3D = "/Game/Fab/Megascans/3D/"
GRASS_PATCHES = [PERSEPOLIS + "SM_Grasspatch_%d" % i for i in range(1, 9)]

# The Persepolis grass is tinted red in its pack; these instances (in the Grass folder) recolour it
MATERIAL_OVERRIDES = {
    "MI_Beyond_GrassGreen": dict(parent="/Game/SICKA_PERSEPOLIS/Materials/MI_GrassPatch", vectors={"Color Tint": (0.2, 0.42, 0.08)}),
    "MI_Beyond_GrassDead": dict(parent="/Game/SICKA_PERSEPOLIS/Materials/MI_GrassPatch", vectors={"Color Tint": (0.36, 0.3, 0.22)}),
    "MI_Beyond_GrassStraw": dict(parent="/Game/SICKA_PERSEPOLIS/Materials/MI_GrassPatch", vectors={"Color Tint": (0.62, 0.55, 0.38)}),
}

GRASS = {
    "Meadow": dict(layer="Grass", regions=(1.0, 0.0, 0.0, 0.0), varieties=[
        (GRASS_PATCHES, 14.0, (0.45, 0.75), (35, 50), False, "MI_Beyond_GrassGreen"),
        ([PLANTS + "KidneyWeed/SM_KidneyWeed_V4"], 5.0, (0.8, 1.2), (30, 45), False),
        ([PLANTS + "GroundCoverA/SM_GroundCover_02", PLANTS + "GroundCoverA/SM_GroundCover_03"], 8.0, (0.8, 1.3), (30, 45), False),
    ]),
    "ForestFloor": dict(layer="ForestGround", regions=(1.0, 0.0, 0.0, 0.3), varieties=[
        ([PLANTS + "FernB/SM_FernB_%02d" % i for i in range(5)], 3.0, (0.7, 1.2), (50, 70), True),
        ([PLANTS + "GroundCoverA/SM_GroundCover_%02d" % i for i in (4, 6, 8)], 8.0, (0.8, 1.2), (35, 50), False),
        (GRASS_PATCHES[:4], 3.0, (0.4, 0.6), (35, 50), False, "MI_Beyond_GrassGreen"),
        ([PLANTS + "DeadBranches/SM_DeadBranches_%02d" % i for i in range(3)], 2.0, (0.8, 1.1), (30, 45), False),
        ([ASSETS + "Branch/SM_Branch_01", ASSETS + "WoodBranchA/SM_WoodBranchA", ASSETS + "WoodBranchB/SM_WoodBranchB",
          ASSETS + "WoodBranchC/SM_WoodBranchC"], 1.0, (0.8, 1.1), (35, 50), True),
        ([ASSETS + "MossyRocksA/SM_MossyRocksA"], 0.3, (0.25, 0.5), (60, 80), True),
    ]),
    "Moss": dict(layer="Moss", regions=(1.0, 0.0, 0.0, 0.0), varieties=[
        ([ASSETS + "IcelandicMossClusters/SM_IcelandicMossClusters_%02d" % i for i in (0, 1, 2, 4, 5, 6, 7, 8)], 2.0, (0.4, 0.8), (35, 50), False),
    ]),
    "Blight": dict(layer="Corrupt", regions=(0.0, 0.0, 0.0, 1.0), varieties=[
        ([PLANTS + "DeadHerbWeed/SM_DeadHerbWeed"], 10.0, (0.8, 1.3), (35, 50), False),
        (GRASS_PATCHES, 6.0, (0.4, 0.7), (35, 50), False, "MI_Beyond_GrassDead"),
        ([PLANTS + "DeadBroomBush/SM_DeadBroomBush_V%d" % i for i in (6, 7, 8, 9)], 2.0, (0.7, 1.1), (50, 70), True),
        ([PLANTS + "DryPlant/SM_DryPlant00_V5", PLANTS + "DryPlant/SM_DryPlant00_V6"], 5.0, (0.8, 1.2), (35, 50), False),
        ([PLANTS + "DeadBranches/SM_DeadBranches_%02d" % i for i in (3, 4, 5)], 3.0, (0.8, 1.2), (35, 50), False),
    ]),
    "Frost": dict(layer="Snow", regions=(0.0, 1.0, 0.0, 0.0), varieties=[
        ([FAB_3D + "Snow_Clump_xe0nfaoqx/Medium/xe0nfaoqx_tier_2/StaticMeshes/xe0nfaoqx_tier_2"], 2.0, (1.5, 3.0), (50, 70), True),
        (GRASS_PATCHES[4:], 2.0, (0.35, 0.55), (35, 50), False, "MI_Beyond_GrassStraw"),
        ([PLANTS + "DryPlantSetA/SM_DryPlantSet_V%d" % i for i in (1, 2, 3)], 1.5, (0.7, 1.0), (35, 50), False),
    ]),
    "Ash": dict(layer="Ash", regions=(0.0, 0.0, 1.0, 0.0), varieties=[
        ([PLANTS + "DeadBranches/SM_DeadBranches_%02d" % i for i in (0, 4)], 1.5, (0.8, 1.1), (35, 50), False),
        ([PLANTS + "DryPlantSetA/SM_DryPlantSet_V6"], 1.0, (0.7, 1.0), (35, 50), False),
    ]),
}

# ---------------------------------------------------------------- water, ice, lava
# Lakes from world_content.LAYOUT["lakes"]: water (an instance of M_Beyond_Water) or ice (M_Beyond_Ice, walkable)
WATER = {
    "mirrormere": "MI_Water_Elderwood",
    "reedpool": "MI_Water_Elderwood",
    "blackmere": "MI_Water_Blight",
    "glassmere": "ice",
}
# Rivers from world_content.LAYOUT["rivers"]: the water instance on their strips
RIVER_WATER = {
    "elder_run": "MI_Water_Elderwood",
    "rime_melt": "MI_Water_Elderwood",
    "blight_channel": "MI_Water_Blight",
}
# Water instances: scattering and absorption (per metre, r g b), the surface's roughness
WATER_LOOKS = {
    "MI_Water_Elderwood": dict(scattering=(0.004, 0.014, 0.013), absorption=(0.45, 0.12, 0.09), roughness=0.03),
    "MI_Water_Blight": dict(scattering=(0.02, 0.008, 0.03), absorption=(0.55, 0.6, 0.35), roughness=0.06),
}
# Ice and lava (material parameters)
ICE_LOOK = dict(colour=(0.22, 0.38, 0.5), roughness=0.06, dust_level=0.68)
LAVA_LOOK = dict(hot=(1.0, 0.22, 0.03), strength=3.5, boost=4.0, crust_level=0.5)
WATER_MARGIN = 8.0        # m beyond the lake's radius the surface reaches (the shore hides its edge)
DEEP_BELOW_SURFACE = 0.6  # m: deep water starts this far under the surface (a demigod wades where it's shallower)

# The caldera's lava: a little under Kael'thar's arena floor, so it shows in the crater floor's low spots and the arena
# stays dry; the disc stays inside the crater wall and out of the breach
LAVA = dict(arena="molten_heart", below_arena=0.8, radius_share=0.75, burn_above=0.3, burn_per_second=20.0)

# ---------------------------------------------------------------- pictures (Saved/WorldBuilder/ground/)
# name, target (a region id, ("lake", id), ("volcano",) or (m east, m north)), from (direction east / north)
VIEWS = [
    ("mossbrook", "village_mossbrook", (0.6, -0.8)),
    ("elderwood_meadow", (-700, -1000), (0.7, 0.7)),
    ("mirrormere_shore", ("lake", "mirrormere"), (-0.8, 0.6)),
    ("northgate_cliffs", (-200, 380), (0.2, -1.0)),
    ("duskwatch", "village_duskwatch", (-0.7, -0.7)),
    ("blackmere", ("lake", "blackmere"), (-0.7, 0.7)),
    ("frostholm", "village_frostholm", (0.7, -0.7)),
    ("glassmere", ("lake", "glassmere"), (-0.7, -0.7)),
    ("cinderhold", "village_cinderhold", (-0.6, -0.8)),
    ("caldera", ("volcano",), (0.7, -0.7)),
]
