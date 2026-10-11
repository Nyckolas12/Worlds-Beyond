"""
Worlds Beyond - the village kit (Plan 5C): houses, sheds and stalls assembled from the modular pieces on hand (the
Megascans medieval walls, the village sample's roofs and props). Used by migrate_pass17.py; pure layout code, the
caller spawns the pieces.

What the pieces are (measured, see Docs/Plans/05_Open_World.md):
- the Megascans walls / doors are 2 m tall, 1 / 1.5 / 2 / 3 m long, pivot bottom-left, running along +X, one-sided
  with their face toward +Y (thickness toward -Y);
- the corner is an L with 1 m arms along +X and -Y, its faces outward (+Y and -X);
- Gable4 is a 4.8 m triangle 2.2 m tall (42 degrees), face +Y, its base 0.28 m under its pivot;
- House07_Roof02 is a gable roof 5.2 m across (ridge along its Y, centre at (176, 216)), 6.8 m long, hipped at its -Y
  end, so roofs are two copies end to end with the hipped ends overlapping in the middle;
- the chimney (Furnace_ChimneyAddon) stands 1.8 m tall from 1.47 m above its pivot.

A house is built around the origin with its door facing +X, at SCALE (1.25: 2.5 m storeys and doors). Sizes in a
recipe are in kit metres (before the scale): width along the front (Y), depth (X).
"""
import math
import random

SCALE = 1.25

MESHES = {
    # walls by length (kit metres)
    "W3": "/Game/Megascans/3D_Assets/MedievalModularWall3x2M/SM_MedievalModularWall3x2M",
    "W2D": "/Game/Megascans/3D_Assets/MedievalModularWall2x2MD/SM_MedievalModularWall2x2MD",
    "W2G": "/Game/Megascans/3D_Assets/MedievalModularWall2x2MG/SM_MedievalModularWall2x2MG",
    "W2H": "/Game/Megascans/3D_Assets/MedievalModularWall2x2MH/SM_MedievalModularWall2x2MH",
    "W15": "/Game/Megascans/3D_Assets/MedievalModularWall15x2M/SM_MedievalModularWall15x2M",
    "W1A": "/Game/Megascans/3D_Assets/MedievalModularWall1x2MA/SM_MedievalModularWall1x2MA",
    "W1B": "/Game/Megascans/3D_Assets/Medieval_Modular_Wall/SM_MedievalModularWall",
    "D1": "/Game/Megascans/3D_Assets/MedievalModularDoor1x2M/SM_MedievalModularDoor1x2M",
    "D15": "/Game/Megascans/3D_Assets/MedievalModularDoor15x2M/SM_MedievalModularDoor15x2M",
    "D2": "/Game/Megascans/3D_Assets/MedievalModularDoor2x2MA/SM_MedievalModularDoor2x2MA",
    "D3": "/Game/Megascans/3D_Assets/MedievalModularDoor3x2M/SM_MedievalModularDoor3x2M",
    "CORNER": "/Game/Megascans/3D_Assets/MedievalModularCornerWall1x2M/SM_MedievalModularCornerWall1x2M",
    "GABLE": "/Game/Megascans/3D_Assets/MedievalModularGable4/SM_MedievalModularGable4",
    "ROOF": "/Game/Meshes/Houses/Roofs/House07_Roof/SM_House07_Roof02",
    "ROOF_SMALL": "/Game/Meshes/Houses/Roofs/House07_Roof/SM_House07_RoofSmall",
    "ROOF_BURNT": "/Game/Meshes/Houses/MODULAR_ASSETS/Forge/Roof_L/SM_RoofConst_Small",
    "CHIMNEY": "/Game/Meshes/Furnace/SM_Furnace_ChimneyAddon",
    "CANOPY": "/Game/Meshes/Houses/MODULAR_ASSETS/SM_SmallOutcrop",
    "SHED": "/Game/Meshes/Houses/MODULAR_ASSETS/SM_Outcrop",
    "SHED_OPEN": "/Game/Meshes/Houses/MODULAR_ASSETS/SM_OutcropOpen",
    "BOARDS": "/Game/Meshes/Houses/MODULAR_ASSETS/SM_BoardedWindow",
    "SHUTTER": "/Game/Meshes/Houses/MODULAR_ASSETS/WindowShutter/SM_WindowOpen_R",
    "PLINTH": "/Game/Megascans/3D_Assets/CastleWall/SM_CastleWall",
    "BEAM": "/Game/Meshes/Houses/MODULAR_ASSETS/HorizontalBeams/SM_HBeam_MS_01",
    # props
    "BARREL": "/Game/Megascans/3D_Assets/WoodenBarrelA/SM_WoodenBarrelA",
    "BOX": "/Game/Megascans/3D_Assets/WoodenBox/SM_WoodenBox",
    "BUCKET": "/Game/Megascans/3D_Assets/OldWoodenBucketA/SM_OldWoodenBucketA",
    "WHEEL": "/Game/Megascans/3D_Assets/WoodenWheelA/SM_WoodenWheelA",
    "CHURN": "/Game/Megascans/3D_Assets/MedievalButterChurn/SM_MedievalButterChurn",
    "CHAINS": "/Game/Megascans/3D_Assets/PileOfChains/SM_PileOfChains",
    "POST": "/Game/Megascans/3D_Assets/WoodenPost/SM_WoodenPost",
    "STRUT": "/Game/Megascans/3D_Assets/PalisadeStrutB/SM_PalisadeStrutB",
    "LOG": "/Game/Megascans/3D_Assets/WornWoodenBeamA/SM_WornWoodenBeamA_00",
    "FENCE": "/Game/Meshes/Fences/Fence01/SM_Fenc01_P1",
    "FENCE_END": "/Game/Meshes/Fences/Fence01/SM_Fence01_End",
    "FENCE_BROKEN": "/Game/Meshes/Fences/Fence01/SM_Fence01_Dmg",
    "PICKETS": "/Game/Meshes/Houses/MODULAR_ASSETS/SM_HouseFence_01",
    "LANTERN": "/Game/Meshes/Lantern/SM_ManMade_Lantern_01",
    "TALISMAN": "/Game/Meshes/Talismans/SM_Talisman_01",
    "STONE_WALL": "/Game/Megascans/3D_Assets/MossyStoneWallC/SM_MossyStoneWallC_03",
}

