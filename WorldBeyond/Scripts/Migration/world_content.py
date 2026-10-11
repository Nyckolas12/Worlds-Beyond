"""
Worlds Beyond - Plan 5 open-world content (read by migrate_pass14.py and the later world passes). Placeholder names and
numbers, in theme; edit freely. Re-running pass 14 only creates what is missing (BEYOND_REBUILD_REGIONS=1 writes the
region definitions again, BEYOND_REBUILD_ROSTER=1 the frost / molten enemies).

The world is "the Wandering Dominion", about 4 x 4 km. Positions are metres east (x) and north (y) of the centre;
Unreal's X is east and Y is south, so the passes turn (x, y) into (x * 100, -y * 100).

Geography from the dialogue: Mossbrook in the middle of the forest; Gorehide's den south-west past the standing stones;
raiders in the western forest; the corrupted "eastern woods" with empty villages (Veyla); a molten demon (Kael'thar) to
the north-west; a troll king in the frost (Hrimgar) to the north-east; "don't go north until you're stronger".
"""

WORLD_NAME = "The Wandering Dominion"

# ---------------------------------------------------------------- sound (placeholders from the village sample)
MUSIC = "/Game/Audio/_Content/Music/wav/"
MUSIC_FOREST = MUSIC + "CallAndResponse/ForestMeadow/mx_ForestMeadow_MelodyTheme_Layer2_4-4_80bpm_64bar+3tail_l"
MUSIC_FROST = MUSIC + "CallAndResponse/ForestMeadow/mx_ForestMeadow_SustainAtonal_Layer1_4-4_80bpm_64bar+3tail_l"
MUSIC_VILLAGE = MUSIC + "CallAndResponse/Village/Village_Split/mx_Village_PadsSolo_Layer1_MEL_4-4_80bpm_17bar+1tail_l"
MUSIC_BLIGHT = MUSIC + "CallAndResponse/Village/Village_Split/mx_Village_PadsSolo_Layer1_DIS_4-4_80bpm_59bar+2tails_l"
MUSIC_MOLTEN = MUSIC + "CallAndResponse/Village/Village_Split/mx_Village_Perc_Layer3_DIS_4-4_80bpm_59bar+2tails_l"
# The music waves get Looping on (they are the sample's loop stems)
LOOPING_MUSIC = [MUSIC_FOREST, MUSIC_FROST, MUSIC_VILLAGE, MUSIC_BLIGHT, MUSIC_MOLTEN]

SFX = "/Game/Audio/_Content/SFX/"
AMB_BIRDS = SFX + "Amb_Misc/Birds/sfx_Ambient_Twilight_Birds_pgfx_meta"
AMB_CRICKETS = SFX + "Amb_Misc/Insects/sfx_Ambient_Cornfield_Insect_Cricket_Long_meta"
AMB_WIND = SFX + "Amb_Misc/Wind/sfx_Ambient_Forest_WindTone_pgfx_meta"
AMB_FIRE = SFX + "Fire/sfx_Ambient_Fire_Burn_Large_Dist_l_meta"

EMBERS = "/Game/Effects/Embers/NS_EmbersLarge"

# ---------------------------------------------------------------- weather presets
# sun lux / colour, sky light intensity / colour, fog density / falloff / colour, saturation, tint, exposure, precipitation
WEATHER = {
    "forest": dict(sun=9.0, sun_colour=(1.0, 0.95, 0.86), sky=1.0, sky_colour=(1.0, 1.0, 1.0),
                   fog=0.02, falloff=0.2, fog_colour=(0.45, 0.56, 0.5), saturation=1.05, tint=(1.0, 1.0, 0.97), exposure=0.0),
    "blight": dict(sun=4.5, sun_colour=(0.8, 0.75, 0.95), sky=0.7, sky_colour=(0.75, 0.7, 0.9),
                   fog=0.065, falloff=0.12, fog_colour=(0.32, 0.28, 0.4), saturation=0.6, tint=(0.92, 0.88, 1.0), exposure=-0.4),
    "frost": dict(sun=7.0, sun_colour=(0.86, 0.92, 1.0), sky=1.1, sky_colour=(0.85, 0.92, 1.0),
                  fog=0.035, falloff=0.15, fog_colour=(0.7, 0.78, 0.9), saturation=0.85, tint=(0.94, 0.97, 1.05), exposure=0.1),
    "molten": dict(sun=6.0, sun_colour=(1.0, 0.72, 0.5), sky=0.85, sky_colour=(1.0, 0.8, 0.7),
                   fog=0.05, falloff=0.1, fog_colour=(0.55, 0.32, 0.2), saturation=1.1, tint=(1.08, 0.95, 0.86), exposure=-0.15,
                   precipitation=EMBERS),
    "crossroads": dict(sun=8.0, sun_colour=(1.0, 0.93, 0.84), sky=1.0, sky_colour=(1.0, 1.0, 1.0),
                       fog=0.025, falloff=0.18, fog_colour=(0.5, 0.55, 0.6), saturation=1.0, tint=(1.0, 1.0, 1.0), exposure=0.0),
}

