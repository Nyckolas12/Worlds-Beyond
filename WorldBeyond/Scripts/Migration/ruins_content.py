"""
Worlds Beyond - the old empire's ruins, read by migrate_pass22 (Plan 5F): Persepolis-style prefabs built from the
SICKA_PERSEPOLIS kit, and the rocks and plants that grow over them. Prefab pieces are in local centimetres, the
entrance toward +X, the ground at z = 0 (floors sit a little into it so they meet uneven ground). Edit freely:
BEYOND_REBUILD_RUIN_PREFABS=1 assembles the prefabs again, BEYOND_REBUILD_RUINS=1 places the sites again.
The sites themselves (where, which prefab, facing, pads) are world_content.RUIN_SITES.
"""
import random
from collections import namedtuple

SICKA = "/Game/SICKA_PERSEPOLIS/Mesh/"
MESHES = {
    "COL_BIG": SICKA + "SM_ColumnBody_Big",
    "COL_MED": SICKA + "SM_ColumnBody_Medium",
    "COL_SMALL": SICKA + "SM_ColumnBody_Small",
    "HEAD_LION": SICKA + "SM_ColumnHeads_Lion",
    "HEAD_HOMA": SICKA + "SM_ColumnHeads_Homa",
    "HEAD_COW": SICKA + "SM_ColumnHeads_Cow",
    "GATE": SICKA + "SM_GateWall_L",
    "ARC": SICKA + "SM_WallSideArc",
    "FLOOR": SICKA + "SM_GroundPlatform",
    "TILE": SICKA + "SM_FloorSetCustom_Base",
    "DISC": SICKA + "SM_CircularPlatform",
    "PLINTH_FLOWER": SICKA + "SM_PlinthSet_Flower",
    "PLINTH_TRUNK": SICKA + "SM_PlinthSet_Trunk",
    "PLINTH": SICKA + "SM_SimplePlinth",
    "LION_KING": SICKA + "SM_WingedLionKing",
    "BASIN": SICKA + "SM_FireBasin",
    "DEMON": SICKA + "SM_PrayingDemon",
    "ENVOY": SICKA + "SM_SittingEnvoy",
    "ENVOY_GIFT": SICKA + "SM_SittingGivingEnvoy",
    "MURAL_SOLDIER": SICKA + "Murals/SM_Murals_Soldier1",
    "MURAL_SOLDIER_B": SICKA + "Murals/SM_Murals_Soldier3",
    "MURAL_FARVAHAR": SICKA + "Murals/SM_Murals_Farvahar",
    "MURAL_LIONCOW": SICKA + "Murals/SM_Murals_lionCow",
}

# Meshes the pass turns Nanite on for (the kit ships 40k-100k triangle meshes without it)
NANITE = sorted(set(MESHES.values()) | {SICKA + "SM_RockSet_1", SICKA + "SM_RockSet_2", SICKA + "SM_MountainPiece_01",
                                        SICKA + "SM_MountainPiece_04", SICKA + "SM_MountainPiece_07"})

Piece = namedtuple("Piece", "name key location yaw pitch roll scale")


def piece(name, key, x, y, z=8.0, yaw=0.0, pitch=0.0, roll=0.0, scale=(1.0, 1.0, 1.0)):
    if not isinstance(scale, tuple):
        scale = (scale, scale, scale)
    return Piece(name, key, (float(x), float(y), float(z)), float(yaw), float(pitch), float(roll), scale)


def floor_grid(nx, ny, tile=440.0, z=-12.0):
    """Ground platforms (4.4 m square, 20 cm thick) centred on the origin, their tops 8 cm above the ground."""
    out = []
    for i in range(nx):
        for j in range(ny):
            out.append(piece("Floor", "FLOOR", (i - (nx - 1) / 2.0) * tile, (j - (ny - 1) / 2.0) * tile, z,
                             yaw=90.0 * ((i * 7 + j * 3) % 4)))
    return out


def standing(r, x, y, z, broken):
    """A column at (x, y): tall, middling or a stump, or gone (None) in a broken ruin."""
    roll = r.random()
    if broken and roll < 0.3:
        return None
    if roll > (0.7 if broken else 0.4):
        key = "COL_BIG"
    elif roll > (0.45 if broken else 0.15):
        key = "COL_MED"
    else:
        key = "COL_SMALL"
    return piece("Column", key, x, y, z, yaw=r.uniform(0.0, 360.0))


def fallen(r, x, y, key="COL_MED"):
    """A column lying on its side (the shaft is 1.9 m across)."""
    return piece("Fallen", key, x, y, 95.0, yaw=r.uniform(0.0, 360.0), pitch=90.0, roll=r.uniform(-10.0, 10.0))


def debris(r, x, y):
    key = r.choice(["HEAD_LION", "HEAD_HOMA", "HEAD_COW"])
    return piece("Debris", key, x, y, 0.0, yaw=r.uniform(0.0, 360.0), pitch=r.uniform(-25.0, 25.0),
                 roll=r.choice([0.0, 90.0, -90.0]) + r.uniform(-10.0, 10.0), scale=r.uniform(1.8, 2.4))


