"""
The UI art generated with Higgsfield for Plan 6 (the source of truth for names, prompts and where each picture goes).

Generated PNGs land in Art/UI/Source/<Category>/<Name>.png (fetch_ui_art.py); migrate_pass21.py imports them into
/Game/WorldsBeyond/UI/<Folder>/<Name> and wires them up. The three mockups (uploaded as Higgsfield media) are the style
references: REF_ARMOR, REF_SKILLTREE, REF_OVERVIEW.

    python ui_art_manifest.py batch <category> <start> <count>   -> JSON requests for generate_image_batch
    python ui_art_manifest.py list                                -> every entry and whether its PNG exists
"""
import json
import os
import sys

REF_SKILLTREE = "b02a710d-2317-46c1-867a-d693389db8d6"
REF_ARMOR = "b6738390-0465-4ea0-b513-48f67feec139"
REF_OVERVIEW = "1dae56f2-eca8-43b7-9414-2c1f80289c57"

SOURCE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "Art", "UI", "Source")

ITEM_STYLE = ("Use the reference image only as a style guide: match the painted dark-fantasy JRPG item icons in its BAG grid and "
              "the large helm art in its item card (realistic painterly rendering, dramatic rim light, burnished metal, deep "
              "shadows). Single object, centered, three-quarter view, filling about 80% of the frame. Transparent background. "
              "No frame, no border, no text, no UI.")
SKILL_STYLE = ("Use the reference image only as a style guide: match the glowing symbols inside the skill-tree nodes (Storm "
               "Affinity, Arc Surge, Tempest Flow, Raging Winds). Only the glowing symbol, centered, filling about 75% of the "
               "frame, soft outer glow. Transparent background. No circle, no frame, no border, no text, no UI.")
EMBLEM_STYLE = ("Use the reference image only as a style guide: match its ornate gold emblems and glowing element and resource "
                "icons (painterly fantasy game art, burnished gold filigree, rim light). Single emblem, centered, filling about "
                "80% of the frame. Transparent background. No text, no UI.")

ANGEL = "luminous electric-blue and violet lightning energy"
JIWOONG = "radiant golden light and warm sun-fire"
DUO = "intertwined blue-violet lightning and radiant gold light"


def item(name, title, description):
    return dict(name="T_Icon_Item_" + name, category="Items", folder="Icons/Items", ref=REF_ARMOR, aspect="1:1", quality="medium",
                transparent=True, prompt="Game inventory icon of the %s: %s. %s" % (title, description, ITEM_STYLE))


def skill(name, title, description, energy):
    return dict(name="T_Icon_Skill_" + name, category="Skills", folder="Icons/Skills", ref=REF_SKILLTREE, aspect="1:1",
                quality="medium", transparent=True, prompt="Skill icon for %s: %s, %s. %s" % (title, description, energy, SKILL_STYLE))


def emblem(name, folder, description, ref=REF_OVERVIEW):
    return dict(name=name, category="Emblems", folder=folder, ref=ref, aspect="1:1", quality="medium", transparent=True,
                prompt="%s. %s" % (description, EMBLEM_STYLE))