WALLS = {3.0: ["W3"], 2.0: ["W2D", "W2G", "W2H"], 1.5: ["W15"], 1.0: ["W1A", "W1B"]}
DOORS = {1.0: "D1", 1.5: "D15", 2.0: "D2", 3.0: "D3"}
# Window centres along each wall piece (kit metres from its start) and height, for boards on abandoned houses
WINDOWS = {"W3": [0.75, 2.25], "W2G": [0.55], "W2H": [1.0], "W15": [0.75], "W1A": [0.5]}
WINDOW_HEIGHT = 1.45

STOREY = 2.0           # kit metres
GABLE_WIDTH = 4.82     # kit metres, its base
GABLE_RISE = 2.18
GABLE_BASE = 0.278     # under its pivot
ROOF_SPAN = 5.227
ROOF_LENGTH = 6.773
ROOF_CENTRE = (1.764, 2.162)
ROOF_TOP = 1.288       # ridge above its pivot
ROOF_THICK = 0.28      # under the ridge, measured on screen
ROOF_PART = 0.8        # each of the two copies covers this much of the length (its hipped 30 % under the other)
CHIMNEY_BASE = 1.471
CHIMNEY_HEIGHT = 1.81

# The house types: width (front), depth, storeys, ridge ("side": along the front, gables left and right; "front": the
# gable over the door), door size, porch canopy, chimney, lean-to shed, stone plinth
RECIPES = {
    "cottage_small": dict(width=5, depth=4, storeys=1, ridge="side", door=1.0, porch=False, chimney=True, shed=False, plinth=True),
    "cottage": dict(width=6, depth=5, storeys=1, ridge="side", door=1.5, porch=True, chimney=True, shed=False, plinth=True),
    "gable_house": dict(width=5, depth=6, storeys=1, ridge="front", door=2.0, porch=True, chimney=True, shed=False, plinth=True),
    "town_house": dict(width=6, depth=5, storeys=2, ridge="side", door=1.5, porch=False, chimney=True, shed=False, plinth=True),
    "longhouse": dict(width=9, depth=5, storeys=1, ridge="side", door=2.0, porch=True, chimney=True, shed=False, plinth=True),
    "workshop": dict(width=6, depth=6, storeys=1, ridge="front", door=3.0, porch=False, chimney=True, shed=True, plinth=True),
    "tower_house": dict(width=4, depth=4, storeys=2, ridge="front", door=1.0, porch=False, chimney=False, shed=False, plinth=True),
}


