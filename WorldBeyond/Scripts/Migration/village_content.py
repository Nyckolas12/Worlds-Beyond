"""
Worlds Beyond - Plan 5C village content (read by migrate_pass17.py): what each village is built from, who lives there
and what they say. Placeholder names and lines, in theme; edit freely.

Re-running pass 17 only adds dialogue rows that are missing (BEYOND_REWRITE_DIALOGUE=1 writes them all again) and only
builds villages that have nothing yet (BEYOND_REBUILD_VILLAGES=1, or =mossbrook,frostholm for some).

Voices (as in dialogue_content.py): Angel (he) is the storm - quick, restless, cheeky, warm underneath. Ji-Woong is the
Radiant Hexblade - calm, few words, dry, honour-bound.
Row names: a chain "X" continues "X.1", "X.2"...; Flag.<Name> in a row's Special Event sets <Name>.
"""

# ---------------------------------------------------------------- how the villages are built
# houses: village_kit recipes, "+b" boarded up; rings: where the houses stand (fractions of the village radius);
# props: "busy" / "quiet" / "empty"; stalls: market stalls on the square; gates: a post-and-beam gate where each road
# comes in; palisade: a stake wall round the village (gaps for the roads); lanterns: lantern posts round the square
VILLAGES = {
    "village_mossbrook": dict(houses=["longhouse", "town_house", "cottage", "gable_house", "workshop", "cottage", "town_house",
                                      "cottage_small", "gable_house", "cottage", "tower_house", "cottage_small"],
                              rings=(0.3, 0.5), props="busy", stalls=3, gates=True, palisade=False, lanterns=6, seed=11),
    "village_fernhollow": dict(houses=["cottage", "cottage_small", "gable_house", "workshop", "cottage_small", "cottage"],
                               rings=(0.38,), props="busy", stalls=1, gates=True, palisade=False, lanterns=3, seed=12),
    "village_brackenford": dict(houses=["cottage", "longhouse", "cottage_small", "gable_house", "cottage_small", "cottage"],
                                rings=(0.38,), props="busy", stalls=1, gates=True, palisade=False, lanterns=3, seed=13),
    "village_willowmere": dict(houses=["cottage", "town_house", "cottage_small", "gable_house", "cottage_small"],
                               rings=(0.4,), props="busy", stalls=1, gates=True, palisade=False, lanterns=3, seed=14),
    "village_duskwatch": dict(houses=["longhouse", "tower_house", "town_house", "workshop", "cottage", "tower_house", "cottage_small"],
                              rings=(0.33,), props="quiet", stalls=1, gates=True, palisade=True, lanterns=6, seed=15),
    "village_greyhallow": dict(houses=["cottage+b", "town_house+b", "cottage_small+b", "longhouse+b", "cottage+b", "cottage_small+b"],
                               rings=(0.38,), props="empty", stalls=0, gates=True, palisade=False, lanterns=0, seed=16),
    "village_marrowfield": dict(houses=["longhouse+b", "cottage+b", "town_house+b", "cottage_small+b", "cottage+b"],
                                rings=(0.38,), props="empty", stalls=0, gates=True, palisade=False, lanterns=0, seed=17),
    "village_sallowend": dict(houses=["cottage+b", "cottage_small+b", "town_house+b", "cottage+b"],
                              rings=(0.4,), props="empty", stalls=0, gates=False, palisade=False, lanterns=0, seed=18),
    "village_frostholm": dict(houses=["longhouse", "longhouse", "town_house", "cottage", "workshop", "gable_house", "cottage",
                                      "tower_house", "cottage_small"],
                              rings=(0.32, 0.52), props="busy", stalls=2, gates=True, palisade=True, lanterns=6, seed=19),
    "village_pinecrest": dict(houses=["workshop", "cottage", "cottage_small", "longhouse", "cottage_small"],
                              rings=(0.4,), props="busy", stalls=0, gates=True, palisade=False, lanterns=3, seed=20),
    "village_hearthfall": dict(houses=["longhouse", "cottage", "gable_house", "cottage_small", "town_house"],
                               rings=(0.4,), props="busy", stalls=1, gates=True, palisade=False, lanterns=4, seed=21),
    "village_cinderhold": dict(houses=["workshop", "longhouse", "town_house", "workshop", "cottage", "tower_house", "gable_house",
                                       "cottage_small"],
                               rings=(0.32, 0.52), props="busy", stalls=2, gates=True, palisade=True, lanterns=6, seed=22),
    "village_slagford": dict(houses=["workshop", "cottage_small", "longhouse", "cottage", "cottage_small"],
                             rings=(0.4,), props="quiet", stalls=0, gates=True, palisade=False, lanterns=2, seed=23),
    "village_emberrest": dict(houses=["gable_house", "cottage", "tower_house", "cottage_small", "cottage"],
                              rings=(0.4,), props="quiet", stalls=1, gates=True, palisade=False, lanterns=4, seed=24),
    # Kingsfork Crossroads: the inn and its stable where the roads meet
    "area_kingsfork": dict(houses=["longhouse", "workshop"], rings=(0.3,), props="busy", stalls=1, gates=False, palisade=False,
                           lanterns=4, seed=25),
}