ITEMS = [
    item("WanderersHelm", "Wanderer's Helm", "a road-worn leather hood with a small riveted steel skullcap, brown leather and grey wool"),
    item("WanderersChest", "Wanderer's Chest", "a weathered brown leather jerkin with a travel-cloak collar, buckles and stitched patches"),
    item("WanderersGauntlets", "Wanderer's Gauntlets", "a pair of worn leather bracers with fingerless gloves and brass buckles"),
    item("WanderersBoots", "Wanderer's Boots", "a pair of travel-worn tall leather boots with wrapped straps"),
    item("IronboundHelm", "Ironbound Helm", "a heavy close-faced dark iron helm with riveted bands and a narrow eye slit, battle-scarred"),
    item("IronboundChest", "Ironbound Chest", "a heavy riveted dark iron cuirass with layered plates and leather straps"),
    item("IronboundGauntlets", "Ironbound Gauntlets", "a pair of heavy dark iron plate gauntlets with riveted knuckles"),
    item("IronboundBoots", "Ironbound Boots", "a pair of iron-shod plate sabatons with greaves"),
    item("AcolytesHelm", "Acolyte's Helm", "an ivory temple cowl with a silver circlet set with a small glowing blue rune gem"),
    item("AcolytesChest", "Acolyte's Chest", "layered ivory temple robes with silver trim and embroidered glowing blue ward runes"),
    item("AcolytesGauntlets", "Acolyte's Gauntlets", "a pair of cloth-wrapped ivory gloves with silver wrist cuffs etched with faint blue runes"),
    item("AcolytesBoots", "Acolyte's Boots", "a pair of soft ivory cloth boots with silver buckles and blue rune stitching"),
    item("VenomweaveHelm", "Venomweave Helm", "a hooded mask of black serpent-scale silk with venom-green trim and a nightshade flower clasp, faint toxic green glow"),
    item("VenomweaveChest", "Venomweave Chest", "a fitted coat of black and deep green serpent-scale silk embroidered with nightshade vines, toxic green glow in the seams"),
    item("VenomweaveGauntlets", "Venomweave Gauntlets", "a pair of black serpent-scale gloves with fang-like claws and venom-green veins"),
    item("VenomweaveBoots", "Venomweave Boots", "a pair of supple black serpent-scale boots with green nightshade embroidery"),
    item("StormforgedHelm", "Stormforged Helm", "a blackened steel helm with swept-back wing crests and veins of crackling blue and violet lightning across the plates"),
    item("StormforgedChest", "Stormforged Chest", "a blackened steel breastplate with a storm-crystal core and arcs of blue and violet lightning across the plates"),
    item("StormforgedGauntlets", "Stormforged Gauntlets", "a pair of blackened steel gauntlets with crackling blue-violet lightning around the knuckles"),
    item("StormforgedBoots", "Stormforged Boots", "a pair of blackened steel boots with lightning-bolt engravings glowing blue and violet"),
    item("SunforgedHelm", "Sunforged Helm", "a gilded knight's helm of burnished gold plate with a flaring sunburst crest and dark steel visor, warm radiant golden glow"),
    item("SunforgedChest", "Sunforged Chest", "a gilded plate cuirass of burnished gold with a radiant sunburst emblem on the chest and dark steel underlayers, warm golden glow"),
    item("SunforgedGauntlets", "Sunforged Gauntlets", "a pair of gilded gold plate gauntlets with sunray engravings and dark steel fingers, warm golden glow"),
    item("SunforgedBoots", "Sunforged Boots", "a pair of gilded gold plate sabatons with sunburst knee guards, warm golden glow"),
    item("TravelersBlade", "Traveler's Blade", "a plain steel arming sword with a leather-wrapped grip and a simple crossguard, honest and well kept"),
    item("GildedSaber", "Gilded Saber", "a slender curved katana-style saber with a gilded guard and a golden-wrapped hilt, bright polished blade"),
    item("Dawnbreaker", "Dawnbreaker", "a radiant golden longsword whose blade glows like sunrise, an ornate sun-shaped guard and rays of light"),
    item("AshwoodStaff", "Ashwood Staff", "a gnarled ash-wood staff scorched by lightning with a small blue crystal bound in its crook"),
    item("StormcallersRod", "Stormcaller's Rod", "a silver rod crowned with a storm-cloud crystal crackling with blue and violet lightning"),
    item("EclipseScepter", "Eclipse Scepter", "a scepter whose head is half golden sun and half violet crescent moon on a dark metal shaft"),
    item("Emberheart", "Emberheart", "a heavy blade forged from black volcanic rock with a glowing molten ember core and cracks of lava light"),
    item("GlacierheartRod", "Glacierheart Rod", "a staff of pale steel topped with a jagged ice-crown shard radiating frost mist"),
    item("Gruxfang", "Gruxfang", "a huge curved beast fang honed into a blade, bone-white, with a leather-wrapped grip and iron rivets"),
    item("HollowVoiceScepter", "Hollow Voice Scepter", "a dark tarnished scepter topped with a hollow-eyed silver mask wreathed in ghostly violet wisps"),
    item("BandOfStatic", "Band of Static", "a silver ring set with a blue storm crystal crackling with tiny lightning arcs"),
    item("SunstoneSignet", "Sunstone Signet", "a heavy gold signet ring set with a glowing orange-gold sunstone"),
    item("RimefrostLoop", "Rimefrost Loop", "a pale ice-blue silver ring wrapped in frost crystals"),
    item("EmberglassRing", "Emberglass Ring", "a blackened gold ring set with glowing ember-red glass"),
    item("VerdantCharm", "Verdant Heartwood Charm", "a carved heartwood amulet on a cord with a glowing green leaf-shaped gem"),
    item("ShadowveilPendant", "Shadowveil Pendant", "a black silver pendant with a dark violet gem trailing wisps of shadow smoke"),
    item("EclipseTalisman", "Eclipse Talisman", "a gold and violet talisman shaped like a sun eclipsed by a crescent moon, on a fine chain"),
    item("StormglassAmulet", "Stormglass Amulet", "a blue teardrop crystal amulet in a silver cage with lightning trapped inside, on a chain"),
]