class Piece(object):
    """One thing to spawn: kit key (or "LIGHT"), location (cm), yaw / pitch / roll (deg), scale (x, y, z), label suffix."""
    __slots__ = ("key", "location", "yaw", "scale", "name", "pitch", "roll")

    def __init__(self, key, location, yaw, scale, name, pitch=0.0, roll=0.0):
        self.key, self.location, self.yaw, self.scale, self.name = key, location, yaw, scale, name
        self.pitch, self.roll = pitch, roll


def _sides(width, depth):
    """The footprint's sides walked so each piece's face (+Y) points out: (start (x, y) m, direction, outward normal, length)."""
    hx, hy = depth / 2.0, width / 2.0
    return [
        ((hx, hy), (0.0, -1.0), (1.0, 0.0), width),     # front (+X), the door side
        ((hx, -hy), (-1.0, 0.0), (0.0, -1.0), depth),   # right (-Y)
        ((-hx, -hy), (0.0, 1.0), (-1.0, 0.0), width),   # back (-X)
        ((-hx, hy), (1.0, 0.0), (0.0, 1.0), depth),     # left (+Y)
    ]


def _yaw(direction):
    return math.degrees(math.atan2(direction[1], direction[0]))


def _fill(length, rng):
    """Wall lengths that add up to length exactly (a multiple of 0.5 m, 0 or at least 1)."""
    pieces = []
    while length > 0.25:
        options = [size for size in WALLS if size <= length + 1e-6 and (abs(length - size) < 1e-6 or length - size >= 1.0 - 1e-6)]
        # Mostly the long pieces, now and then the short ones
        weights = [size * size for size in options]
        size = rng.choices(options, weights=weights)[0]
        pieces.append(size)
        length -= size
    rng.shuffle(pieces)
    return pieces


def _split_front(fill, door, rng):
    """Wall lengths left and right of the door on a front of fill metres."""
    rest = fill - door
    if rest < 0.25:
        return 0.0, 0.0
    left = round(rest / 2.0 * 2.0) / 2.0
    if rng.random() < 0.5:
        left = math.floor(rest / 2.0 * 2.0) / 2.0
    right = rest - left
    # 0.5 m left over on one side can't be filled: shift it
    if 0.25 < left < 0.99:
        left, right = 0.0, rest
    if 0.25 < right < 0.99:
        left, right = rest, 0.0
    return left, right


