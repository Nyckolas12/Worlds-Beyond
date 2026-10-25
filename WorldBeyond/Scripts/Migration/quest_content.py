"""
Worlds Beyond - the quests (Plan 6), read by migrate_pass23.py into /Game/WorldsBeyond/Quests/DA_QuestDatabase.

Objectives (see EBeyondObjectiveType): ("flag", "<story flag>", text), ("kill", "<enemy id or None>", count, text),
("item", "DA_Item_<X>", count, text), ("level", count, text), ("waystones", count, text), ("bond", count, text),
("quest", "<quest id>", text), ("talk", "<NPC id>", text).
Flags the world sets: Discovered.<region / village / place / waystone id>, Waystone.<id>, Boss.<boss id>,
Talked.<NPC id>.<row>; the ids are world_content.py's, the NPCs dialogue_content.py's, the enemies the roster's.
Rewards: exp, gold, crystals, shards, fragments, bond (Bond EXP), items (DA_Item_<X> names), tier.
Thumbnails: T_Quest_<name> pictures in /Game/WorldsBeyond/UI/Quests (made by pass 21).
"""

MAIN = [
    {
        "id": "ch1_mossbrook", "chapter": 1, "title": "Storm over Mossbrook", "location": "Mossbrook",
        "summary": "Something howls in the woods around Mossbrook. Find the village and hear what Elder Maren has to say.",
        "thumbnail": "Chapter1",
        "objectives": [
            ("flag", "Discovered.village_mossbrook", "Reach Mossbrook"),
            ("talk", "elder_maren", "Speak with Elder Maren"),
        ],
        "reward": {"exp": 150, "gold": 200, "crystals": 2},
    },
    {
        "id": "ch2_pale_alpha", "chapter": 2, "title": "The Pale Alpha", "location": "The Verdant Wilds",
        "summary": "A great pale wolf leads the forest's packs. The standing stones remember where it hunts.",
        "thumbnail": "Chapter2", "requires": ["ch1_mossbrook"],
        "objectives": [
            ("flag", "Discovered.standing_stones", "Find the Standing Stones"),
            ("flag", "Boss.gorehide", "Defeat Gorehide, the Pale Alpha"),
        ],
        "reward": {"exp": 600, "gold": 500, "crystals": 5, "fragments": 1, "bond": 20},
    },
    {
        "id": "ch3_hollow_voice", "chapter": 3, "title": "The Hollow Voice", "location": "The Corrupted Woods",
        "summary": "A voice calls from the corrupted woods, and the blight answers it. Follow it to its source.",
        "thumbnail": "Chapter3", "requires": ["ch2_pale_alpha"],
        "objectives": [
            ("flag", "Discovered.region_corrupted_woods", "Enter the Corrupted Woods"),
            ("flag", "Discovered.chapel_of_ash", "Search the Chapel of Ash"),
            ("flag", "Boss.veyla", "Silence Veyla, the Hollow Voice"),
        ],
        "reward": {"exp": 1200, "gold": 900, "crystals": 8, "fragments": 1, "bond": 30},
    },
    {
        "id": "ch4_rime_and_ruin", "chapter": 4, "title": "Rime and Ruin", "location": "The Frozen North",
        "summary": "Beyond Kingsfork the passes freeze over, and a troll king holds the heights.",
        "thumbnail": "Chapter4", "requires": ["ch3_hollow_voice"],
        "objectives": [
            ("flag", "Discovered.area_kingsfork", "Cross Kingsfork"),
            ("flag", "Discovered.region_frost", "Climb into the frozen north"),
            ("flag", "Boss.hrimgar", "Topple Hrimgar, the Troll King"),
        ],
        "reward": {"exp": 2000, "gold": 1400, "crystals": 12, "shards": 3, "fragments": 1, "bond": 40},
    },
    {
        "id": "ch5_molten_crown", "chapter": 5, "title": "The Molten Crown", "location": "The Caldera",
        "summary": "The caldera burns. Something colossal stirs beneath the ash, and it wears a crown of fire.",
        "thumbnail": "Chapter5", "requires": ["ch4_rime_and_ruin"],
        "objectives": [
            ("flag", "Discovered.region_molten", "Descend into the caldera"),
            ("flag", "Discovered.caldera_breach", "Reach the Caldera Breach"),
            ("flag", "Boss.kaelthar", "Defeat Kael'thar"),
        ],
        "reward": {"exp": 3200, "gold": 2200, "crystals": 18, "shards": 5, "fragments": 2, "bond": 60},
    },
    {
        "id": "ch6_fractured_skies", "chapter": 6, "title": "Fractured Skies", "location": "Kingsfork",
        "summary": "An ancient spire splits the sky above the old empire's heart. The storm and the sun meet there.",
        "thumbnail": "Chapter6", "requires": ["ch5_molten_crown"],
        "objectives": [
            ("flag", "Discovered.ruins_kingsfork_acropolis", "Investigate the ancient spire"),
            ("bond", 5, "Deepen the Bond (level 5)"),
            ("flag", "Story.SpireOpened", "Open the way to the spire's summit"),
        ],
        "reward": {"exp": 5000, "gold": 3000, "crystals": 25, "shards": 8, "fragments": 3},
    },
]