def back_wall(x, half_span, step=440.0):
    """Arched wall pieces (83 cm thick, 4.4 m long) along y at x, facing +X."""
    out = []
    y = -half_span + step
    while y <= half_span + 1.0:
        out.append(piece("Wall", "ARC", x, y, 8.0))
        y += step
    return out


def colonnade(seed, broken):
    """A hall's two rows of columns on a paved floor, a plinth in the middle, a wall at the back."""
    r = random.Random(seed)
    out = floor_grid(4, 6)
    for side in (-1, 1):
        for k in range(5):
            column = standing(r, side * 600.0, -1000.0 + k * 500.0, 8.0, broken)
            if column is not None:
                out.append(column)
    for _ in range(3 if broken else 1):
        out.append(fallen(r, r.uniform(-300.0, 300.0), r.uniform(-900.0, 900.0), r.choice(["COL_MED", "COL_SMALL"])))
    for _ in range(4 if broken else 2):
        out.append(debris(r, r.uniform(-800.0, 800.0), r.uniform(-1200.0, 1200.0)))
    out.append(piece("Plinth", "PLINTH_FLOWER", 0.0, 0.0))
    out += back_wall(-963.0, 880.0)
    if broken:
        out.append(piece("Statue", "LION_KING", -500.0, 0.0, 0.0, yaw=r.uniform(-20.0, 20.0), roll=82.0))
    else:
        out.append(piece("Statue", "LION_KING", -650.0, 0.0, 8.0))
    return out


def gate(seed):
    """The great gate: two walls (one mirrored) on a paved square, winged lions and fire basins before it."""
    r = random.Random(seed)
    out = floor_grid(3, 3)
    out.append(piece("Gate", "GATE", -385.0, 400.0, 0.0))
    out.append(piece("Gate", "GATE", -385.0, -400.0, 0.0, scale=(1.0, -1.0, 1.0)))
    for side in (-1, 1):
        out.append(piece("Lion", "LION_KING", 750.0, side * 700.0, 0.0, yaw=0.0))
        out.append(piece("Basin", "BASIN", 600.0, side * 330.0, 8.0))
        column = standing(r, -800.0, side * 750.0, 0.0, True)
        if column is not None:
            out.append(column)
    out.append(fallen(r, -900.0, 150.0, "COL_SMALL"))
    for _ in range(3):
        out.append(debris(r, r.uniform(-600.0, 900.0), r.uniform(-900.0, 900.0)))
    return out


def terrace(seed):
    """A low platform (its top 38 cm up, a step) with columns, envoys and a relief wall at the back."""
    r = random.Random(seed)
    out = []
    for i in range(-1, 3):
        for j in range(-1, 3):
            out.append(piece("Terrace", "TILE", i * 250.0, j * 250.0, -65.0))
    for x in (-350.0, 0.0, 350.0):
        for y in (-350.0, 350.0):
            column = standing(r, x, y, 38.0, True)
            if column is not None:
                out.append(column)
    out += back_wall(-583.0, 440.0)
    for k, y in enumerate(range(-300, 301, 100)):
        out.append(piece("Relief", "MURAL_SOLDIER" if k % 2 == 0 else "MURAL_SOLDIER_B", -480.0, float(y), 38.0))
    out.append(piece("Envoy", "ENVOY", 250.0, 180.0, 38.0, yaw=180.0))
    out.append(piece("Envoy", "ENVOY_GIFT", 250.0, -180.0, 38.0, yaw=180.0))
    out.append(piece("Lion", "LION_KING", 750.0, 0.0, 0.0))
    out.append(fallen(r, 900.0, 600.0, "COL_SMALL"))
    for _ in range(2):
        out.append(debris(r, r.uniform(500.0, 1000.0), r.uniform(-700.0, 700.0)))
    return out


def shrine(seed):
    """A round platform with a fire basin, a ring of columns, kneeling and seated figures, the winged sign above."""
    r = random.Random(seed)
    out = [piece("Disc", "DISC", 0.0, 0.0, -30.0), piece("Basin", "BASIN", 0.0, 0.0, 8.0)]
    for k in range(6):
        import math
        angle = k * math.pi / 3.0
        column = standing(r, 600.0 * math.cos(angle), 600.0 * math.sin(angle), 8.0, True)
        if column is not None:
            out.append(column)
    out.append(piece("Figure", "DEMON", -420.0, 0.0, 8.0))
    out.append(piece("Envoy", "ENVOY", 300.0, 380.0, 8.0, yaw=200.0))
    out.append(piece("Envoy", "ENVOY_GIFT", 300.0, -380.0, 8.0, yaw=160.0))
    out.append(piece("Wall", "ARC", -700.0, 220.0, 8.0))
    out.append(piece("Sign", "MURAL_FARVAHAR", -605.0, 0.0, 380.0))
    out.append(piece("Plinth", "PLINTH_TRUNK", 0.0, 420.0, 8.0))
    out.append(piece("Plinth", "PLINTH_TRUNK", 0.0, -420.0, 8.0))
    out.append(debris(r, 500.0, 600.0))
    return out