def house(recipe, seed=0, boarded=False, scale=SCALE, bounds=None):
    """The pieces of a house (list of Piece), its door facing +X. bounds: {key: ((min x, y, z), (max x, y, z))} cm."""
    bounds = bounds or {}
    rng = random.Random(seed)
    r = dict(recipe)
    width, depth, storeys = float(r["width"]), float(r["depth"]), int(r["storeys"])
    unit = 100.0 * scale
    pieces = []

    def put(key, x, y, z, yaw, sx=1.0, sy=1.0, sz=1.0, name=None):
        pieces.append(Piece(key, (x * unit, y * unit, z * unit), yaw, (sx * scale, sy * scale, sz * scale), name or key))

    sides = _sides(width, depth)
    for storey in range(storeys):
        z = storey * STOREY
        for index, (start, direction, normal, length) in enumerate(sides):
            yaw = _yaw(direction)
            put("CORNER", start[0], start[1], z, yaw, name="Corner")
            fill = length - 2.0
            if index == 0 and storey == 0:
                left, right = _split_front(fill, r["door"], rng)
                runs = [("wall", left), ("door", r["door"]), ("wall", right)]
            else:
                runs = [("wall", fill)]
            t = 1.0
            for kind, run in runs:
                if kind == "door":
                    sizes = [run]
                else:
                    sizes = _fill(run, rng)
                for size in sizes:
                    if kind == "door":
                        key = DOORS[size]
                    else:
                        choices = WALLS[size]
                        # Upstairs and on the back: more windows
                        key = rng.choice(choices)
                    x, y = start[0] + direction[0] * t, start[1] + direction[1] * t
                    put(key, x, y, z, yaw, name="Door" if kind == "door" else "Wall")
                    if boarded:
                        for centre in WINDOWS.get(key, []):
                            bx = x + direction[0] * centre + normal[0] * 0.06
                            by = y + direction[1] * centre + normal[1] * 0.06
                            put("BOARDS", bx, by, z + WINDOW_HEIGHT, yaw + 90.0, 0.55, 0.55, 0.65, name="Boards")
                    t += size

    top = storeys * STOREY
    ridge_along_y = r["ridge"] == "side"
    span = depth if ridge_along_y else width
    length = width if ridge_along_y else depth

    # Gables on the two sides across the ridge
    gable_scale = span / GABLE_WIDTH
    for index, (start, direction, normal, side_length) in enumerate(sides):
        across = (index in (1, 3)) if ridge_along_y else (index in (0, 2))
        if not across:
            continue
        mid = (start[0] + direction[0] * side_length / 2.0, start[1] + direction[1] * side_length / 2.0)
        # Its triangle is centred 7 cm right of the pivot
        offset = -0.07 * gable_scale
        put("GABLE", mid[0] + direction[0] * offset, mid[1] + direction[1] * offset, top, _yaw(direction),
            gable_scale, 1.0, gable_scale, name="Gable")

    # The roof: two copies end to end, each hipped end turned inward under the other copy, the ridge on the gables' apex
    overhang, end_overhang = 0.35, 0.3
    roof_sx = (span + 2.0 * overhang) / ROOF_SPAN
    roof_sz = roof_sx
    total = length + 2.0 * end_overhang
    part = total * ROOF_PART
    roof_sy = part / ROOF_LENGTH
    apex = top + (GABLE_RISE - GABLE_BASE) * gable_scale
    roof_z = apex + ROOF_THICK * roof_sz - ROOF_TOP * roof_sz
    ridge = (0.0, 1.0) if ridge_along_y else (1.0, 0.0)
    for end, lift in ((-1.0, 0.0), (1.0, 0.016)):
        # The mesh's hipped end is its -Y end: (sin yaw, -cos yaw) points it toward the middle
        toward = (-end * ridge[0], -end * ridge[1])
        yaw = math.degrees(math.atan2(toward[0], -toward[1]))
        along = end * (total - part) / 2.0
        # The mesh's own centre is off its pivot: turn the offset with the copy
        cx, cy = ROOF_CENTRE[0] * roof_sx * scale, ROOF_CENTRE[1] * roof_sy * scale
        rad = math.radians(yaw)
        ox = cx * math.cos(rad) - cy * math.sin(rad)
        oy = cx * math.sin(rad) + cy * math.cos(rad)
        px, py = ridge[0] * along, ridge[1] * along
        put("ROOF", px - ox / scale, py - oy / scale, roof_z + lift, yaw, roof_sx * (1.0 + lift * 0.4), roof_sy, roof_sz, name="Roof")

    if r.get("chimney") and not boarded:
        # Through the back slope, near one end
        if ridge_along_y:
            cx, cy = -span * 0.22, length * 0.28
        else:
            cx, cy = -length * 0.28, span * 0.22
        ridge_top = roof_z + ROOF_TOP * roof_sz
        sz = (ridge_top + 0.5 - top) / CHIMNEY_HEIGHT
        put("CHIMNEY", cx, cy, top - CHIMNEY_BASE * sz, 0.0, 1.0, 1.0, sz, name="Chimney")

    if r.get("porch"):
        # The canopy over the door, its high side (-Y) on the wall
        back = -bounds["CANOPY"][0][1] / 100.0 if "CANOPY" in bounds else 0.5
        put("CANOPY", depth / 2.0 + back + 0.02, 0.0, 0.0, -90.0, name="Porch")

    if r.get("shed"):
        put("SHED_OPEN" if not boarded else "SHED", 0.0, -(width / 2.0 + 1.25), 0.0, 0.0, name="Shed")

    if r.get("plinth"):
        # A low stone footing round the walls, half in the ground
        for start, direction, normal, side_length in sides:
            mid = (start[0] + direction[0] * side_length / 2.0, start[1] + direction[1] * side_length / 2.0)
            put("PLINTH", mid[0] + normal[0] * 0.05, mid[1] + normal[1] * 0.05, -0.12, _yaw(direction),
                (side_length + 0.3) / 3.61, 0.45, 1.2, name="Plinth")
    return pieces