# ---------------------------------------------------------------- region borders (m), shared by neighbours and followed by
# the ridges between them, so the land and the region volumes agree
# The Elderwood | the Blightwood, south to north (the Willowmere Gap at (735, -600))
BORDER_FOREST_BLIGHT = [(750, -1950), (690, -1500), (800, -1100), (720, -700), (780, -300), (700, 100), (760, 420)]
# The Elderwood | the north, west to east (the Northgate Pass at (-200, 450))
BORDER_NORTH = [(-1950, 450), (-1500, 520), (-1000, 420), (-600, 500), (-200, 450), (0, 419), (250, 380), (760, 420)]
# The Cinderlands | the Rimewood Reach (the Greyspine), south to north; Kingsfork (0, 650) stays east of it
BORDER_FIRE_FROST = [(0, 419), (-50, 650), (80, 850), (-60, 1250), (60, 1600), (-30, 1950)]
# The Blightwood | the Rimewood Reach, west to east (the Rimegate at (1300, 400))
BORDER_BLIGHT_FROST = [(760, 420), (1100, 330), (1300, 400), (1600, 470), (1950, 380)]

# ---------------------------------------------------------------- regions, villages, areas
# id: name, subtitle, kind, parent, tag, (level min, max), banter context (None: the id), music, ambience, weather key
#     (villages: None keeps the region's), map colour, centre (m), radius (m) or polygon [(x, y)...] (m)
REGIONS = {
    # The start forest, Mossbrook in its middle; raiders west, wolves south-west
    "region_forest": dict(name="The Elderwood", subtitle=WORLD_NAME, kind="Region", parent=None, tag="Region.Forest",
                          band=(1, 6), banter="region_forest", music=MUSIC_FOREST, ambience=AMB_CRICKETS, weather="forest",
                          colour=(0.38, 0.62, 0.32), centre=(-650, -800),
                          polygon=[(-1950, -1950), (750, -1950)] + BORDER_FOREST_BLIGHT[1:] + list(reversed(BORDER_NORTH))[1:]),
    # The corrupted eastern woods: empty villages, Veyla
    "region_corrupted_woods": dict(name="The Blightwood", subtitle=WORLD_NAME, kind="Region", parent=None, tag="Region.CorruptedWoods",
                                   band=(6, 10), banter="region_corrupted_woods", music=MUSIC_BLIGHT, ambience=AMB_WIND,
                                   weather="blight", colour=(0.5, 0.35, 0.62), centre=(1380, -800),
                                   polygon=[(1950, -1950), (1950, 380)] + list(reversed(BORDER_BLIGHT_FROST))[1:]
                                   + list(reversed(BORDER_FOREST_BLIGHT))[1:]),
    # The frozen north-east: Hrimgar
    "region_frost": dict(name="The Rimewood Reach", subtitle=WORLD_NAME, kind="Region", parent=None, tag="Region.Frost",
                         band=(10, 13), banter="region_frost", music=MUSIC_FROST, ambience=AMB_WIND, weather="frost",
                         colour=(0.62, 0.78, 0.92), centre=(1000, 1200),
                         polygon=BORDER_BLIGHT_FROST + [(1950, 1950)] + list(reversed(BORDER_FIRE_FROST))
                         + [(250, 380)]),
    # The volcanic north-west: Kael'thar
    "region_molten": dict(name="The Cinderlands", subtitle=WORLD_NAME, kind="Region", parent=None, tag="Region.Molten",
                          band=(13, 16), banter="region_molten", music=MUSIC_MOLTEN, ambience=AMB_FIRE, weather="molten",
                          colour=(0.85, 0.38, 0.18), centre=(-1000, 1200),
                          polygon=BORDER_NORTH[:5] + BORDER_FIRE_FROST + [(-1950, 1950)]),

    # The crossroads north of the gate, between the frost and the fire
    "area_kingsfork": dict(name="Kingsfork Crossroads", subtitle=None, kind="Area", parent="region_frost", tag="Region.Frost",
                           band=(9, 11), banter="area_kingsfork", music=None, ambience=None, weather="crossroads",
                           colour=(0.9, 0.85, 0.6), centre=(0, 650), radius=110),

    # Elderwood villages (Mossbrook keeps Plan 4's region_village banter)
    "village_mossbrook": dict(name="Mossbrook", subtitle=None, kind="Village", parent="region_forest", tag="Region.Forest",
                              band=(1, 3), banter="region_village", music=MUSIC_VILLAGE, ambience=AMB_BIRDS, weather=None,
                              colour=(1.0, 0.82, 0.45), centre=(-250, -800), radius=95),
    "village_fernhollow": dict(name="Fernhollow", subtitle=None, kind="Village", parent="region_forest", tag="Region.Forest",
                               band=(2, 4), banter="village_fernhollow", music=MUSIC_VILLAGE, ambience=AMB_BIRDS, weather=None,
                               colour=(1.0, 0.82, 0.45), centre=(-1150, -250), radius=60),
    "village_brackenford": dict(name="Brackenford", subtitle=None, kind="Village", parent="region_forest", tag="Region.Forest",
                                band=(1, 3), banter="village_brackenford", music=MUSIC_VILLAGE, ambience=AMB_BIRDS, weather=None,
                                colour=(1.0, 0.82, 0.45), centre=(300, -1500), radius=60),
    "village_willowmere": dict(name="Willowmere", subtitle=None, kind="Village", parent="region_forest", tag="Region.Forest",
                               band=(4, 6), banter="village_willowmere", music=MUSIC_VILLAGE, ambience=AMB_BIRDS, weather=None,
                               colour=(1.0, 0.82, 0.45), centre=(450, -300), radius=55),

    # Blightwood: one held outpost, three emptied villages
    "village_duskwatch": dict(name="Duskwatch", subtitle=None, kind="Village", parent="region_corrupted_woods",
                              tag="Region.CorruptedWoods", band=(6, 8), banter="village_duskwatch", music=None, ambience=None,
                              weather=None, colour=(0.95, 0.75, 0.5), centre=(1050, -650), radius=75),
    "village_greyhallow": dict(name="Greyhallow (abandoned)", subtitle=None, kind="Village", parent="region_corrupted_woods",
                               tag="Region.CorruptedWoods", band=(7, 9), banter="village_empty", music=None, ambience=None,
                               weather=None, colour=(0.55, 0.5, 0.6), centre=(1500, -1250), radius=60),
    "village_marrowfield": dict(name="Marrowfield (abandoned)", subtitle=None, kind="Village", parent="region_corrupted_woods",
                                tag="Region.CorruptedWoods", band=(8, 10), banter="village_empty", music=None, ambience=None,
                                weather=None, colour=(0.55, 0.5, 0.6), centre=(1700, -300), radius=60),
    "village_sallowend": dict(name="Sallow End (abandoned)", subtitle=None, kind="Village", parent="region_corrupted_woods",
                              tag="Region.CorruptedWoods", band=(7, 9), banter="village_empty", music=None, ambience=None,
                              weather=None, colour=(0.55, 0.5, 0.6), centre=(1250, 150), radius=55),

    # Rimewood Reach
    "village_frostholm": dict(name="Frostholm", subtitle=None, kind="Village", parent="region_frost", tag="Region.Frost",
                              band=(10, 11), banter="village_frostholm", music=MUSIC_VILLAGE, ambience=AMB_WIND, weather=None,
                              colour=(0.85, 0.92, 1.0), centre=(850, 950), radius=85),
    "village_pinecrest": dict(name="Pinecrest", subtitle=None, kind="Village", parent="region_frost", tag="Region.Frost",
                              band=(11, 12), banter="village_pinecrest", music=MUSIC_VILLAGE, ambience=AMB_WIND, weather=None,
                              colour=(0.85, 0.92, 1.0), centre=(350, 1350), radius=55),
    "village_hearthfall": dict(name="Hearthfall", subtitle=None, kind="Village", parent="region_frost", tag="Region.Frost",
                               band=(11, 13), banter="village_hearthfall", music=MUSIC_VILLAGE, ambience=AMB_WIND, weather=None,
                               colour=(0.85, 0.92, 1.0), centre=(1500, 800), radius=55),

    # Cinderlands
    "village_cinderhold": dict(name="Cinderhold", subtitle=None, kind="Village", parent="region_molten", tag="Region.Molten",
                               band=(13, 14), banter="village_cinderhold", music=None, ambience=AMB_FIRE, weather=None,
                               colour=(1.0, 0.7, 0.45), centre=(-750, 850), radius=85),
    "village_slagford": dict(name="Slagford", subtitle=None, kind="Village", parent="region_molten", tag="Region.Molten",
                             band=(14, 15), banter="village_slagford", music=None, ambience=AMB_FIRE, weather=None,
                             colour=(1.0, 0.7, 0.45), centre=(-1500, 650), radius=55),
    "village_emberrest": dict(name="Emberrest", subtitle=None, kind="Village", parent="region_molten", tag="Region.Molten",
                              band=(14, 16), banter="village_emberrest", music=None, ambience=AMB_FIRE, weather=None,
                              colour=(1.0, 0.7, 0.45), centre=(-450, 1500), radius=55),
}