# ---------------------------------------------------------------- Mossbrook's people (Plan 4's NPC Blueprints)
# (blueprint, label, (east, north) m from the village centre, facing (degrees, 0 = east, 90 = north), chatter partner labels)
MOSSBROOK = [
    ("BP_NPC_Elder", "elder_maren", (-3.0, 4.0), -90.0, []),
    ("BP_NPC_Merchant", "merchant_bram", (-9.0, -2.0), 0.0, []),
    ("BP_NPC_Guard", "guard_ysolde", (1.0, 13.0), 90.0, []),
    ("BP_NPC_Villager_Tilda", "villager_tilda", (6.0, 1.0), 200.0, ["villager_osk"]),
    ("BP_NPC_Villager_Osk", "villager_osk", (7.4, 0.0), 20.0, []),
    ("BP_NPC_Villager_Pell", "villager_pell", (4.0, -6.0), 160.0, ["villager_wren"]),
    ("BP_NPC_Villager_Wren", "villager_wren", (5.5, -6.6), -20.0, []),
]

# ---------------------------------------------------------------- the other villagers
# Placed as BP_NPC_Base with their own settings. id: village, name, look ("Manny" / "Quinn" + material instances),
# where ((east, north) m from the village centre, facing), conversations [(start, required, blocked, once)],
# chatter [(start, required, blocked)], greetings, partners (ids), wander radius (m)
VILLAGERS = {
    # Fernhollow: hunters and foragers on the western edge, raiders in the trees
    "fern_garrick": dict(village="village_fernhollow", name="Hunter Garrick", look=("Manny", ["MI_Manny_01_Red", "MI_Manny_02"]),
                         at=((-4.0, 3.0), -60.0),
                         conversations=[("Fern_Garrick_Intro", [], ["Fern_Garrick_Met"], False), ("Fern_Garrick_Default", [], [], False)],
                         greet=["Greet_Fern_Garrick"]),
    "fern_lisbet": dict(village="village_fernhollow", name="Lisbet", look=("Quinn", ["MI_Quinn_02"]), at=((5.0, -2.0), 160.0),
                        chatter=[("Chat_Fern_Raiders", [], [])], greet=["Greet_Villager_A"], partners=["fern_tam"]),
    "fern_tam": dict(village="village_fernhollow", name="Tam", look=("Manny", ["MI_Manny_01_Blue", "MI_Manny_02_Blue"]),
                     at=((6.4, -2.8), -20.0), greet=["Greet_Villager_B"]),

    # Brackenford: a fishing hamlet at the ford by Mirrormere
    "brack_hild": dict(village="village_brackenford", name="Ferrywoman Hild", look=("Quinn", ["MI_Quinn_01"]), at=((3.0, 4.0), -120.0),
                       conversations=[("Brack_Hild_Intro", [], ["Brack_Hild_Met"], False), ("Brack_Hild_Default", [], [], False)],
                       greet=["Greet_Brack_Hild"]),
    "brack_odo": dict(village="village_brackenford", name="Odo", look=("Manny", ["MI_Manny_01", "MI_Manny_02_Blue"]),
                      at=((-5.0, -1.0), 10.0), chatter=[("Chat_Brack_Fish", [], [])], greet=["Greet_Villager_B"], partners=["brack_wynn"]),
    "brack_wynn": dict(village="village_brackenford", name="Wynn", look=("Quinn", ["MI_Quinn_02"]), at=((-3.6, -0.4), 190.0),
                       greet=["Greet_Villager_A"], wander=5.0),

    # Willowmere: the last village before the Gap; folk who fled the blight
    "will_ansel": dict(village="village_willowmere", name="Healer Ansel", look=("Manny", ["MI_Manny_01_Blue", "MI_Manny_02"]),
                       at=((-3.0, -3.0), 45.0),
                       conversations=[("Will_Ansel_Veyla", ["Boss.veyla"], [], True), ("Will_Ansel_Intro", [], ["Will_Ansel_Met"], False),
                                      ("Will_Ansel_Default", [], [], False)],
                       greet=["Greet_Will_Ansel"]),
    "will_mira": dict(village="village_willowmere", name="Mira", look=("Quinn", ["MI_Quinn_01"]), at=((4.0, 2.0), 200.0),
                      chatter=[("Chat_Will_Home", [], ["Boss.veyla"]), ("Chat_Will_Back", ["Boss.veyla"], [])], greet=["Greet_Villager_A"],
                      partners=["will_jory"]),
    "will_jory": dict(village="village_willowmere", name="Jory", look=("Manny", ["MI_Manny_01_Red", "MI_Manny_02_Red"]),
                      at=((5.4, 1.2), 20.0), greet=["Greet_Villager_B"]),

    # Duskwatch: the palisade outpost holding the road into the Blightwood
    "dusk_corvin": dict(village="village_duskwatch", name="Sergeant Corvin", look=("Manny", ["MI_Manny_01_Red", "MI_Manny_02_Red"]),
                        at=((2.0, 5.0), -90.0), scale=1.05,
                        conversations=[("Dusk_Corvin_Veyla", ["Boss.veyla"], [], False), ("Dusk_Corvin_Intro", [], ["Dusk_Corvin_Met"], False),
                                       ("Dusk_Corvin_Default", [], [], False)],
                        greet=["Greet_Dusk_Corvin"]),
    "dusk_hale": dict(village="village_duskwatch", name="Private Hale", look=("Manny", ["MI_Manny_01_Red", "MI_Manny_02"]),
                      at=((-5.0, 0.0), 0.0), chatter=[("Chat_Dusk_Watch", [], ["Boss.veyla"]), ("Chat_Dusk_Quiet", ["Boss.veyla"], [])],
                      greet=["Greet_Soldier"], partners=["dusk_brenna"]),
    "dusk_brenna": dict(village="village_duskwatch", name="Private Brenna", look=("Quinn", ["MI_Quinn_02"]), at=((-3.6, 0.6), 180.0),
                        greet=["Greet_Soldier"]),

    # Frostholm: the hold of the Rimewood
    "frost_sigrun": dict(village="village_frostholm", name="Jarl Sigrun", look=("Quinn", ["MI_Quinn_01"]), at=((0.0, 6.0), -90.0),
                         scale=1.05,
                         conversations=[("Frost_Sigrun_Hrimgar", ["Boss.hrimgar"], [], True), ("Frost_Sigrun_Intro", [], ["Frost_Sigrun_Met"], False),
                                        ("Frost_Sigrun_Default", [], [], False)],
                         greet=["Greet_Frost_Sigrun"]),
    "frost_ulf": dict(village="village_frostholm", name="Ulf the Smith", look=("Manny", ["MI_Manny_01", "MI_Manny_02_Red"]),
                      at=((-6.0, -2.0), 20.0), chatter=[("Chat_Frost_Cold", [], [])], greet=["Greet_Frost_Ulf"], partners=["frost_runa"]),
    "frost_runa": dict(village="village_frostholm", name="Runa", look=("Quinn", ["MI_Quinn_02"]), at=((-4.6, -1.4), 200.0),
                       greet=["Greet_Villager_A"], wander=6.0),

    # Pinecrest: woodcutters under the Wolfjaw
    "pine_eskil": dict(village="village_pinecrest", name="Woodcutter Eskil", look=("Manny", ["MI_Manny_01_Blue", "MI_Manny_02"]),
                       at=((3.0, -3.0), 135.0),
                       conversations=[("Pine_Eskil_Intro", [], ["Pine_Eskil_Met"], False), ("Pine_Eskil_Default", [], [], False)],
                       greet=["Greet_Pine_Eskil"]),
    "pine_gudrun": dict(village="village_pinecrest", name="Gudrun", look=("Quinn", ["MI_Quinn_01"]), at=((-4.0, 2.0), -30.0),
                        chatter=[("Chat_Pine_Wolves", [], [])], greet=["Greet_Villager_A"], partners=["pine_arvid"]),
    "pine_arvid": dict(village="village_pinecrest", name="Arvid", look=("Manny", ["MI_Manny_01", "MI_Manny_02_Blue"]),
                       at=((-2.8, 1.2), 150.0), greet=["Greet_Villager_B"]),

    # Hearthfall: round a warm spring in the snow
    "hearth_thora": dict(village="village_hearthfall", name="Keeper Thora", look=("Quinn", ["MI_Quinn_02"]), at=((-3.0, 3.0), -45.0),
                         conversations=[("Hearth_Thora_Intro", [], ["Hearth_Thora_Met"], False), ("Hearth_Thora_Default", [], [], False)],
                         greet=["Greet_Hearth_Thora"]),
    "hearth_bjorn": dict(village="village_hearthfall", name="Bjorn", look=("Manny", ["MI_Manny_01_Red", "MI_Manny_02"]),
                         at=((4.0, -2.0), 160.0), chatter=[("Chat_Hearth_Spring", [], [])], greet=["Greet_Villager_B"],
                         partners=["hearth_ylva"]),
    "hearth_ylva": dict(village="village_hearthfall", name="Ylva", look=("Quinn", ["MI_Quinn_01"]), at=((5.4, -2.6), -20.0),
                        greet=["Greet_Villager_A"]),

    # Cinderhold: the miners' town under the volcano
    "cinder_brann": dict(village="village_cinderhold", name="Forgemaster Brann", look=("Manny", ["MI_Manny_01", "MI_Manny_02_Red"]),
                         at=((3.0, 4.0), -120.0), scale=1.08,
                         conversations=[("Cinder_Brann_Kaelthar", ["Boss.kaelthar"], [], True), ("Cinder_Brann_Intro", [], ["Cinder_Brann_Met"], False),
                                        ("Cinder_Brann_Default", [], [], False)],
                         greet=["Greet_Cinder_Brann"]),
    "cinder_kestra": dict(village="village_cinderhold", name="Kestra", look=("Quinn", ["MI_Quinn_02"]), at=((-5.0, -1.0), 10.0),
                          chatter=[("Chat_Cinder_Ash", [], ["Boss.kaelthar"]), ("Chat_Cinder_Calm", ["Boss.kaelthar"], [])],
                          greet=["Greet_Villager_A"], partners=["cinder_dorn"]),
    "cinder_dorn": dict(village="village_cinderhold", name="Dorn", look=("Manny", ["MI_Manny_01_Red", "MI_Manny_02_Red"]),
                        at=((-3.6, -0.4), 190.0), greet=["Greet_Miner"]),

    # Slagford: rough folk of the slag mines
    "slag_rusk": dict(village="village_slagford", name="Foreman Rusk", look=("Manny", ["MI_Manny_01_Red", "MI_Manny_02"]),
                      at=((-3.0, -3.0), 45.0),
                      conversations=[("Slag_Rusk_Intro", [], ["Slag_Rusk_Met"], False), ("Slag_Rusk_Default", [], [], False)],
                      greet=["Greet_Slag_Rusk"]),
    "slag_nell": dict(village="village_slagford", name="Nell", look=("Quinn", ["MI_Quinn_01"]), at=((4.0, 2.0), 200.0),
                      chatter=[("Chat_Slag_Mines", [], [])], greet=["Greet_Miner"], partners=["slag_cobb"]),
    "slag_cobb": dict(village="village_slagford", name="Cobb", look=("Manny", ["MI_Manny_01", "MI_Manny_02_Blue"]), at=((5.4, 1.4), 20.0),
                      greet=["Greet_Miner"]),

    # Emberrest: pilgrims of the Pyre
    "ember_ashka": dict(village="village_emberrest", name="Pilgrim Ashka", look=("Quinn", ["MI_Quinn_02"]), at=((2.0, 4.0), -100.0),
                        conversations=[("Ember_Ashka_Intro", [], ["Ember_Ashka_Met"], False), ("Ember_Ashka_Default", [], [], False)],
                        greet=["Greet_Ember_Ashka"]),
    "ember_soren": dict(village="village_emberrest", name="Brother Soren", look=("Manny", ["MI_Manny_01_Blue", "MI_Manny_02_Blue"]),
                        at=((-4.0, -2.0), 20.0), chatter=[("Chat_Ember_Pyre", [], [])], greet=["Greet_Pilgrim"], partners=["ember_isa"]),
    "ember_isa": dict(village="village_emberrest", name="Sister Isa", look=("Quinn", ["MI_Quinn_01"]), at=((-2.6, -1.4), 200.0),
                      greet=["Greet_Pilgrim"], wander=5.0),
}

