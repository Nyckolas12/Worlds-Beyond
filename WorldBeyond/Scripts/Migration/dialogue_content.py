"""
Worlds Beyond - Plan 4 starter dialogue (read by migrate_pass13.py). Placeholder lines, in theme; edit freely.

Re-running pass 13 only adds rows that are missing from the tables, so edits made in the table editor survive
(BEYOND_REWRITE_DIALOGUE=1 writes every row below again).

Voices: Angel (he) is the storm - quick, restless, cheeky, warm underneath. Ji-Woong is the Radiant Hexblade -
calm, few words, dry, honour-bound. The starting village is Mossbrook, in the forest.

Row names: a chain "X" continues "X.1", "X.2"...; options point at other chains.
Story flags: Flag.<Name> in a row's Special Event sets <Name>; Boss.<boss id> (gorehide, veyla, kaelthar, hrimgar) is set when a boss falls.
"""

# ---------------------------------------------------------------- speakers (DT_Speakers)
# row: (name shown, portrait side in conversations)
SPEAKERS = {
    "Angel": ("Angel", "Left"),
    "JiWoong": ("Ji-Woong", "Left"),
    "Maren": ("Elder Maren", "Right"),
    "Bram": ("Bram", "Right"),
    "Ysolde": ("Captain Ysolde", "Right"),
    "Tilda": ("Tilda", "Right"),
    "Osk": ("Osk", "Right"),
    "Pell": ("Pell", "Right"),
    "Wren": ("Wren", "Right"),
}
PARTY = ("Angel", "JiWoong")

# In banter Angel's name sits left and Ji-Woong's right
BANTER_SIDES = {"Angel": "Left", "JiWoong": "Right"}

# ---------------------------------------------------------------- conversations (DT_Dialogue, face to face)
# chain: [(speaker, text) | (speaker, text, special_event) | ("@options", [(text, next_row), ...])]
# "next" says where a chain goes after its last line (default: the end)
CONVERSATIONS = {
    # Elder Maren
    "Elder_Intro": {"lines": [
        ("Maren", "So the storms were no ordinary weather. Two demigods, walking into Mossbrook on foot... the old songs never said you'd look so tired."),
        ("Angel", "Long road. Short temper. Mostly his."),
        ("JiWoong", "Mine is fine. We heard your village needs help, Elder."),
        ("Maren", "It does. A white wolf leads the packs in the forest now. Gorehide, the hunters call him. He took three of them this month."),
        ("Maren", "And past the forest, the eastern woods have gone wrong. Dark magic. The villages there stand empty."),
        ("@options", [("We'll hunt the Pale Alpha.", "Elder_Accept"),
                      ("Tell us about the corruption.", "Elder_Lore"),
                      ("We'll be back.", "None")]),
    ]},
    "Elder_Accept": {"lines": [
        ("Maren", "Then may the old gods remember you kindly. His den lies south-west, past the standing stones.", "Flag.Quest_Gorehide"),
        ("Angel", "Directions are nice. A map would be nicer."),
        ("Maren", "Bram sells those. Badly drawn, but honest."),
    ]},
    "Elder_Lore": {"lines": [
        ("Maren", "It began after the eclipse. The trees in the east stopped casting shadows... then the shadows started walking."),
        ("JiWoong", "Shadows with no master. Someone is feeding them."),
        ("Maren", "A voice, the survivors say. A woman's voice, hollow as a dry well. They call her Veyla."),
    ], "next": "Elder_Intro.5"},
    "Elder_Waiting": {"lines": [
        ("Maren", "The howls still carry at night. Gorehide's den is south-west, past the standing stones."),
        ("JiWoong", "We'll bring you a quiet night, Elder."),
    ]},
    "Elder_GorehideDone": {"lines": [
        ("Maren", "The hunters found the white wolf at dawn. You've given Mossbrook its nights back."),
        ("Angel", "He fought like he had something to prove. So did we."),
        ("Maren", "Then prove one more thing. The eastern woods... Veyla's voice grows louder every night.", "Flag.Quest_Veyla"),
        ("JiWoong", "Then we go east."),
    ]},
    "Elder_Default": {"lines": [
        ("Maren", "Rest by the fire while you can. The Dominion rarely gives second chances."),
    ]},

    # Bram, the merchant
    "Bram_Intro": {"lines": [
        ("Bram", "Customers! Real ones! ...Oh. You're the glowing sort. Do glowing sorts pay?", "Flag.Met_Bram"),
        ("Angel", "Depends what you're selling."),
        ("Bram", "Today? Nothing. Raiders took my cart on the forest road. Make the road safe and I'll have stock again."),
        ("JiWoong", "We'll keep an eye out for your cart."),
    ]},
    "Bram_AfterGorehide": {"lines": [
        ("Bram", "With the white wolf gone the carts are rolling again! The shop's still... a work in progress, mind you."),
    ]},
    "Bram_Default": {"lines": [
        ("Bram", "Maps, rope, lamp oil... once the road's safe. Come back then, glowing friends."),
    ]},

    # Captain Ysolde, at the north gate
    "Guard_Gate": {"lines": [
        ("Ysolde", "Halt. ...Fine, you're clearly not raiders. Raiders don't crackle."),
        ("Ysolde", "North of this gate the camps get bolder every week. Raiders in the western forest, worse things in the east."),
        ("@options", [("What's out there?", "Guard_Threats"),
                      ("We can handle it.", "Guard_Handle")]),
    ]},
    "Guard_Threats": {"lines": [
        ("Ysolde", "Two giants, if the scouts weren't drunk. A demon of molten rock to the north-west, and a troll king in the frost to the north-east."),
        ("Angel", "Fire and ice. Very balanced."),
        ("Ysolde", "Very deadly. Don't go north until you're stronger."),
    ]},
    "Guard_Handle": {"lines": [
        ("Ysolde", "Everyone says that. Their names are carved on the shrine by the well."),
        ("JiWoong", "Then we'll keep ours off it."),
    ]},
    "Guard_Both": {"lines": [
        ("Ysolde", "Both giants fallen. The scouts are sober for once and still don't believe it. Mossbrook owes you its walls."),
    ]},
    "Guard_Kaelthar": {"lines": [
        ("Ysolde", "The fires in the north-west went out last night. Was that you? ...Of course it was."),
    ]},
    "Guard_Hrimgar": {"lines": [
        ("Ysolde", "The frost is lifting off the northern road. The troll king's dead? The children will want songs."),
    ]},
}