PRIORITY = {"Region": 0, "Area": 5, "Village": 10}

# ---------------------------------------------------------------- frost and molten enemies
# New roster entries: copies of the Plan 3 enemies with a new id, name, region, tint and sturdier stats.
# (asset name, copy of, enemy id, display name, region tag, tint (r, g, b, strength), health x, description)
ROSTER_VARIANTS = [
    ("DA_Enemy_FrostWolf", "DA_Enemy_TimberWolf", "frost_wolf", "Frost Wolf", "Region.Frost", (0.7, 0.88, 1.0, 0.55), 1.3,
     "A white-furred hunter of the Rimewood; packs of three."),
    ("DA_Enemy_FrostReaver", "DA_Enemy_Raider", "frost_reaver", "Frost Reaver", "Region.Frost", (0.55, 0.75, 1.0, 0.45), 1.3,
     "A raider gone north, wrapped in furs and rime."),
    ("DA_Enemy_RimeSlinger", "DA_Enemy_RaiderSlinger", "frost_slinger", "Rime Slinger", "Region.Frost", (0.6, 0.82, 1.0, 0.45), 1.3,
     "Throws shards of ice from the treeline."),
    ("DA_Enemy_RimeTreant", "DA_Enemy_Treant", "frost_treant", "Rime Treant", "Region.Frost", (0.78, 0.9, 1.0, 0.6), 1.3,
     "An old spruce-guardian frozen to the core."),
    ("DA_Enemy_CinderWraith", "DA_Enemy_HollowWraith", "cinder_wraith", "Cinder Wraith", "Region.Molten", (1.0, 0.42, 0.1, 0.6), 1.35,
     "A drifting husk of ash and heat."),
    ("DA_Enemy_AshenKnight", "DA_Enemy_CursedKnight", "ashen_knight", "Ashen Knight", "Region.Molten", (0.35, 0.28, 0.25, 0.65), 1.35,
     "Kael'thar's sworn guard, armour fused by fire."),
    ("DA_Enemy_MagmaBeast", "DA_Enemy_CorruptedBeast", "magma_beast", "Magma Beast", "Region.Molten", (1.0, 0.35, 0.05, 0.6), 1.35,
     "Spits molten rock that pools and burns."),
    ("DA_Enemy_CinderSlinger", "DA_Enemy_RaiderSlinger", "cinder_slinger", "Cinder Slinger", "Region.Molten", (1.0, 0.48, 0.15, 0.5), 1.35,
     "A miner turned bandit, hurling embers."),
]