SKILLS = [
    # Abilities (their upgrade nodes use these too)
    skill("Ability_ArcaneSpikes", "Arcane Spikes", "jagged arcane crystal spikes erupting from the ground", ANGEL),
    skill("Ability_LightningBlink", "Lightning Blink", "a lightning bolt streaking forward with speed lines, a dash", ANGEL),
    skill("Ability_ArcaneBolt", "Arcane Bolt", "a glowing arcane bolt projectile with a trailing comet tail", ANGEL),
    skill("Ability_HealingLight", "Healing Light", "a soft healing sigil, a radiant four-pointed star with gentle sparkles", "pale teal and white light"),
    skill("Ability_Sunbrand", "Sunbrand", "a blazing brand mark, a sun sigil seared into the air", JIWOONG),
    skill("Ability_GildedStep", "Gilded Step", "a dashing golden afterimage footstep with radiant streaks", JIWOONG),
    skill("Ability_HeavensJudgment", "Heaven's Judgment", "a lightning bolt and a sun merging into a shockwave burst", DUO),
    skill("Ability_EclipseBrand", "Eclipse Brand", "an eclipse sigil, a dark violet moon over a golden corona, with lightning brands", DUO),
    skill("Ability_TempestAegis", "Tempest Aegis", "a round storm shield of swirling lightning with a golden rim", DUO),
    # Angel
    skill("Angel_Arcana1", "Storm Affinity", "a triangle of crackling lightning around a small storm eye", ANGEL),
    skill("Angel_Vitality1", "Thunder-Tempered", "a heart crackling with lightning, toughened", ANGEL),
    skill("Angel_Stamina", "Second Wind", "swirling wind currents around a spark, the breath of the storm", ANGEL),
    skill("Angel_Arcana2", "Eye of the Storm", "a calm glowing eye at the center of a spiraling storm", ANGEL),
    skill("Angel_Ward", "Static Ward", "a crackling hexagonal static shield", ANGEL),
    skill("Angel_Overcharge", "Overcharge", "an overloaded crystal bursting with surging lightning", ANGEL),
    skill("Angel_Vitality2", "Unbroken Spirit", "an unbreakable spirit flame wreathed in lightning", ANGEL),
    skill("Angel_Tempest", "Tempest Incarnate", "a human silhouette made of storm and lightning with arms outstretched", ANGEL),
    skill("Angel_StormsEmbrace", "Storm's Embrace", "arcs of wind and lightning curling protectively around a glowing core", ANGEL),
    skill("Angel_RagingMomentum", "Raging Momentum", "a lightning-fast chevron arrow with motion streaks and a critical spark", ANGEL),
    skill("Angel_ElementalResonance", "Elemental Resonance", "concentric resonance rings of lightning pulsing from a core", ANGEL),
    skill("Angel_KeenStatic", "Keen Static", "a sharp needle of static lightning piercing a point of light", ANGEL),
    # Ji-Woong
    skill("JiWoong_Strength1", "Gilded Edge", "a gleaming sword edge with radiant light running along the blade", JIWOONG),
    skill("JiWoong_Vitality1", "Sunlit Vigor", "a heart glowing with sunlight", JIWOONG),
    skill("JiWoong_Combo", "Sun-Forged Combo", "three sword slashes crossing in radiant arcs", JIWOONG),
    skill("JiWoong_Stamina", "Relentless", "a forward-pressing double chevron with radiant trails", JIWOONG),
    skill("JiWoong_Hex", "Hex Scholar", "a hex rune circle of arcane glyphs", JIWOONG),
    skill("JiWoong_Guard", "Bulwark", "a shield of solid light", JIWOONG),
    skill("JiWoong_Strength2", "Radiant Might", "a blazing fist gripping a sword hilt", JIWOONG),
    skill("JiWoong_Vitality2", "Unyielding", "a pillar of light planted firmly, an unbreakable stance", JIWOONG),
    skill("JiWoong_Judgment", "Radiant Judgment", "a sword descending point-first beneath a sun halo", JIWOONG),
    skill("JiWoong_RadiantZeal", "Radiant Zeal", "a rising flame of zeal", JIWOONG),
    skill("JiWoong_ExecutionersEdge", "Executioner's Edge", "a crescent executioner's blade with a critical gleam", JIWOONG),
    skill("JiWoong_SunlitMomentum", "Sunlit Momentum", "a sun with forward motion streaks", JIWOONG),
    # Duo
    skill("Duo_Kindred", "Kindred Spirits", "two intertwined spirit flames, one blue-violet and one gold", DUO),
    skill("Duo_Echo", "Lingering Bond", "a crescent moon and a star linked by a glowing thread with echoing rings", DUO),
    skill("Duo_TotalEclipse", "Total Eclipse", "a full eclipse, a black disc with a blazing corona", DUO),
    skill("Duo_EyeOfTempest", "Eye of the Tempest", "a golden eye inside a spiraling blue-violet storm", DUO),
    skill("Duo_SoulBound", "Soul-Bound", "two hearts, one blue and one gold, bound by a chain of light", DUO),
    skill("Duo_Resonance", "Resonance", "blue and gold sound-wave rings resonating together", DUO),
    skill("Duo_Concord", "Concord", "two hands, one blue and one gold, clasped in a sigil of light", DUO),
]