# ---------------------------------------------------------------- over-head chatter (DT_TextOverHead)
# chain: [(who, text, seconds)] - who: "self" (the NPC that starts it) or a partner index (0, 1...)
CHATTER = {
    "Chat_Wolves": [
        ("self", "Did you hear the howling last night?", 3.5),
        (0, "Hard not to. Sounded closer than ever.", 3.5),
        ("self", "Old Pim says it's the white one. Gorehide.", 3.5),
        (0, "Old Pim says a lot of things.", 3.0),
    ],
    "Chat_Demigods": [
        ("self", "Is that... the storm-touched one? With the golden swordsman?", 4.0),
        (0, "Don't stare. Gods hate staring.", 3.0),
        ("self", "They're demigods, Osk. Half of them probably doesn't mind.", 4.0),
    ],
    "Chat_AlphaDead": [
        ("self", "The white wolf's dead! The hunters brought back his pelt.", 4.0),
        (0, "Then I'm sleeping with the window open tonight.", 3.5),
    ],
    "Chat_Crops": [
        ("self", "Another blighted field this morning. Black roots, every one.", 4.0),
        (0, "It's spreading from the eastern woods. Mother won't let me near the fence.", 4.0),
        ("self", "Smart woman, your mother.", 3.0),
    ],
    "Chat_Bram": [
        ("self", "Bram's been sulking since the raiders took his cart.", 3.5),
        (0, "He'll talk your ear off about it if you buy so much as a candle.", 4.0),
    ],
    "Chat_Veyla": [
        ("self", "They say the hollow voice sings at night in the eastern woods.", 3.5),
        (0, "Then don't listen. Promise me.", 3.0),
    ],
    "Chat_VeylaDead": [
        ("self", "Did you hear? The eastern woods went quiet.", 3.5),
        (0, "Quiet's good. Quiet means nothing's singing.", 3.5),
    ],
    # Greetings: one line each
    "Greet_Maren": [("self", "Welcome, travellers. Come, sit by the fire.", 3.0)],
    "Greet_Bram": [("self", "Step up, step up! ...There's nothing to step up to yet, but still!", 3.5)],
    "Greet_Ysolde": [("self", "Weapons stay sheathed inside the village.", 3.0)],
    "Greet_Tilda": [("self", "Oh! Hello!", 2.0)],
    "Greet_Osk": [("self", "Mornin'.", 2.0)],
    "Greet_Pell": [("self", "Mind the mud.", 2.0)],
    "Greet_Wren": [("self", "Are you really demigods?", 2.5)],
}