# The bosses' real homes (pass 11 tagged Kael'thar and Hrimgar with the Plan 3 roster regions)
BOSS_REGIONS = {
    "DA_Boss_Kaelthar": "Region.Molten",
    "DA_Boss_Hrimgar": "Region.Frost",
}

# ================================================================ the world's shape (Plan 5B; DA_WorldLayout)
# Terrain style per region: style, base height (m), hill height (m), hill width (m)
TERRAIN = {
    "region_forest": ("Forest", 30, 22, 320),
    "region_corrupted_woods": ("Blight", 24, 16, 220),
    "region_frost": ("Frost", 60, 42, 380),
    "region_molten": ("Molten", 45, 34, 300),
}

LAYOUT = dict(
    seed=1977, components=32, sections=2, quads=63, height_scale_z=200.0,
    border_width=220.0, border_height=300.0, region_blend=180.0, border_warp=45.0, border_warp_scale=380.0,
    # Long rises between regions; passes cut through them: points, height, width, passes, pass width
    ridges=[
        # Elderwood | Blightwood, the Willowmere Gap
        (BORDER_FOREST_BLIGHT, 70, 160, [(735, -600)], 110),
        # Elderwood | the north, the Northgate Pass ("don't go north until you're stronger")
        (BORDER_NORTH, 90, 170, [(-200, 450)], 120),
        # The Greyspine between the frost and the fire (from north of Kingsfork)
        (BORDER_FIRE_FROST[2:], 230, 260, [], 100),
        # Blightwood | Rimewood, the Rimegate
        (BORDER_BLIGHT_FROST, 70, 140, [(1300, 400)], 100),
    ],
    # Peaks: centre, radius, height, sharpness
    peaks=[
        ((1700, 1750), 500, 300, 1.6),
        ((1850, 1100), 350, 200, 1.5),
        ((300, 1850), 300, 170, 1.6),
        ((-1850, 1850), 350, 190, 1.5),
        ((-1750, -1850), 260, 110, 1.4),
    ],
    volcano=dict(centre=(-1250, 1350), radius=550, rim_radius=280, rim_height=260, floor_height=140, lava_level=134,
                 breach_bearing=135, breach_half_angle=14),
    # Lakes: id, centre, radius, depth, frozen
    lakes=[
        ("mirrormere", (350, -1200), 220, 9, False),
        ("reedpool", (-650, -150), 90, 5, False),
        ("blackmere", (1400, -800), 150, 7, False),
        ("glassmere", (1100, 1350), 180, 6, True),
    ],
    # Roads: id, points, width (m), cobble
    roads=[
        ("kings_road", [(300, -1500), (100, -1150), (-250, -800), (-250, -300), (-200, 450), (0, 650)], 10, True),
        ("road_northwest", [(0, 650), (-350, 780), (-750, 850), (-1000, 1000), (-1060, 1120)], 8, False),
        ("road_northeast", [(0, 650), (400, 800), (850, 950), (1150, 880), (1500, 800)], 8, False),
        ("road_east", [(-250, -800), (100, -650), (450, -300), (735, -600), (1050, -650), (1300, -950), (1500, -1250)], 8, False),
        ("path_fernhollow", [(-250, -800), (-700, -500), (-1150, -250)], 6, False),
        ("path_stones", [(-250, -800), (-700, -1150), (-1100, -1450), (-1400, -1700)], 6, False),
        ("road_rimegate", [(1050, -650), (1250, -100), (1250, 150), (1300, 400), (1500, 800)], 7, False),
        ("path_pinecrest", [(850, 950), (600, 1150), (350, 1350)], 6, False),
        ("path_emberrest", [(-750, 850), (-600, 1200), (-450, 1500)], 6, False),
        ("path_slagford", [(-750, 850), (-1100, 750), (-1500, 650)], 6, False),
        ("path_marrowfield", [(1050, -650), (1400, -500), (1700, -300)], 6, False),
        ("path_crown", [(1500, 800), (1600, 1150), (1650, 1550)], 6, False),
    ],
)