# ---------------------------------------------------------------- speakers (DT_Speakers): row: (name shown, side)
SPEAKERS = {
    "Garrick": ("Hunter Garrick", "Right"),
    "Hild": ("Ferrywoman Hild", "Right"),
    "Ansel": ("Healer Ansel", "Right"),
    "Corvin": ("Sergeant Corvin", "Right"),
    "Sigrun": ("Jarl Sigrun", "Right"),
    "Eskil": ("Woodcutter Eskil", "Right"),
    "Thora": ("Keeper Thora", "Right"),
    "Brann": ("Forgemaster Brann", "Right"),
    "Rusk": ("Foreman Rusk", "Right"),
    "Ashka": ("Pilgrim Ashka", "Right"),
}

# ---------------------------------------------------------------- conversations (DT_Dialogue, face to face)
# chain: [(speaker, text) | (speaker, text, special_event)]
CONVERSATIONS = {
    "Fern_Garrick_Intro": {"lines": [
        ("Garrick", "Easy. Lower the sparks, friend - we've had enough strangers walk out of those trees with steel in hand."),
        ("Angel", "Raiders?"),
        ("Garrick", "A camp of them, west along the creek. They take our snares, our smoked meat, once a cart and its mule."),
        ("JiWoong", "Bram's cart, from Mossbrook."),
        ("Garrick", "Aye, that's the one. If you're heading west, they won't be glad to see you. I'd count that a favour.",
         "Flag.Fern_Garrick_Met"),
    ]},
    "Fern_Garrick_Default": {"lines": [
        ("Garrick", "Keep to the path after dark. The trees out west have eyes, and most of them carry knives."),
    ]},
    "Brack_Hild_Intro": {"lines": [
        ("Hild", "Ferry's free for demigods. Not that you need one - you look like you'd walk across the lake if it suited you."),
        ("Angel", "I've thought about it."),
        ("Hild", "Mirrormere shows you the sky even at night. The old folk say the dead look back at you from it."),
        ("JiWoong", "Do they?"),
        ("Hild", "Never caught one at it. South-east there's the Old Quarry - something's moved into it. Big. Keep that in mind.",
         "Flag.Brack_Hild_Met"),
    ]},
    "Brack_Hild_Default": {"lines": [
        ("Hild", "Fish are biting. Wolves too, along the north shore. Mind which one you're feeding."),
    ]},
    "Will_Ansel_Intro": {"lines": [
        ("Ansel", "Half the beds in my house are full of Greyhallow folk. They came through the Gap with black roots in their wounds."),
        ("JiWoong", "The blight follows them?"),
        ("Ansel", "It follows the wind. Something in the eastern woods sings, and the forest dies a little more each night."),
        ("Angel", "Then we go find the singer and ask her to stop. Politely. With lightning."),
        ("Ansel", "Duskwatch still holds the road beyond the Gap. Start there.", "Flag.Will_Ansel_Met"),
    ]},
    "Will_Ansel_Veyla": {"lines": [
        ("Ansel", "The wounds are closing. Every one of them, all at once, the night the singing stopped. That was you, wasn't it?"),
        ("Angel", "We had help. Mostly his sword."),
        ("Ansel", "Then Willowmere owes you both. Come back whenever you're hurt - my door is yours."),
    ]},
    "Will_Ansel_Default": {"lines": [
        ("Ansel", "Rest if you need to. I've herbs for burns, cuts and whatever it is lightning does to a man."),
    ]},
    "Dusk_Corvin_Intro": {"lines": [
        ("Corvin", "Halt - oh. You're the ones from Mossbrook. Ysolde's riders said you'd come. Took your time."),
        ("Angel", "We stopped to fight everything on the way. It adds up."),
        ("Corvin", "Greyhallow, Marrowfield, Sallow End... all emptied in one season. We hold this wall so the blight doesn't reach the Gap."),
        ("JiWoong", "And the source?"),
        ("Corvin", "South-east, the Weeping Grove. Nobody I've sent there has come back singing the same song.", "Flag.Dusk_Corvin_Met"),
    ]},
    "Dusk_Corvin_Veyla": {"lines": [
        ("Corvin", "The woods went quiet, and my lads slept a whole night through for the first time in a year. Thank you."),
        ("JiWoong", "Hold the wall a little longer. The villages will need people again."),
    ]},
    "Dusk_Corvin_Default": {"lines": [
        ("Corvin", "Stay off the bog road after dusk. Witchlight out there, and it doesn't lead anywhere you'd want to go."),
    ]},
    "Frost_Sigrun_Intro": {"lines": [
        ("Sigrun", "Two southerners in summer cloth, alive in the Rimewood. Either you're very strong or very stupid."),
        ("Angel", "Can it be both?"),
        ("Sigrun", "Ha! It usually is. Frostholm welcomes you. Eat, then listen."),
        ("Sigrun", "Hrimgar the troll king squats in the Frozen Crown, north-east. Each winter his frost reaches further south."),
        ("JiWoong", "Then this winter it stops.", "Flag.Frost_Sigrun_Met"),
    ]},
    "Frost_Sigrun_Hrimgar": {"lines": [
        ("Sigrun", "The Crown is cracking. Meltwater in the Glassmere for the first time in living memory. You did this."),
        ("Angel", "He fell over. A lot. It was mostly gravity."),
        ("Sigrun", "Then Frostholm will drink to gravity tonight - and to you. Your names go in the skald's cairn."),
    ]},
    "Frost_Sigrun_Default": {"lines": [
        ("Sigrun", "The Glassmere holds your weight. Mostly. Walk light."),
    ]},
    "Pine_Eskil_Intro": {"lines": [
        ("Eskil", "Careful where you stand - that pine's coming down. ...There. Now, what brings demigods up the mountain?"),
        ("JiWoong", "The road. And whatever is hunting along it."),
        ("Eskil", "Frost wolves. White ones, big as ponies. They den up by the Wolfjaw Pass and come down when the snow's deep."),
        ("Angel", "Big as ponies. Great. I love ponies.", "Flag.Pine_Eskil_Met"),
    ]},
    "Pine_Eskil_Default": {"lines": [
        ("Eskil", "Pinecrest timber built half of Frostholm. The other half's still standing on it."),
    ]},
    "Hearth_Thora_Intro": {"lines": [
        ("Thora", "Welcome to Hearthfall. Warm your hands in the spring - it's never once frozen, not even in the long winter."),
        ("Angel", "Hot water in the snow? I'm moving here."),
        ("Thora", "People say a fire spirit sleeps under it. I say it's just the mountain breathing. Either way, it keeps us alive."),
        ("JiWoong", "We'll keep it that way.", "Flag.Hearth_Thora_Met"),
    ]},
    "Hearth_Thora_Default": {"lines": [
        ("Thora", "The Rimegate's open to the south. Duskwatch folk trade furs for our smoked fish."),
    ]},
    "Cinder_Brann_Intro": {"lines": [
        ("Brann", "Mind the slag. Steps glow for a reason. You'll be the storm-born and the golden one."),
        ("JiWoong", "You know of us?"),
        ("Brann", "Ash carries news faster than birds out here. Listen: something woke in the caldera. Kael'thar, the old ones call him."),
        ("Brann", "His knights walk the flats in burned armour. We can't dig, we can't trade, and the mountain grumbles every night."),
        ("Angel", "Lava demon. Knights. Grumbling mountain. Got it.", "Flag.Cinder_Brann_Met"),
    ]},
    "Cinder_Brann_Kaelthar": {"lines": [
        ("Brann", "The mountain's quiet. First time I can hear my own hammer. You put the demon down?"),
        ("JiWoong", "He won't trouble Cinderhold again."),
        ("Brann", "Then the forge is yours. Anything I make, you'll have first look at."),
    ]},
    "Cinder_Brann_Default": {"lines": [
        ("Brann", "The Caldera Breach is the only way in. Bring water. Bring more water than that."),
    ]},
    "Slag_Rusk_Intro": {"lines": [
        ("Rusk", "Not another pair of heroes. Last ones went into the mines and came back as ash."),
        ("Angel", "We're better than the last ones."),
        ("Rusk", "That's what they said. The Slag Mines are full of beasts that spit molten rock. Clear them, and I'll eat my words."),
        ("JiWoong", "Have them ready.", "Flag.Slag_Rusk_Met"),
    ]},
    "Slag_Rusk_Default": {"lines": [
        ("Rusk", "Work's work. The mountain doesn't care who's swinging the pick."),
    ]},
    "Ember_Ashka_Intro": {"lines": [
        ("Ashka", "Peace on your road, travellers. We walk to the Shrine of the Pyre, north of here, to give our ashes to the flame."),
        ("JiWoong", "Your ashes?"),
        ("Ashka", "What we were, before. The Pyre takes it, and we go home lighter. You both carry a great deal, I think."),
        ("Angel", "...She's not wrong."),
        ("Ashka", "The view from Emberfall shows the whole Dominion. Go there, when the world feels too heavy.", "Flag.Ember_Ashka_Met"),
    ]},
    "Ember_Ashka_Default": {"lines": [
        ("Ashka", "The flame remembers everyone who stands before it. Even demigods."),
    ]},
}