# ---------------------------------------------------------------- NPCs
# Blueprint name: settings. conversations: (start row, required flags, blocked-by flags, once)
# chatter: (start row, required, blocked); partners are wired per placed NPC (see VILLAGE).
NPCS = {
    "BP_NPC_Elder": {
        "id": "elder_maren", "name": "Elder Maren", "mesh": "Quinn", "materials": [],
        "conversations": [
            ("Elder_GorehideDone", ["Boss.gorehide"], [], True),
            ("Elder_Waiting", ["Quest_Gorehide"], ["Boss.gorehide"], False),
            ("Elder_Intro", [], ["Quest_Gorehide", "Boss.gorehide"], False),
            ("Elder_Default", [], [], False),
        ],
        "greet": ["Greet_Maren"],
    },
    "BP_NPC_Merchant": {
        "id": "merchant_bram", "name": "Bram", "mesh": "Manny", "materials": ["MI_Manny_01_Blue", "MI_Manny_02_Blue"],
        "conversations": [
            ("Bram_Intro", [], [], True),
            ("Bram_AfterGorehide", ["Boss.gorehide"], [], False),
            ("Bram_Default", [], [], False),
        ],
        "greet": ["Greet_Bram"],
    },
    "BP_NPC_Guard": {
        "id": "guard_ysolde", "name": "Captain Ysolde", "mesh": "Manny", "materials": ["MI_Manny_01_Red", "MI_Manny_02_Red"],
        "scale": 1.05,
        "conversations": [
            ("Guard_Both", ["Boss.kaelthar", "Boss.hrimgar"], [], False),
            ("Guard_Kaelthar", ["Boss.kaelthar"], [], False),
            ("Guard_Hrimgar", ["Boss.hrimgar"], [], False),
            ("Guard_Gate", [], [], False),
        ],
        "greet": ["Greet_Ysolde"],
    },
    "BP_NPC_Villager_Tilda": {
        "id": "villager_tilda", "name": "Tilda", "mesh": "Quinn", "materials": [],
        "chatter": [
            ("Chat_AlphaDead", ["Boss.gorehide"], []),
            ("Chat_Wolves", [], ["Boss.gorehide"]),
            ("Chat_Demigods", [], []),
        ],
        "greet": ["Greet_Tilda"],
    },
    "BP_NPC_Villager_Osk": {
        "id": "villager_osk", "name": "Osk", "mesh": "Manny", "materials": [],
        "greet": ["Greet_Osk"],
    },
    "BP_NPC_Villager_Pell": {
        "id": "villager_pell", "name": "Pell", "mesh": "Manny", "materials": ["MI_Manny_01_Red", "MI_Manny_02"],
        "chatter": [
            ("Chat_VeylaDead", ["Boss.veyla"], []),
            ("Chat_Veyla", ["Quest_Veyla"], ["Boss.veyla"]),
            ("Chat_Crops", [], ["Boss.veyla"]),
            ("Chat_Bram", [], []),
        ],
        "greet": ["Greet_Pell"],
    },
    "BP_NPC_Villager_Wren": {
        "id": "villager_wren", "name": "Wren", "mesh": "Quinn", "materials": [],
        "greet": ["Greet_Wren"],
    },
}

# Where they stand in TestArena's village corner: (blueprint, label, (x, y) offset from the village centre, yaw,
# chatter partner labels)
VILLAGE_CENTRE = (0.0, -2150.0)
VILLAGE = [
    ("BP_NPC_Elder", "NPC_ElderMaren", (-160.0, 170.0), -90.0, []),
    ("BP_NPC_Merchant", "NPC_Bram", (-750.0, -80.0), 0.0, []),
    ("BP_NPC_Guard", "NPC_CaptainYsolde", (60.0, 780.0), -90.0, []),
    ("BP_NPC_Villager_Tilda", "NPC_Tilda", (620.0, 40.0), 35.0, ["NPC_Osk"]),
    ("BP_NPC_Villager_Osk", "NPC_Osk", (760.0, 140.0), -145.0, []),
    ("BP_NPC_Villager_Pell", "NPC_Pell", (480.0, -520.0), 20.0, ["NPC_Wren"]),
    ("BP_NPC_Villager_Wren", "NPC_Wren", (630.0, -460.0), -160.0, []),
]

# Banter volumes: (label, context, centre (x, y), half size (x, y))
BANTER_VOLUMES = [
    ("Banter_Village", "region_village", (0.0, -2050.0), (1100.0, 1100.0)),
    ("Banter_Forest", "region_forest", (-2600.0, 1100.0), (1500.0, 1400.0)),
    ("Banter_CorruptedWoods", "region_corrupted_woods", (2600.0, 1500.0), (1500.0, 1400.0)),
]