def footprint(recipe, scale=SCALE):
    """Half sizes (cm) of the ground a house takes, porch and shed included: (front +X, back -X, half width)."""
    unit = 100.0 * scale
    front = recipe["depth"] / 2.0 + (1.6 if recipe.get("porch") else 0.4)
    back = recipe["depth"] / 2.0 + 0.4
    half = recipe["width"] / 2.0 + (2.8 if recipe.get("shed") else 0.4)
    return front * unit, back * unit, half * unit


# ---------------------------------------------------------------- the smaller things (centimetres, scale 1)

def _p(key, x, y, z, yaw=0.0, scale=(1.0, 1.0, 1.0), name=None, pitch=0.0, roll=0.0):
    return Piece(key, (x, y, z), yaw, scale, name or key, pitch, roll)


def stall(seed=0):
    """A market stall: the shingled canopy on posts, crates and barrels under it; open side toward +X."""
    rng = random.Random(seed)
    pieces = [_p("CANOPY", 0.0, 0.0, 0.0, -90.0, (1.1, 1.6, 1.0), "Canopy")]
    for i in range(rng.randint(3, 5)):
        key = rng.choice(["BOX", "BOX", "BARREL", "BUCKET", "CHURN"])
        pieces.append(_p(key, rng.uniform(-40.0, 30.0), rng.uniform(-110.0, 110.0), 0.0, rng.uniform(0.0, 360.0), name="Goods"))
    # A crate on a crate
    pieces.append(_p("BOX", -20.0, rng.uniform(-60.0, 60.0), 45.0, rng.uniform(0.0, 40.0), name="Goods"))
    return pieces


def lantern_post():
    """A tall post with an arm and a hanging lantern (and its light), the lantern toward +X."""
    return [
        _p("POST", 0.0, 0.0, -20.0, 0.0, (1.3, 1.3, 2.1), "Post"),
        # The beam lying across the top, sticking out toward the lantern
        _p("LOG", 45.0, 0.0, 285.0, 0.0, (0.55, 0.55, 0.45), "Arm", pitch=90.0),
        _p("LANTERN", 95.0, 0.0, 278.0, 0.0, (1.3, 1.3, 1.3), "Lantern"),
        _p("LIGHT", 95.0, 0.0, 230.0, 0.0, (1.0, 1.0, 1.0), "Light"),
    ]


def palisade_segment(seed=0, length=340.0):
    """A run of stakes along Y (centred), sharpened tops uneven, with a beam behind (-X)."""
    rng = random.Random(seed)
    pieces = []
    count = 12
    for i in range(count):
        y = -length / 2.0 + (i + 0.5) * length / count
        height = rng.uniform(1.25, 1.45)
        pieces.append(_p("LOG", rng.uniform(-4.0, 4.0), y, 128.0 * height - 40.0, rng.uniform(0.0, 360.0),
                         (1.45, 1.45, height), "Stake", roll=rng.uniform(-2.5, 2.5)))
    for z in (90.0, 230.0):
        pieces.append(_p("LOG", -22.0, 0.0, z, 90.0, (0.7, 0.7, length / 256.0 * 1.04), "Rail", pitch=90.0))
    return pieces