# ---------------------------------------------------------------- over-head chatter (DT_TextOverHead)
# chain: [(who, text, seconds)] - who: "self" or a partner index
CHATTER = {
    "Chat_Fern_Raiders": [
        ("self", "They took the smokehouse door this time. The door.", 3.5),
        (0, "What does a raider want with a door?", 3.0),
        ("self", "Firewood, Tam. It's always firewood.", 3.0),
    ],
    "Chat_Brack_Fish": [
        ("self", "Caught a pike this long. Swear on my nets.", 3.0),
        (0, "You said that last week. It was a boot.", 3.0),
        ("self", "A big boot.", 2.0),
    ],
    "Chat_Will_Home": [
        ("self", "Do you think we'll ever go back to Greyhallow?", 3.5),
        (0, "Not while the trees there still whisper.", 3.0),
    ],
    "Chat_Will_Back": [
        ("self", "The roots stopped spreading! We could go home by spring.", 3.5),
        (0, "Then I'm planting the first field. Right where the black ones were.", 3.5),
    ],
    "Chat_Dusk_Watch": [
        ("self", "Did you hear it last night? The singing?", 3.0),
        (0, "Stuff your ears with wax and keep your eyes on the treeline.", 3.5),
    ],
    "Chat_Dusk_Quiet": [
        ("self", "It's too quiet now. I almost miss the singing.", 3.0),
        (0, "Say that again and I'll sing to you myself.", 3.0),
    ],
    "Chat_Frost_Cold": [
        ("self", "Cold enough to freeze the words in your mouth today.", 3.5),
        (0, "Good. Then you'll stop complaining about it.", 3.0),
    ],
    "Chat_Pine_Wolves": [
        ("self", "Saw tracks by the sawpit. Wide as my hand.", 3.0),
        (0, "Then we work in pairs tomorrow. Axes close.", 3.0),
    ],
    "Chat_Hearth_Spring": [
        ("self", "The spring's warmer than usual. Steam all morning.", 3.0),
        (0, "Mother says it means a hard winter coming.", 3.0),
        ("self", "Your mother says everything means a hard winter coming.", 3.5),
    ],
    "Chat_Cinder_Ash": [
        ("self", "Ash in the bread again. Ash in everything.", 3.0),
        (0, "Better ash than lava. The mountain's angry.", 3.0),
    ],
    "Chat_Cinder_Calm": [
        ("self", "No ashfall for three days. The sky's actually blue.", 3.0),
        (0, "Don't get used to it. Go dig while it lasts.", 3.0),
    ],
    "Chat_Slag_Mines": [
        ("self", "Lost two picks and a cart down the east shaft.", 3.0),
        (0, "Lost? Or melted?", 2.5),
        ("self", "...Melted.", 2.0),
    ],
    "Chat_Ember_Pyre": [
        ("self", "Three more days to the Pyre, if the road is clear.", 3.0),
        (0, "The road is never clear, Brother. That is the point of walking it.", 3.5),
    ],
    # Greetings: one line each
    "Greet_Fern_Garrick": [("self", "Mind the snares by the gate.", 2.5)],
    "Greet_Brack_Hild": [("self", "Need the ferry? Free for heroes.", 2.5)],
    "Greet_Will_Ansel": [("self", "Hurt? Come in, come in.", 2.5)],
    "Greet_Dusk_Corvin": [("self", "Eyes on the trees, everyone.", 2.5)],
    "Greet_Frost_Sigrun": [("self", "Southerners! In the snow!", 2.5)],
    "Greet_Frost_Ulf": [("self", "Need an edge put on that?", 2.5)],
    "Greet_Pine_Eskil": [("self", "Timber!", 2.0)],
    "Greet_Hearth_Thora": [("self", "Warm yourselves by the spring.", 2.5)],
    "Greet_Cinder_Brann": [("self", "Watch the slag.", 2.0)],
    "Greet_Slag_Rusk": [("self", "More heroes. Wonderful.", 2.5)],
    "Greet_Ember_Ashka": [("self", "Peace on your road.", 2.5)],
    "Greet_Villager_A": [("self", "Oh! Good day to you.", 2.0)],
    "Greet_Villager_B": [("self", "Travellers. Welcome.", 2.0)],
    "Greet_Soldier": [("self", "Keep moving, the watch is busy.", 2.5)],
    "Greet_Miner": [("self", "Mind your feet. Floor's hot.", 2.5)],
    "Greet_Pilgrim": [("self", "May the flame warm you.", 2.5)],
}