EMBLEMS = [
    emblem("T_Icon_Element_Storm", "Icons/Elements", "Element icon: a glowing storm rune of blue and violet lightning in a small round crest"),
    emblem("T_Icon_Element_Radiant", "Icons/Elements", "Element icon: a glowing radiant sun rune of gold light in a small round crest"),
    emblem("T_Icon_Element_Eclipse", "Icons/Elements", "Element icon: a glowing eclipse rune, a violet crescent over a gold corona, in a small round crest"),
    emblem("T_Icon_Element_Verdant", "Icons/Elements", "Element icon: a glowing verdant leaf rune of emerald green light in a small round crest"),
    emblem("T_Icon_Element_Shadow", "Icons/Elements", "Element icon: a glowing shadow rune of dark violet smoke in a small round crest"),
    emblem("T_Icon_Element_Frost", "Icons/Elements", "Element icon: a glowing frost rune, an ice-blue snowflake crystal, in a small round crest"),
    emblem("T_Icon_Element_Flame", "Icons/Elements", "Element icon: a glowing flame rune of orange fire in a small round crest"),
    emblem("T_Emblem_Set_Venomweave", "Icons/Sets", "Heraldic armor-set badge for Venomweave: a green serpent coiled around a nightshade flower on a gold-rimmed shield", REF_ARMOR),
    emblem("T_Emblem_Set_Stormforged", "Icons/Sets", "Heraldic armor-set badge for Stormforged: a lightning-struck anvil in blue and violet on a silver-rimmed shield", REF_ARMOR),
    emblem("T_Emblem_Set_Sunforged", "Icons/Sets", "Heraldic armor-set badge for Sunforged: a radiant gold sunburst on a gold-rimmed shield", REF_ARMOR),
    emblem("T_Emblem_Class_Stormcaller", "Emblems", "Class crest for the Stormcaller: an ornate gold compass-star crest with a blue lightning core, like the emblem beside STORMCALLER", REF_SKILLTREE),
    emblem("T_Emblem_Class_RadiantHexblade", "Emblems", "Class crest for the Radiant Hexblade: an ornate gold crest of a sword piercing a radiant sun with hex runes around it", REF_SKILLTREE),
    emblem("T_Emblem_Class_BondOfHeaven", "Emblems", "Class crest for the Bond of Heaven: an ornate gold crest where a crescent moon and a sun interlock, blue and gold glow", REF_SKILLTREE),
    emblem("T_Emblem_Bond", "Emblems", "Emblem for Bond Status: a glowing crescent moon of gold and blue with a small star inside, like the Bond Status emblem"),
    emblem("T_Emblem_Character", "Emblems", "Menu emblem: an ornate gold compass-star crest with fine filigree, like the emblem beside CHARACTER at the top left"),
    emblem("T_Icon_Currency_Gold", "Icons/Currency", "Currency icon for Gold: a single thick antique gold coin stamped with a four-pointed star, seen at a slight angle"),
    emblem("T_Icon_Currency_Crystals", "Icons/Currency", "Currency icon for Crystals: a single faceted glowing blue crystal"),
    emblem("T_Icon_Currency_EchoShards", "Icons/Currency", "Currency icon for Echo Shards: a violet crystal shard with faint echoing rings of light"),
    emblem("T_Icon_Currency_ReverieFragments", "Icons/Currency", "Currency icon for Reverie Fragments: a pale blue-white star-shaped fragment, dreamlike and glowing"),
    emblem("T_Icon_Currency_SkillPoint", "Icons/Currency", "Icon for a Skill Point: a gold four-pointed star sparkle with a bright core"),
    emblem("T_Icon_Currency_BondPoint", "Icons/Currency", "Icon for a Bond Point: a glowing crescent moon holding a tiny star, blue and gold"),
]