def gate(opening):
    """Two posts either side of a road along X and a beam over it, a lantern and charms hanging; opening in cm."""
    half = opening / 2.0 + 40.0
    pieces = []
    for side in (-1.0, 1.0):
        pieces.append(_p("POST", 0.0, side * half, -30.0, 0.0, (1.7, 1.7, 2.6), "GatePost"))
    span = (2.0 * half + 80.0) / 256.0
    pieces.append(_p("LOG", 0.0, 0.0, 365.0, 90.0, (0.9, 0.9, span), "GateBeam", pitch=90.0))
    pieces.append(_p("LANTERN", 0.0, 0.0, 352.0, 0.0, (1.4, 1.4, 1.4), "GateLantern"))
    pieces.append(_p("LIGHT", 0.0, 0.0, 300.0, 0.0, (1.0, 1.0, 1.0), "GateLight"))
    for side in (-0.55, 0.55):
        pieces.append(_p("TALISMAN", 0.0, side * half, 352.0, 90.0, (1.0, 1.0, 1.0), "Charm"))
    return pieces


def yard_props(rng, kind="busy"):
    """A small cluster by a wall (the wall along Y at x = 0, the yard toward +X)."""
    pieces = []
    if kind == "empty":
        for _ in range(rng.randint(1, 3)):
            key = rng.choice(["BARREL", "BOX", "BUCKET", "CHAINS"])
            # Knocked over
            pieces.append(_p(key, rng.uniform(40.0, 160.0), rng.uniform(-150.0, 150.0), 18.0 if key != "CHAINS" else 0.0,
                             rng.uniform(0.0, 360.0), name="Junk", roll=90.0 if key in ("BARREL", "BUCKET") else 0.0))
        return pieces
    choice = rng.random()
    if choice < 0.4:
        # Barrels against the wall
        for i in range(rng.randint(2, 3)):
            pieces.append(_p(rng.choice(["BARREL", "BARREL", "BUCKET"]), 35.0 + rng.uniform(0.0, 15.0), -60.0 + i * 45.0, 0.0,
                             rng.uniform(0.0, 360.0), name="Barrel"))
    elif choice < 0.7:
        # A woodpile: logs lying along the wall
        for i in range(rng.randint(3, 5)):
            pieces.append(_p("LOG", 40.0 + (i % 2) * 22.0, rng.uniform(-10.0, 10.0), 10.0 + (i // 2) * 19.0, 90.0,
                             (1.0, 1.0, 0.75), "Woodpile", pitch=90.0))
    elif choice < 0.85:
        pieces.append(_p("WHEEL", 30.0, rng.uniform(-80.0, 80.0), 0.0, 90.0 + rng.uniform(-10.0, 10.0), name="Wheel", roll=-12.0))
        pieces.append(_p("BOX", 70.0, rng.uniform(-80.0, 80.0), 0.0, rng.uniform(0.0, 360.0), name="Crate"))
    else:
        pieces.append(_p("CHURN", 40.0, rng.uniform(-60.0, 60.0), 0.0, rng.uniform(0.0, 360.0), name="Churn"))
        pieces.append(_p("BUCKET", 70.0, rng.uniform(-60.0, 60.0), 0.0, rng.uniform(0.0, 360.0), name="Bucket"))
    return pieces


def garden(width, depth, broken=False, rng=None):
    """A fenced plot (width along Y, depth along +X from x = 0), a gap in its far side; cm."""
    rng = rng or random.Random(0)
    piece_length = 270.0
    pieces = []
    key = "FENCE_BROKEN" if broken else "FENCE"
    # (start, direction, length) of each side, walked round
    sides = [((0.0, -width / 2.0), (1.0, 0.0), depth), ((depth, -width / 2.0), (0.0, 1.0), width),
             ((depth, width / 2.0), (-1.0, 0.0), depth)]
    for start, direction, length in sides:
        count = max(1, int(round(length / piece_length)))
        step = length / count
        for i in range(count):
            # The middle of the far side is the way in
            if direction == (0.0, 1.0) and count > 1 and i == count // 2:
                continue
            x = start[0] + direction[0] * (i * step + 17.0)
            y = start[1] + direction[1] * (i * step + 17.0)
            pieces.append(_p(key if rng.random() > 0.15 else "FENCE", x, y, 0.0, _yaw(direction),
                             (step / piece_length, 1.0, 1.0), "Fence"))
    return pieces