# ================================================================ what goes in the world (Plan 5B / 5C)
# Waystones: id, name, (x, y) m, facing (degrees, 0 = east, 90 = north), start attuned
WAYSTONES = [
    ("mossbrook", "Mossbrook Waystone", (-250, -720), 90, True),
    ("fernhollow", "Fernhollow Waystone", (-1110, -250), 0, False),
    ("brackenford", "Brackenford Waystone", (330, -1450), 90, False),
    ("willowmere", "Willowmere Waystone", (420, -260), 0, False),
    ("standing_stones", "Standing Stones Waystone", (-1120, -1380), 45, False),
    ("northgate", "Northgate Waystone", (-200, 380), 90, False),
    ("duskwatch", "Duskwatch Waystone", (1110, -650), 180, False),
    ("greyhallow", "Greyhallow Waystone", (1460, -1200), 135, False),
    ("marrowfield", "Marrowfield Waystone", (1650, -330), 180, False),
    ("sallow_end", "Sallow End Waystone", (1230, 110), 90, False),
    ("weeping_grove", "Weeping Grove Waystone", (1600, -1560), 225, False),
    ("frostholm", "Frostholm Waystone", (900, 960), 0, False),
    ("pinecrest", "Pinecrest Waystone", (330, 1300), 90, False),
    ("hearthfall", "Hearthfall Waystone", (1450, 830), 180, False),
    ("frozen_crown", "Frozen Crown Waystone", (1560, 1520), 45, False),
    ("wolfjaw", "Wolfjaw Waystone", (520, 1720), 90, False),
    ("cinderhold", "Cinderhold Waystone", (-700, 850), 180, False),
    ("slagford", "Slagford Waystone", (-1460, 650), 0, False),
    ("emberrest", "Emberrest Waystone", (-420, 1460), 90, False),
    ("molten_heart", "Caldera Breach Waystone", (-1020, 1080), 135, False),
    ("obsidian_rift", "Obsidian Rift Waystone", (-1720, 1060), 0, False),
    ("kingsfork", "Kingsfork Waystone", (40, 650), 90, False),
]