BACKDROPS = [
    dict(name="T_UI_Backdrop_Menu", category="Backdrops", folder="Backdrops", ref=REF_SKILLTREE, aspect="16:9", quality="high",
         resolution="2k", transparent=False,
         prompt=("Wide background painting for a game menu: a gothic dark-fantasy city at night seen from a ruined terrace, spires and "
                 "cathedral silhouettes, faint warm windows, drifting mist, deep navy and midnight-blue palette, subtle moonlight, "
                 "very dark and low contrast so menu panels read on top, darker toward the edges and the bottom. Match the "
                 "backdrop of the reference image. No characters, no text, no UI.")),
    dict(name="T_UI_Backdrop_MainMenu", category="Backdrops", folder="Backdrops", ref=REF_OVERVIEW, aspect="16:9", quality="high",
         resolution="2k", transparent=False,
         prompt=("Title-screen painting for the dark-fantasy game Worlds Beyond: a vast storm-lit sky over ruined gothic spires, a "
                 "crack of blue-violet lightning meeting a rising golden dawn on the horizon, mist over a forest valley below, deep "
                 "navy palette, epic and moody, open space in the center-left for a logo. No characters, no text, no UI.")),
]

ORNAMENTS = [
    dict(name="T_UI_RuneCircle", category="Ornaments", folder="Ornaments", ref=REF_ARMOR, aspect="1:1", quality="high", transparent=True,
         prompt=("A thin ornate gold arcane rune circle seen flat from the front: concentric rings, fine tick marks, small runes and an "
                 "eight-pointed star inside, delicate filigree, like the gold ring around the equipment slots in the reference. Only "
                 "the gold linework on a transparent background, the center empty. No text, no UI.")),
    dict(name="T_UI_CornerFlourish", category="Ornaments", folder="Ornaments", ref=REF_ARMOR, aspect="1:1", quality="high", transparent=True,
         prompt=("A single ornate gold filigree corner ornament for a fantasy UI panel, an L-shaped bracket with a small diamond and "
                 "curling flourishes, placed in the top-left corner of the frame, thin elegant gold lines like the panel corners in "
                 "the reference. Transparent background. No text.")),
]