SIDE = [
    {
        "id": "side_thin_the_packs", "title": "Thin the Packs", "location": "Mossbrook",
        "summary": "Tilda's goats keep vanishing. Drive the wolves back from the village's edge.",
        "thumbnail": "Side_Forest", "requires": ["ch1_mossbrook"],
        "objectives": [("kill", "forest_wolf", 8, "Defeat forest wolves")],
        "reward": {"exp": 120, "gold": 150, "crystals": 1},
    },
    {
        "id": "side_raiders_road", "title": "Raiders on the Road", "location": "Brackenford",
        "summary": "Raiders and slingers prey on the travellers between the villages. Make the road safe again.",
        "thumbnail": "Side_Forest", "requires": ["ch1_mossbrook"],
        "objectives": [
            ("kill", "forest_raider", 6, "Defeat raiders"),
            ("kill", "forest_slinger", 4, "Defeat slingers"),
        ],
        "reward": {"exp": 220, "gold": 300, "items": ["DA_Item_IronboundGauntlets"], "tier": "UNCOMMON"},
    },
    {
        "id": "side_echoes_in_stone", "title": "Echoes in the Stone", "location": "The Verdant Wilds",
        "summary": "Old places hum with the storm's memory. Visit them and listen.",
        "thumbnail": "Chapter2", "requires": ["ch1_mossbrook"],
        "objectives": [
            ("flag", "Discovered.standing_stones", "Visit the Standing Stones"),
            ("flag", "Discovered.hollowroot_cave", "Explore Hollowroot Cave"),
            ("flag", "Discovered.mirrormere", "Find Mirrormere"),
        ],
        "reward": {"exp": 180, "gold": 120, "shards": 2},
    },
    {
        "id": "side_paths_of_light", "title": "Paths of Light", "location": "The Dominion",
        "summary": "The waystones remember the old roads. Wake enough of them and the Dominion opens up.",
        "thumbnail": "Chapter6", "requires": ["ch1_mossbrook"],
        "objectives": [("waystones", 5, "Attune waystones")],
        "reward": {"exp": 250, "crystals": 3, "shards": 2},
    },
    {
        "id": "side_two_hearts", "title": "Two Hearts, One Storm", "location": "Anywhere",
        "summary": "Fight side by side. The Bond between storm and sun grows with every battle won together.",
        "thumbnail": "Chapter1", "requires": ["ch1_mossbrook"],
        "objectives": [("bond", 3, "Reach Bond level 3")],
        "reward": {"exp": 300, "gold": 250, "fragments": 1},
    },
    {
        "id": "side_smiths_request", "title": "The Smith's Request", "location": "Mossbrook",
        "summary": "Bram wants to see what the old iron of the north looks like. Bring him an Ironbound Helm.",
        "thumbnail": "Chapter1", "requires": ["ch1_mossbrook"],
        "objectives": [
            ("item", "DA_Item_IronboundHelm", 1, "Carry an Ironbound Helm"),
            ("talk", "merchant_bram", "Show it to Bram"),
        ],
        "reward": {"exp": 200, "gold": 400, "crystals": 2},
    },
    {
        "id": "side_captains_watch", "title": "The Captain's Watch", "location": "The Old Watchtower",
        "summary": "Captain Ysolde lost contact with the old watchtower. See what became of it.",
        "thumbnail": "Side_Forest", "requires": ["ch2_pale_alpha"],
        "objectives": [
            ("talk", "guard_ysolde", "Speak with Captain Ysolde"),
            ("flag", "Discovered.old_watchtower", "Reach the old watchtower"),
        ],
        "reward": {"exp": 400, "gold": 350, "crystals": 3},
    },
    {
        "id": "side_rot_beneath", "title": "The Rot Beneath", "location": "Rotroot Hollow",
        "summary": "The blight spreads from somewhere below the trees. Burn out the wraiths that tend it.",
        "thumbnail": "Side_Blight", "requires": ["ch2_pale_alpha"],
        "objectives": [
            ("flag", "Discovered.rotroot_hollow", "Find Rotroot Hollow"),
            ("kill", "woods_wraith", 5, "Destroy blight wraiths"),
        ],
        "reward": {"exp": 700, "gold": 600, "crystals": 4, "shards": 1},
    },
    {
        "id": "side_black_waters", "title": "Black Waters", "location": "Blackmere",
        "summary": "The lakes of the corrupted woods have turned dark. Something drinks from them at night.",
        "thumbnail": "Side_Blight", "requires": ["ch3_hollow_voice"],
        "objectives": [
            ("flag", "Discovered.blackmere", "Reach Blackmere"),
            ("kill", "woods_beast", 3, "Hunt the blighted beasts"),
        ],
        "reward": {"exp": 900, "gold": 700, "shards": 2},
    },
    {
        "id": "side_cold_comfort", "title": "Cold Comfort", "location": "The Frozen North",
        "summary": "Frostholm's hunters speak of a grotto that glows under the ice, and a cairn that sings.",
        "thumbnail": "Side_Frost", "requires": ["ch3_hollow_voice"],
        "objectives": [
            ("flag", "Discovered.rime_grotto", "Find the Rime Grotto"),
            ("flag", "Discovered.skalds_cairn", "Hear the Skald's Cairn"),
        ],
        "reward": {"exp": 1100, "gold": 800, "crystals": 5, "shards": 2},
    },
    {
        "id": "side_through_the_slag", "title": "Through the Slag", "location": "The Caldera",
        "summary": "The old slag mines lead under the caldera, to a shrine that still keeps its fire.",
        "thumbnail": "Side_Molten", "requires": ["ch4_rime_and_ruin"],
        "objectives": [
            ("flag", "Discovered.slag_mines", "Enter the Slag Mines"),
            ("flag", "Discovered.shrine_of_the_pyre", "Reach the Shrine of the Pyre"),
        ],
        "reward": {"exp": 1600, "gold": 1200, "crystals": 6, "shards": 3},
    },
]