# Boss arenas: id, name, (x, y) m, boss definition (None: an empty site for a later boss), level, arena radius (uu)
ARENAS = [
    ("gorehide_den", "Gorehide's Den", (-1400, -1700), "DA_Boss_Gorehide", 5, 2000),
    ("old_quarry", "The Old Quarry", (650, -1850), None, 6, 2000),
    ("heartroot_glade", "Heartroot Glade", (-1650, -600), None, 6, 2600),
    ("weeping_grove", "The Weeping Grove", (1750, -1700), "DA_Boss_Veyla", 9, 2000),
    ("witchlight_bog", "Witchlight Bog", (1000, -1600), None, 9, 2000),
    ("hollow_throne", "The Hollow Throne", (1850, 250), None, 10, 2600),
    ("frozen_crown", "The Frozen Crown", (1650, 1700), "DA_Boss_Hrimgar", 12, 2600),
    ("wolfjaw_pass", "Wolfjaw Pass", (500, 1800), None, 12, 2000),
    ("molten_heart", "The Molten Heart", (-1300, 1400), "DA_Boss_Kaelthar", 15, 2600),
    ("obsidian_rift", "The Obsidian Rift", (-1800, 1100), None, 15, 2000),
]

# Places: id, name, kind (Landmark / Cave / Shrine / Ruins / Vista / Lake / Camp), (x, y) m, discovery radius (m)
PLACES = [
    ("standing_stones", "Standing Stones of Brannoch", "Landmark", (-1100, -1450), 40),
    ("hollowroot_cave", "Hollowroot Cave", "Cave", (-900, -1100), 30),
    ("mirrormere", "Mirrormere", "Lake", (350, -1200), 70),
    ("old_watchtower", "The Old Watchtower", "Ruins", (-300, 300), 35),
    ("reedpool", "Reedpool", "Lake", (-650, -150), 45),
    ("blackmere", "Blackmere", "Lake", (1400, -800), 60),
    ("rotroot_hollow", "Rotroot Hollow", "Cave", (1550, -1000), 30),
    ("chapel_of_ash", "The Chapel of Ash", "Ruins", (1150, -1300), 35),
    ("glassmere", "Glassmere", "Lake", (1100, 1350), 60),
    ("rime_grotto", "The Rime Grotto", "Cave", (1300, 1650), 30),
    ("skalds_cairn", "Skald's Cairn", "Shrine", (700, 1550), 30),
    ("northwatch", "Northwatch", "Vista", (1800, 1900), 40),
    ("caldera_breach", "The Caldera Breach", "Landmark", (-1000, 1100), 40),
    ("ashen_tube", "The Ashen Tube", "Cave", (-1550, 1600), 30),
    ("slag_mines", "The Slag Mines", "Ruins", (-1650, 800), 35),
    ("shrine_of_the_pyre", "Shrine of the Pyre", "Shrine", (-600, 1250), 30),
    ("emberfall", "Emberfall", "Vista", (-300, 1800), 40),
]