QUESTS = [
    dict(name="T_Quest_Chapter1", prompt="a small forest village of timber houses at dusk, lantern light, mist between tall pines (Mossbrook)"),
    dict(name="T_Quest_Chapter2", prompt="a ring of ancient standing stones in a moonlit forest clearing, a huge pale wolf shadow among the trees"),
    dict(name="T_Quest_Chapter3", prompt="a corrupted blighted woodland with twisted black trees, violet fog and a ruined chapel"),
    dict(name="T_Quest_Chapter4", prompt="a frozen mountain pass with a broken stone fortress, blizzard, a hulking troll silhouette"),
    dict(name="T_Quest_Chapter5", prompt="a volcanic caldera with rivers of lava, ash clouds and a colossal molten silhouette"),
    dict(name="T_Quest_Chapter6", prompt="an ancient spire of dark stone rising into a fractured sky full of blue lightning and floating shards"),
    dict(name="T_Quest_Side_Forest", prompt="a sunlit forest road with an overturned merchant cart and wolf tracks"),
    dict(name="T_Quest_Side_Blight", prompt="eerie violet lights floating over a black swamp in the corrupted woods"),
    dict(name="T_Quest_Side_Frost", prompt="an icy grotto with blue crystal formations and an old cairn"),
    dict(name="T_Quest_Side_Molten", prompt="a shrine of red stone above a lava tube glowing orange"),
]
for quest in QUESTS:
    quest.update(category="Quests", folder="Quests", ref=REF_SKILLTREE, aspect="16:9", quality="medium", transparent=False,
                 prompt="Painted quest illustration for a dark-fantasy JRPG menu: %s. Moody, deep navy shadows, painterly, cinematic "
                        "lighting, matching the art style of the reference. No characters in the foreground, no text, no UI." % quest["prompt"])

CATEGORIES = {"items": ITEMS, "skills": SKILLS, "emblems": EMBLEMS, "backdrops": BACKDROPS, "ornaments": ORNAMENTS, "quests": QUESTS}
ALL = ITEMS + SKILLS + EMBLEMS + BACKDROPS + ORNAMENTS + QUESTS


def png_path(entry):
    return os.path.join(SOURCE, entry["category"], entry["name"] + ".png")


def request(entry, index):
    params = dict(model="gpt_image_2_5", prompt=entry["prompt"], aspect_ratio=entry["aspect"], resolution=entry.get("resolution", "1k"),
                  quality=entry["quality"], medias=[dict(role="image_references", value=entry["ref"])])
    if entry.get("transparent"):
        params["background"] = "transparent"
    return dict(index=index, params=params)


def main():
    if len(sys.argv) >= 2 and sys.argv[1] == "list":
        for entry in ALL:
            print("%-48s %-10s %s" % (entry["name"], entry["category"], "ok" if os.path.exists(png_path(entry)) else "-"))
        return
    if len(sys.argv) >= 5 and sys.argv[1] == "batch":
        entries = [entry for entry in CATEGORIES[sys.argv[2]] if not os.path.exists(png_path(entry))]
        start, count = int(sys.argv[3]), int(sys.argv[4])
        chosen = entries[start:start + count]
        print(json.dumps([request(entry, start + i) for i, entry in enumerate(chosen)]))
        print(json.dumps({str(start + i): entry["name"] for i, entry in enumerate(chosen)}))
        return
    print(__doc__)


if __name__ == "__main__":
    main()