# ---------------------------------------------------------------- banter (DT_Dialogue free movement + DA_Banter)
# id: trigger, context, once, cooldown, leader ("Angel" / "JiWoong" / None), lines [(speaker, text)],
#     required flags, blocked-by flags
BANTER = {
    "Village_1": ("RegionEntered", "region_village", True, 0, None, [
        ("Angel", "Smoke from chimneys. Real food, maybe."),
        ("JiWoong", "Real people, too. Mind your sparks."),
    ]),
    "Forest_1": ("RegionEntered", "region_forest", True, 0, None, [
        ("JiWoong", "These woods are old. Older than the village."),
        ("Angel", "Old and full of teeth. Something's watching us from the tree line."),
    ]),
    "Forest_2": ("RegionEntered", "region_forest", False, 600, None, [
        ("Angel", "Wolves again. Want to bet who gets more?"),
        ("JiWoong", "You count kills. I count the ones that got away from you."),
    ]),
    "Corrupt_1": ("RegionEntered", "region_corrupted_woods", True, 0, None, [
        ("Angel", "The air tastes wrong. Like a storm with no lightning."),
        ("JiWoong", "Dark magic. Even my blade's light flickers here. Stay close."),
    ]),
    "Corrupt_2": ("RegionEntered", "region_corrupted_woods", False, 600, None, [
        ("JiWoong", "No birdsong. Nothing alive wants to be here."),
        ("Angel", "Then let's not stay long enough to find out why."),
    ]),
    "Low_Angel": ("LowHealth", None, False, 120, "Angel", [
        ("Angel", "Okay... that one hurt. A lot."),
        ("JiWoong", "Fall back to me. I'll hold them."),
    ]),
    "Low_JiWoong": ("LowHealth", None, False, 120, "JiWoong", [
        ("JiWoong", "I'm bleeding light. Angel, cover me."),
        ("Angel", "On it. Try not to die heroically."),
    ]),
    "Low_Any": ("LowHealth", None, False, 120, None, [
        ("Angel", "We need to regroup!"),
        ("JiWoong", "Breathe. Strike when they overreach."),
    ]),
    "Boss_Gorehide": ("BossDefeated", "gorehide", True, 0, None, [
        ("Angel", "The Pale Alpha's down. Mossbrook can sleep tonight."),
        ("JiWoong", "We should tell the elder."),
    ]),
    "Boss_Veyla": ("BossDefeated", "veyla", True, 0, None, [
        ("JiWoong", "Her voice is gone. The woods are breathing again."),
        ("Angel", "Remind me never to listen to singing trees."),
    ]),
    "Boss_Kaelthar": ("BossDefeated", "kaelthar", True, 0, None, [
        ("Angel", "A demon made of lava. I'll be smelling smoke in my hair for a week."),
        ("JiWoong", "Your hair survived. The colossus didn't."),
    ]),
    "Boss_Hrimgar": ("BossDefeated", "hrimgar", True, 0, None, [
        ("JiWoong", "The troll king kneels at last."),
        ("Angel", "He didn't kneel, he fell over. Big difference. Lots of ice."),
    ]),
    "Boss_Any": ("BossDefeated", None, False, 60, None, [
        ("Angel", "Is it dead? Tell me it's dead."),
        ("JiWoong", "It's dead. Breathe."),
    ]),
    "Idle_1": ("Idle", None, False, 300, None, [
        ("Angel", "Are we waiting for something, or just admiring the grass?"),
        ("JiWoong", "I was meditating. You were fidgeting."),
    ]),
    "Idle_2": ("Idle", None, False, 300, None, [
        ("JiWoong", "When this is over, what will you do?"),
        ("Angel", "Sleep for a hundred years. Then eat. Then sleep again."),
    ]),
    "Idle_3": ("Idle", None, False, 300, None, [
        ("Angel", "Your sword hums when it's quiet. Did you know that?"),
        ("JiWoong", "It's listening. Unlike you."),
    ]),
    "Idle_4": ("Idle", None, False, 300, None, [
        ("JiWoong", "Your lightning leaves scorch marks on everything."),
        ("Angel", "It's called style."),
    ]),
    "Level_1": ("LevelUp", None, False, 120, None, [
        ("Angel", "Felt that. Stronger, faster... still handsome."),
        ("JiWoong", "Two out of three."),
    ]),
    "Level_2": ("LevelUp", None, False, 120, None, [
        ("JiWoong", "The light answers more easily now."),
        ("Angel", "Good. Make it answer louder."),
    ]),
    "Revive_1": ("Revived", None, False, 90, None, [
        ("Angel", "Up you get. You're not allowed to quit before me."),
        ("JiWoong", "...Noted. Thank you."),
    ]),
    "Revive_2": ("Revived", None, False, 90, None, [
        ("JiWoong", "Stay down next time and I'll leave you there."),
        ("Angel", "You say that every time."),
    ]),
}

LEADERS = {
    "Angel": "/Game/WorldsBeyond/Characters/Angel/BP_Angel.BP_Angel_C",
    "JiWoong": "/Game/WorldsBeyond/Characters/Ji-Woong/BP_Ji-Woong.BP_Ji-Woong_C",
}