# ---------------------------------------------------------------- banter for the new places (DT_Dialogue + DA_Banter)
# id: trigger, context, once, cooldown, leader, lines [(speaker, text)]
BANTER = {
    "Frost_1": ("RegionEntered", "region_frost", True, 0, None, [
        ("Angel", "Okay. Cold. Really cold. Why didn't anyone tell me about the cold?"),
        ("JiWoong", "Captain Ysolde did. Twice. You were eating."),
    ]),
    "Frost_2": ("RegionEntered", "region_frost", False, 600, None, [
        ("JiWoong", "Your sparks hiss in the snow."),
        ("Angel", "They're complaining. I'm with them."),
    ]),
    "Molten_1": ("RegionEntered", "region_molten", True, 0, None, [
        ("JiWoong", "The ground is warm through my boots. The mountain is awake."),
        ("Angel", "Lava, ash, a demon somewhere. Remind me why we didn't go to the beach?"),
    ]),
    "Molten_2": ("RegionEntered", "region_molten", False, 600, None, [
        ("Angel", "I can taste the smoke in my teeth."),
        ("JiWoong", "Then stop talking. It helps."),
    ]),
    "Kingsfork_1": ("RegionEntered", "area_kingsfork", True, 0, None, [
        ("JiWoong", "Kingsfork. Fire to the west, frost to the east."),
        ("Angel", "Pick one and pretend we planned it."),
    ]),
    "Fern_1": ("RegionEntered", "village_fernhollow", True, 0, None, [
        ("Angel", "Snares everywhere. Watch your feet."),
        ("JiWoong", "Hunters live here. They'll know where the raiders sleep."),
    ]),
    "Brack_1": ("RegionEntered", "village_brackenford", True, 0, None, [
        ("Angel", "A ford, a lake and a lot of fish. Smells like home. Somebody's home, anyway."),
        ("JiWoong", "Quiet places are worth protecting."),
    ]),
    "Will_1": ("RegionEntered", "village_willowmere", True, 0, None, [
        ("JiWoong", "These people fled the east. Look at their hands - farmers, not fighters."),
        ("Angel", "Then we do the fighting. Fair trade."),
    ]),
    "Dusk_1": ("RegionEntered", "village_duskwatch", True, 0, None, [
        ("Angel", "A wall of stakes in the middle of a dead forest. Cheerful."),
        ("JiWoong", "Someone has held this line for a long time. Show them respect."),
    ]),
    "Empty_1": ("RegionEntered", "village_empty", True, 0, None, [
        ("Angel", "Boarded windows. Nobody left to board the doors."),
        ("JiWoong", "They left in a hurry. Whatever came, it came at night."),
    ]),
    "Empty_2": ("RegionEntered", "village_empty", False, 900, None, [
        ("JiWoong", "Another empty village."),
        ("Angel", "I hate how quiet they are."),
    ]),
    "Frostholm_1": ("RegionEntered", "village_frostholm", True, 0, None, [
        ("Angel", "Smoke, fires, people who look like they wrestle bears for fun. I like it."),
        ("JiWoong", "Mind your manners. Northerners remember insults for generations."),
    ]),
    "Pine_1": ("RegionEntered", "village_pinecrest", True, 0, None, [
        ("JiWoong", "Fresh-cut pine. A good smell."),
        ("Angel", "Smells like a winter festival. Without the festival. Or the warmth."),
    ]),
    "Hearth_1": ("RegionEntered", "village_hearthfall", True, 0, None, [
        ("Angel", "Is that steam? Is that hot water? Ji-Woong. Ji-Woong, look."),
        ("JiWoong", "I see it. We are not bathing in the town spring."),
    ]),
    "Cinderhold_1": ("RegionEntered", "village_cinderhold", True, 0, None, [
        ("JiWoong", "A town built in the volcano's shadow. Stubborn people."),
        ("Angel", "Stubborn I get. Living next to lava I don't."),
    ]),
    "Slag_1": ("RegionEntered", "village_slagford", True, 0, None, [
        ("Angel", "Everything here is either broken, burning or both."),
        ("JiWoong", "And yet they stay. There is strength in that."),
    ]),
    "Ember_1": ("RegionEntered", "village_emberrest", True, 0, None, [
        ("JiWoong", "Pilgrims. They walk toward the fire, not away from it."),
        ("Angel", "Braver than me. Or crazier. Probably both."),
    ]),
}