# Enemy camps: id, (x, y) m, [(enemy id, count)], level offset in the region band, respawn rule
CAMPS = [
    # Elderwood
    ("forest_raiders_west", (-1500, -350), [("forest_raider", 2), ("forest_slinger", 1)], 2, "OnRest"),
    ("forest_raiders_ford", (-1300, -900), [("forest_raider", 2)], 1, "OnRest"),
    ("forest_raiders_south", (-1700, -1150), [("forest_raider", 1), ("forest_slinger", 1)], 3, "OnRest"),
    ("forest_wolves_stones", (-900, -1500), [("forest_wolf", 3)], 2, "OnRest"),
    ("forest_wolves_lake", (200, -1050), [("forest_wolf", 3)], 0, "OnRest"),
    ("forest_treant_south", (-600, -1700), [("forest_treant", 1)], 2, "OnRest"),
    ("forest_treant_west", (-1050, -650), [("forest_treant", 1)], 1, "OnRest"),
    ("forest_wolves_gap", (600, -750), [("forest_wolf", 3)], 4, "OnRest"),
    # Blightwood
    ("woods_wraiths_road", (1300, -480), [("woods_wraith", 2)], 0, "OnRest"),
    ("woods_greyhallow", (1520, -1260), [("woods_beast", 2), ("woods_wraith", 1)], 1, "OnRest"),
    ("woods_marrowfield", (1720, -300), [("woods_knight", 1), ("woods_wraith", 2)], 2, "OnRest"),
    ("woods_sallow_end", (1260, 160), [("woods_beast", 2)], 1, "OnRest"),
    ("woods_wraiths_bog", (900, -1300), [("woods_wraith", 2)], 2, "OnRest"),
    ("woods_knight_grove", (1450, -1500), [("woods_knight", 1)], 3, "OnRest"),
    ("woods_beasts_east", (1800, -900), [("woods_beast", 2)], 2, "OnRest"),
    ("woods_mixed_north", (1050, -150), [("woods_wraith", 1), ("woods_beast", 1)], 1, "OnRest"),
    # Rimewood Reach
    ("frost_wolves_west", (600, 1050), [("frost_wolf", 3)], 0, "OnRest"),
    ("frost_reavers_road", (1200, 1100), [("frost_reaver", 2), ("frost_slinger", 1)], 1, "OnRest"),
    ("frost_treant_pine", (420, 1560), [("frost_treant", 1)], 2, "OnRest"),
    ("frost_wolves_east", (1700, 1200), [("frost_wolf", 3)], 2, "OnRest"),
    ("frost_reavers_north", (1100, 1720), [("frost_reaver", 2)], 2, "OnRest"),
    ("frost_slingers_lake", (800, 1380), [("frost_slinger", 2)], 1, "OnRest"),
    ("frost_treant_gate", (1600, 600), [("frost_treant", 1)], 1, "OnRest"),
    # Cinderlands
    ("cinder_slingers_road", (-900, 720), [("cinder_slinger", 2)], 0, "OnRest"),
    ("cinder_beasts_flats", (-1300, 820), [("magma_beast", 2)], 1, "OnRest"),
    ("cinder_knight_rift", (-1620, 1300), [("ashen_knight", 1)], 2, "OnRest"),
    ("cinder_wraiths_ember", (-700, 1620), [("cinder_wraith", 2)], 2, "OnRest"),
    ("cinder_mixed_shrine", (-420, 1100), [("magma_beast", 1), ("cinder_wraith", 1)], 1, "OnRest"),
    ("cinder_slag", (-1800, 500), [("cinder_slinger", 1), ("magma_beast", 1)], 2, "OnRest"),
    ("cinder_knight_north", (-1000, 1720), [("ashen_knight", 1), ("cinder_wraith", 1)], 3, "OnRest"),
]

# Where the party starts (x, y) m and facing (degrees, 0 = east, 90 = north)
PLAYER_START = ((-250, -780), 90)