def citadel(seed):
    """The acropolis: a paved court with the great gate, a hall of columns, a relief wall, lions and envoys."""
    r = random.Random(seed)
    out = floor_grid(6, 6)
    out.append(piece("Gate", "GATE", 715.0, 400.0, 0.0))
    out.append(piece("Gate", "GATE", 715.0, -400.0, 0.0, scale=(1.0, -1.0, 1.0)))
    for side in (-1, 1):
        out.append(piece("Lion", "LION_KING", 1750.0, side * 700.0, 0.0))
        out.append(piece("Basin", "BASIN", 1550.0, side * 330.0, 0.0))
        out.append(piece("Basin", "BASIN", -1100.0, side * 1100.0, 8.0))
    for x in (-900.0, -450.0, 0.0, 450.0):
        for y in (-900.0, -300.0, 300.0, 900.0):
            column = standing(r, x, y, 8.0, True)
            if column is not None:
                out.append(column)
    for _ in range(3):
        out.append(fallen(r, r.uniform(-900.0, 450.0), r.uniform(-1000.0, 1000.0), r.choice(["COL_BIG", "COL_MED"])))
    out += back_wall(-1403.0, 1320.0)
    for k, y in enumerate(range(-1100, 1101, 120)):
        out.append(piece("Relief", "MURAL_SOLDIER" if k % 2 == 0 else "MURAL_SOLDIER_B", -1300.0, float(y), 8.0))
    out.append(piece("Relief", "MURAL_LIONCOW", -1300.0, 0.0, 260.0))
    out.append(piece("Plinth", "PLINTH_FLOWER", -200.0, 0.0))
    out.append(piece("Envoy", "ENVOY", -700.0, 300.0, 8.0, yaw=90.0))
    out.append(piece("Envoy", "ENVOY_GIFT", -700.0, -300.0, 8.0, yaw=270.0))
    for _ in range(6):
        out.append(debris(r, r.uniform(-1200.0, 1300.0), r.uniform(-1300.0, 1300.0)))
    return out


PREFABS = {
    "Ruin_ColonnadeA": colonnade(11, broken=False),
    "Ruin_ColonnadeB": colonnade(23, broken=True),
    "Ruin_Gate": gate(31),
    "Ruin_Terrace": terrace(41),
    "Ruin_Shrine": shrine(53),
    "Ruin_Citadel": citadel(67),
}
# world_content.RUIN_SITES prefab name -> the prefab here
PREFAB_OF = {"ColonnadeA": "Ruin_ColonnadeA", "ColonnadeB": "Ruin_ColonnadeB", "Gate": "Ruin_Gate", "Terrace": "Ruin_Terrace",
             "Shrine": "Ruin_Shrine", "Citadel": "Ruin_Citadel"}
# How far (m) each reaches from its centre (the overgrowth and rocks start past it; unpadded sites sit at the lowest
# ground under it so no edge floats)
FOOTPRINT = {"Ruin_ColonnadeA": 16.0, "Ruin_ColonnadeB": 16.0, "Ruin_Gate": 13.0, "Ruin_Terrace": 11.0, "Ruin_Shrine": 8.5,
             "Ruin_Citadel": 21.0}

# ---------------------------------------------------------------- what grows round them
# Rock outcrops: mesh, scale (min, max), share of the height sunk into the ground
OUTCROPS = [
    (SICKA + "SM_RockSet_1", (0.6, 1.2), 0.45),
    (SICKA + "SM_RockSet_2", (0.4, 0.8), 0.45),
    (SICKA + "SM_MountainPiece_01", (0.9, 1.6), 0.35),
    (SICKA + "SM_MountainPiece_04", (0.8, 1.5), 0.2),
    (SICKA + "SM_MountainPiece_07", (0.7, 1.3), 0.35),
]
PERSEPOLIS_FOLIAGE = SICKA + "Foliage/"
GREEN = ([PERSEPOLIS_FOLIAGE + "SM_FolliageProp_%s" % n for n in ("1", "2", "3", "4", "5", "6", "2-2", "3-2", "4-2")]
         + [PERSEPOLIS_FOLIAGE + "SM_Grasspatch_Assmble0%d" % i for i in (1, 2, 3)])
MEGA_PLANTS = "/Game/Megascans/3D_Plants/"
# Overgrowth by region (Forest, Blight, Frost, Molten): meshes, scale (min, max); "IVY" is looked up by name
OVERGROWTH = {
    "region_forest": (GREEN + ["IVY"], (0.8, 1.5)),
    "region_corrupted_woods": (GREEN[:4] + ["IVY", "DEAD_BRANCHES"], (0.7, 1.2)),
    "region_frost": (GREEN[6:], (0.6, 1.0)),
    "region_molten": ([], (1.0, 1.0)),
}
IVY_NAME = "SM_CreeperIvy_03"
DEAD_BRANCHES_NAME = "SM_DeadBranches_02"
# Kept off roads (m from the edge) and clear of waystones / camps / the player start (m)
ROAD_MARGIN = 3.0
SITE_CLEARANCE = 16.0
SEED = 2026
