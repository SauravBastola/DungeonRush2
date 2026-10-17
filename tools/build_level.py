#!/usr/bin/env python3
"""
DungeonRush2 level generator.

Single source of truth for the world geometry. Emits:
  - assets/tileset/ForegroundLayer.map  (terrain, -11 for sky)
  - assets/tileset/BackgroundLayer.map  (parallax filler)
  - assets/tileset/DecorLayer.map       (sparse props: rocks, crystals, lamps)
  - assets/tileset/CoinLayer.map        (0/1 per tile)
  - assets/tileset/Colliders.txt        (one `x y w h` rect per line, tile units)
  - src/Level.h                         (C++ spawn data, tile units)

All coordinates inside the generator are in TILE units. Colliders are
auto-derived from the terrain heightmap + explicit platform rects so
visuals and physics can never disagree.

PLAYABILITY
-----------
The level used to be mathematically unplayable: it asked the hero to climb
2- and 4-tile walls and clear 4- and 5-tile gaps.  Those numbers are not
matters of taste, they come straight out of the physics in Config.h:

    jump height  = v^2 / 2g   = 620^2 / 4000       =  96 px  = 1.50 tiles
    jump length  = speed * 2v/g = 280 * 0.62 s     = 174 px  = 2.71 tiles

so anything steeper than ONE tile up, or wider than TWO tiles across, is a
wall.  Every obstacle below is generated against those two limits and then
re-verified by `validate_level()`, which walks the heightmap and refuses to
emit a level that cannot be finished.
"""
import math
import os
import sys
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Tuple

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Object ids, parsed from src/Objects.h so the two generators cannot drift.
OBJ_IDS: Dict[str, int] = {}

# ---------------------------------------------------------------------------
# Traversal limits (see module docstring for the derivation)
#
# These are READ OUT OF Config.h rather than copied.  The level used to drift
# out of sync with the physics — the generator happily emitted 2-tile walls
# for a hero that could only clear 1.5 tiles — so the numbers now have
# exactly one home and this script refuses to emit an impossible level.
# ---------------------------------------------------------------------------
import re

import build_terrain as BT   # terrain_v2.png ids live there (ART-10)
import build_props as BP     # props_v2.png ids live there (Batch A)

CONFIG_H = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                        "src", "Config.h")


def _read_config_float(name: str, default: float) -> float:
    try:
        with open(CONFIG_H) as f:
            txt = f.read()
        m = re.search(rf"constexpr\s+float\s+{name}\s*=\s*(-?[0-9.]+)f", txt)
        return float(m.group(1)) if m else default
    except OSError:
        return default


TILE         = 64
JUMP_VEL     = abs(_read_config_float("PLAYER_JUMP_VEL", 900.0))
GRAVITY      = _read_config_float("PLAYER_GRAVITY", 2000.0)
RUN_SPEED    = _read_config_float("PLAYER_SPEED", 280.0)

JUMP_HEIGHT  = (JUMP_VEL ** 2) / (2 * GRAVITY)          # px
JUMP_LENGTH  = RUN_SPEED * (2 * JUMP_VEL / GRAVITY)     # px

# Leave a little margin rather than aiming at the exact limit: a jump that
# only just clears a ledge feels bad and breaks the moment anything is tuned.
MAX_STEP_UP  = max(1, int(JUMP_HEIGHT // TILE) - 1)   # tiles the hero can climb
MAX_GAP      = max(1, int(JUMP_LENGTH // TILE) - 1)   # tiles the hero can clear

# ---------------------------------------------------------------------------
# Tile IDs — platformer.png, 9 cols x 5 rows of 64 px (45 cells, 36 usable)
#
# Indices are row * 9 + col.  Every entry below was established by measuring
# the PNG: per-cell fill fraction, the opaque-pixel count on each of the four
# edges, the dominant palette colour, and the shape of the alpha silhouette.
#
# The previous table was guessed from colour alone and got several tiles
# badly wrong -- it called tile 40 a "crystal" and tile 41 a "chest" when they
# are the BOTTOM halves of the two trees, and it called 31 a "plant" when it
# is the TOP half of the green tree.  That is how the level ended up strewn
# with severed tree parts standing in for crystals.
#
# The sheet's one hard constraint, measured directly:
#
#   Only tiles 2, 5 and 8 have all four edges 64/64 opaque.  They are the
#   ONLY cells that can be repeated without leaving a seam.  Everything else
#   carries transparent margins -- and because platformer.png stores the RGB
#   (255,0,64) in all 69271 of its transparent pixels, a non-seamless tile
#   used as terrain fill paints a magenta square wherever it fails to cover.
#   (Verified: blitting tile 31 with SDL_BLENDMODE_NONE puts 2930 magenta
#   pixels on screen; with BLEND it composites correctly.)
#
# So the terrain is built from 2/5/8 as fill with purpose-drawn tiles at the
# boundaries, and ALL of the interesting art goes in the decor layer, which
# draws on top of already-solid ground where transparency is free.
# ---------------------------------------------------------------------------

EMPTY = -11                        # sentinel matching the existing loader

# --- seamless fills: the only three repeatable cells -----------------------
FILL_TEAL = 8                      # 99.5% opaque, teal
FILL_SOIL = 2                      # 99.8% opaque, dark brown subsoil
FILL_NAVY = 5                      # 99.5% opaque, deep blue

# --- teal terrain: a CORNER set, not a left/mid/right slice ---------------
#
# Measured edge profiles (opaque pixels out of 64):
#     6:  T 9  B61  L 9  R61     grass fringe along TOP and LEFT
#     7:  T 9  B55  L61  R 9     grass fringe along TOP and RIGHT
#    15:  T61  B 9  L 3  R55     grass fringe along the BOTTOM, left corner
#    16:  T55  B11  L55  R 7     grass fringe along the BOTTOM, right corner
#
# 6/7 are therefore the exposed TOP corners of a cliff and 15/16 the exposed
# BOTTOM corners; the left/right values are what actually join to the fill.
TEAL_TOP    = (6,  FILL_TEAL, 7)   # walkable surface, grass on top
TEAL_BOTTOM = (15, FILL_TEAL, 16)  # underside, fringe hangs down
TEAL_BODY   = TEAL_TOP             # rows between them are plain fill

SOIL_SET    = (0,  FILL_SOIL, 1)   # 0/1 subsoil caps, 2 fill
SOIL_ALT    = (9,  FILL_SOIL, 10)  # second subsoil variant (T60/T52, banded)
STONE_SET   = (18, FILL_SOIL, 19)  # near-black deep stone
STONE_WALL  = 20                   # seamless HORIZONTALLY (L64 R64), T35 B39
NAVY_SET    = (3,  FILL_NAVY, 4)
NAVY_BOTTOM = (12, FILL_NAVY, 13)

# --- water -----------------------------------------------------------------
WATER_SET   = (36, 28, 37)         # 28 bright surface, 36/37 rounded ends
WATER_BRIGHT = 28                  # the lit band just under the surface
WATER_SHORE = 27                   # water meeting ground, left-weighted

# --- props (transparent margins: DECOR LAYER ONLY) -------------------------
BUSH         = 30                  # teal mound with plants growing up (B64)
TREE_G_TOP, TREE_G_BOT = 31, 40    # green tree, 2 cells tall
TREE_A_TOP, TREE_A_BOT = 32, 41    # autumn tree, 2 cells tall
BOULDER_SOIL = 11                  # brown round rock
BOULDER_NAVY = 14                  # deep-blue round rock (reads as wet)
BOULDER_TEAL = 17                  # teal round rock
CRYSTAL      = 22                  # slate crystal cluster + red-berried branch
SHARD        = 23                  # small crystal shard
LEDGE_ARM    = 21                  # ledge jutting right, attached on the left
GRASS_TUFT   = 29                  # teal block, plants along its right side
CHUNK_TEAL   = 38                  # chunky teal block, rounded top (B64)
CHUNK_TEAL_B = 39                  # variant with a notch on the right
CHUNK_WATER  = 36                  # chunky water block
CHUNK_WATER_B = 37                 # variant with a notch on the right

# Every distinct cell the sheet offers, for the coverage check in main().
# Duplicates are collapsed: several tiles legitimately appear in more than one
# set (8 is both the teal fill and the middle of TEAL_TOP, 36/37 are both the
# water end caps and the chunky water blocks).
ALL_USABLE_TILES = sorted(set(
    [FILL_TEAL, FILL_SOIL, FILL_NAVY, WATER_BRIGHT] +
    list(TEAL_TOP) + list(TEAL_BOTTOM) +
    list(SOIL_SET) + list(SOIL_ALT) + list(STONE_SET) + [STONE_WALL] +
    list(NAVY_SET) + list(NAVY_BOTTOM) + list(WATER_SET) + [WATER_SHORE] +
    [BUSH, TREE_G_TOP, TREE_G_BOT, TREE_A_TOP, TREE_A_BOT,
     BOULDER_SOIL, BOULDER_NAVY, BOULDER_TEAL, CRYSTAL, SHARD, LEDGE_ARM,
     GRASS_TUFT, CHUNK_TEAL, CHUNK_TEAL_B, CHUNK_WATER, CHUNK_WATER_B]))

# ---------------------------------------------------------------------------
# Level — "The Sunken Path"
# ---------------------------------------------------------------------------
WIDTH  = 200
HEIGHT = 36
GROUND_ROW = 29   # baseline: the last EMPTY row above the surface


@dataclass
class EnemySpawn:
    kind: str
    col: int
    row: Optional[int] = None
    stands_on: str = "ground"


@dataclass
class Powerup:
    kind: str
    col: int
    row: Optional[int] = None


@dataclass
class Platform:
    col: int
    row: int
    w: int
    h: int
    moving: bool = False
    range: int = 0
    speed: float = 0.0
    vertical: bool = False


@dataclass
class Hazard:
    kind: str
    col: int
    w: int
    h: int
    row: Optional[int] = None


@dataclass
class Facility:
    kind: str
    col: int
    row: Optional[int] = None


@dataclass
class SceneryItem:
    """A whole prop placed in the world.

    `kind` names an entry in tools/build_objects.py.  These are assembled
    objects, not tile ids: the two trees on the sheet are each drawn across two
    64px cells, so placing them by tile id scattered half-trees around the
    level.  `col` is the tile column and `row` the tile row whose TOP EDGE the
    object stands on, so an object is anchored by its bottom centre and taller
    objects simply occupy the space above.
    """
    kind: str
    col: int
    row: int
    scale: float = 1.0
    flip: bool = False


@dataclass
class Level:
    name: str
    width: int
    height: int
    ground: List[Optional[int]]
    decor: List[Tuple[int, int, int]] = field(default_factory=list)  # (col,row,tile)
    scenery: List[SceneryItem] = field(default_factory=list)
    platforms: List[Platform] = field(default_factory=list)
    hazards:   List[Hazard]   = field(default_factory=list)
    coins:     List[Tuple[int, int]] = field(default_factory=list)
    powerups:  List[Powerup]  = field(default_factory=list)
    enemies:   List[EnemySpawn] = field(default_factory=list)
    facilities: List[Facility] = field(default_factory=list)
    player: Tuple[int, int] = (4, GROUND_ROW - 2)


# ---------------------------------------------------------------------------
# Terrain-aware placement
# ---------------------------------------------------------------------------

def _solid_row(L: Level, col: int) -> int:
    """Topmost solid row at `col`; walks outward over chasms if needed."""
    col = max(0, min(L.width - 1, col))
    if L.ground[col] is not None and L.ground[col] >= 0:
        return L.ground[col]
    for d in range(1, L.width):
        for c in (col - d, col + d):
            if 0 <= c < L.width and L.ground[c] is not None and L.ground[c] >= 0:
                return L.ground[c]
    return GROUND_ROW


def _clamp_row(row: int) -> int:
    return max(1, min(HEIGHT - 2, row))


def resolve_rows(L: Level) -> None:
    pcol, _ = L.player
    L.player = (pcol, _clamp_row(_solid_row(L, pcol) - 1))

    for e in L.enemies:
        if e.row is None:
            e.row = _clamp_row(_solid_row(L, e.col) - 1)
    # _solid_row() returns the last EMPTY row above the ground (its name says
    # "topmost solid", which is one off).  Enemies tolerate the extra -1 -
    # they fall onto the ground on the first frame - but facilities and
    # hazards are static, so they used to float a tile up: spikes hung in the
    # air and the door left a 64 px gap the 50 px hero walked under.
    for f in L.facilities:
        if f.row is None:
            f.row = _clamp_row(_solid_row(L, f.col))
    for p in L.powerups:
        if p.row is None:
            p.row = _clamp_row(_solid_row(L, p.col) - 2)
    for h in L.hazards:
        if h.row is None:
            cols = range(h.col, h.col + h.w)
            tops = [_solid_row(L, c) for c in cols]
            h.row = _clamp_row(min(tops))


# ---------------------------------------------------------------------------
# Playable terrain
#
# The heightmap is built from features, then every adjacent delta is clamped
# to +-MAX_STEP_UP so no wall can sneak in, then gaps are cut no wider than
# MAX_GAP.  Doing it in that order means a later feature can never resurrect
# an impassable step.
# ---------------------------------------------------------------------------

def _clamp_steps(g: List[Optional[int]]) -> None:
    """Force |height change| between neighbours to at most MAX_STEP_UP.

    Row numbers grow downward, so a smaller row means higher ground.  A step
    UP is therefore (g[c] - g[c+1]) > 0 and that is the direction that has to
    be limited — walking off a ledge is always allowed.
    """
    for c in range(len(g) - 1):
        if g[c] is None or g[c + 1] is None:
            continue
        if g[c] < 0 or g[c + 1] < 0:
            continue
        rise = g[c] - g[c + 1]          # >0 means the next column is higher
        if rise > MAX_STEP_UP:
            g[c + 1] = g[c] - MAX_STEP_UP
        elif rise < -MAX_STEP_UP:
            # a drop steeper than the limit is fine to fall down, but keep it
            # from becoming a sheer cliff the hero cannot climb back up
            g[c + 1] = g[c] + MAX_STEP_UP


def _h(i: int) -> float:
    """Deterministic 0..1 hash of an integer.

    A plain multiply-modulo LCG is no good for scattering: stepping the input
    by a fixed stride makes the high bits advance almost linearly, so the
    "random" value simply counts across the level and whole categories of
    detail never appear.  A murmur-style finalizer avalanches the input first.
    """
    x = (i * 0x9E3779B1) & 0xFFFFFFFF
    x ^= x >> 15
    x = (x * 0x85EBCA6B) & 0xFFFFFFFF
    x ^= x >> 13
    x = (x * 0xC2B2AE35) & 0xFFFFFFFF
    x ^= x >> 16
    return (x & 0xFFFFFF) / float(0x1000000)


# ---------------------------------------------------------------------------
# Biome Zones
# ---------------------------------------------------------------------------
ZONE_MEADOW = (0, 50)
ZONE_RUINS  = (50, 120)
ZONE_SUNKEN = (120, 175)
ZONE_ARENA  = (175, 200)


# Zone table of the stage being built: (name, first col, end col).  main()
# swaps it per stage; stage 1's is the original four-zone layout.  Every
# zone-aware step (materials, props, ambient export) reads it through
# get_zone(), so a stage changes its look by changing this table.
STAGE1_ZONES = [("meadow", *ZONE_MEADOW), ("ruins", *ZONE_RUINS),
                ("sunken", *ZONE_SUNKEN), ("arena", *ZONE_ARENA)]
ZONES = list(STAGE1_ZONES)
# Material of the walkable top row; Ember Canyon walks on pebbly soil.
SURFACE_MATERIAL = "teal"


def get_zone(col: int) -> str:
    for name, a, b in ZONES:
        if a <= col < b:
            return name
    return ZONES[-1][0]


DECOR_KEEP = {21, 22, 23, 31, 32}   # rocks, crystal + branch, small trees
SCENERY_DROP = {"bush", "boulder_teal", "boulder_deep"}


def scatter_decor(L: Level) -> None:
    """Populate the decor layer: the art that sits ON the terrain.

    Zoned by biome:
      - 0..50   Meadow:       Lush teal/green, green trees, bushes, tufts, teal boulders
      - 50..120 Moss Ruins:   Ancient ruins, autumn trees, soil boulders, crystal veins, stone walls
      - 120..175 Sunken Water: Wet navy stone, crystal veins, water chunks, shoreline ripples
      - 175..200 Boss Arena:  Dramatic framing at the gate & throne, clean floor for boss combat
    """
    g = L.ground
    cells: Dict[Tuple[int, int], int] = {}

    def put(c: int, r: int, t: int) -> None:
        if 0 <= c < L.width and 0 <= r < L.height:
            cells[(c, r)] = t

    def solid(c: int) -> bool:
        return 0 <= c < len(g) and g[c] is not None and g[c] >= 0

    def surf(c: int) -> int:
        """Row of the walkable cap tile -- the row a prop stands on top of."""
        return g[c] + 1

    def flat(c: int) -> bool:
        """Ground level with both neighbours, so nothing looks like it floats."""
        if not solid(c):
            return False
        base = g[c]
        return all(solid(c + d) and g[c + d] == base for d in (-1, 0, 1))

    def falls_right(c: int) -> int:
        """Rows of exposed cliff face on the right of column c."""
        if not (solid(c) and solid(c + 1)):
            return 0
        return max(0, g[c + 1] - g[c])

    taken = set()

    def claim(c: int, r: int, t: int, w: int = 1) -> None:
        """Place a prop and reserve its footprint so nothing overlaps it."""
        for k in range(w):
            if (c + k, r) in taken:
                return
        for k in range(w):
            put(c + k, r, t)
            taken.add((c + k, r))

    # --- 1. Trees (Two cells tall: top half r-2, trunk r-1) ---
    # Meadow gets green trees; Moss Ruins gets autumn trees.
    # Sunken Water and Arena have zero trees (cavern & colosseum).
    last_tree = -99
    for c in range(3, WIDTH - 3):
        zone = get_zone(c)
        if zone in ("sunken", "arena"):
            continue
        if not flat(c) or c - last_tree < 10:
            continue
        if _h(c * 31 + 3) > 0.35:
            continue
        r = surf(c)
        if (c, r - 1) in taken or (c, r - 2) in taken:
            continue
        if zone == "meadow":
            put(c, r - 2, TREE_G_TOP)
            put(c, r - 1, TREE_G_BOT)
            taken.add((c, r - 1))
            taken.add((c, r - 2))
            # Understorey cluster: 1-2 bushes or tufts nestled by the trunk
            for d in (-1, 1):
                if flat(c + d) and (c + d, surf(c + d) - 1) not in taken:
                    claim(c + d, surf(c + d) - 1, BUSH if _h(c * 43 + d) < 0.5 else GRASS_TUFT)
        elif zone == "ruins":
            put(c, r - 2, TREE_A_TOP)
            put(c, r - 1, TREE_A_BOT)
            taken.add((c, r - 1))
            taken.add((c, r - 2))
            # Ruins understorey: bush or soil boulder
            for d in (-1, 1):
                if flat(c + d) and (c + d, surf(c + d) - 1) not in taken:
                    claim(c + d, surf(c + d) - 1, BUSH if _h(c * 47 + d) < 0.6 else BOULDER_SOIL)
        last_tree = c

    # --- 2. Crystal Outcrops (Clusters of 1-3 with shards at base) ---
    # Allowed in Ruins, Sunken, and Arena framing. Denied in Meadow.
    last_x = -99
    for c in range(3, WIDTH - 3):
        zone = get_zone(c)
        if zone == "meadow":
            continue
        # Arena: Keep the battle floor (181..196) clean!
        if zone == "arena" and 181 <= c <= 196:
            continue
        if not flat(c) or c - last_x < 8 or _h(c * 53 + 17) > 0.32:
            continue
        n = 1 + int(_h(c * 59 + 23) * 3)
        n = min(n, 3)
        if all(not flat(c + k) for k in range(n)):
            continue
        for k in range(n):
            if flat(c + k):
                claim(c + k, surf(c + k) - 1, CRYSTAL)
        # Shards clustered at the foot of crystals
        for k in (-1, n):
            if flat(c + k) and _h(c * 61 + k + 7) < 0.65:
                claim(c + k, surf(c + k) - 1, SHARD)
        last_x = c + n

    # --- 3. Ground Details & Clustering with Breathing Room ---
    # Clustered in groups of 1-3, followed by breathing space
    in_cluster = 0
    cluster_zone = ""
    for c in range(2, WIDTH - 2):
        if not solid(c):
            in_cluster = 0
            continue
        zone = get_zone(c)
        # Keep arena floor clean for the boss fight
        if zone == "arena" and 181 <= c <= 196:
            continue
        r = surf(c) - 1
        if (c, r) in taken:
            continue

        hval = _h(c * 67 + 29)
        # Breathing room: only start a cluster if hash passes
        if in_cluster == 0:
            if hval > 0.28:
                continue
            in_cluster = 1 + int(_h(c * 71 + 11) * 2.5)  # cluster size 1 to 3
            cluster_zone = zone

        if in_cluster > 0 and zone == cluster_zone:
            in_cluster -= 1
            v = _h(c * 73 + 19)
            if zone == "meadow":
                if v < 0.40:
                    claim(c, r, GRASS_TUFT)
                elif v < 0.75:
                    claim(c, r, BUSH)
                else:
                    claim(c, r, BOULDER_TEAL)
            elif zone == "ruins":
                if v < 0.45:
                    claim(c, r, BOULDER_SOIL)
                elif v < 0.80:
                    claim(c, r, BUSH)
                else:
                    claim(c, r, SHARD)
            elif zone == "sunken":
                if v < 0.40:
                    claim(c, r, BOULDER_NAVY)
                elif v < 0.70:
                    claim(c, r, CHUNK_WATER)
                elif v < 0.85:
                    claim(c, r, CHUNK_WATER_B)
                else:
                    claim(c, r, SHARD)
            elif zone == "arena":
                # Boundary framing only
                if c < 181:
                    claim(c, r, BOULDER_NAVY if v < 0.5 else SHARD)
                else:
                    claim(c, r, SHARD if v < 0.6 else CRYSTAL)

    # --- 4. Cliff lips and exposed rock faces ---
    for c in range(2, WIDTH - 2):
        face = falls_right(c)
        if face < 1:
            continue
        r = surf(c)
        zone = get_zone(c)
        # Ledge arm jutting out over drop
        if _h(c * 71 + 31) < 0.45:
            claim(c, r - 1, LEDGE_ARM)
        if face < 2:
            continue
        depth = 1 + int(_h(c * 73 + 37) * min(face - 1, 2))
        fr = r + depth
        if fr < L.height:
            pick = _h(c * 79 + 41)
            if zone == "meadow":
                claim(c, fr, CHUNK_TEAL if pick < 0.6 else CHUNK_TEAL_B)
            elif zone == "ruins":
                claim(c, fr, STONE_WALL if pick < 0.6 else CRYSTAL)
            elif zone == "sunken":
                claim(c, fr, CRYSTAL if pick < 0.5 else CHUNK_WATER)
            else:
                claim(c, fr, STONE_WALL)

    # --- 5. Shorelines (meeting chasm water) ---
    for c in range(1, WIDTH - 1):
        if solid(c) and not solid(c + 1):
            put(c, surf(c) - 1, WATER_SHORE)
        elif solid(c) and not solid(c - 1) and _h(c * 83 + 43) < 0.5:
            put(c, surf(c) - 1, WATER_SHORE)

    # Batch A: keep only platformer.png cells that are genuine small props.
    # The rest (soil and navy blobs, water bands, teal blocks with mushrooms
    # on top, tree halves) are terrain chunks that clash with the autotiled
    # terrain_v2 ground; props_v2 fills the cells they leave.
    L.decor = [(c, r, t) for (c, r), t in sorted(cells.items())
               if t in DECOR_KEEP and L.ground[c] is not None and r == L.ground[c]]


def build_level() -> Level:
    L = Level(name="The Sunken Path", width=WIDTH, height=HEIGHT,
              ground=[GROUND_ROW] * WIDTH)
    g = L.ground

    # --- rolling hills -----------------------------------------------------
    for x in range(WIDTH):
        g[x] = GROUND_ROW + int(round(1.4 * math.sin(x * 0.055)
                                      + 0.8 * math.sin(x * 0.021 + 1.3)))

    # --- authored features -------------------------------------------------
    # gentle opening ramp so the first steps read clearly
    for x in range(0, 10):
        g[x] = GROUND_ROW - min(3, x // 3)

    # flat arena for the boss at the far end
    for x in range(180, WIDTH):
        g[x] = 31

    # a low rise and a matching fall, both walkable after clamping
    for x in range(56, 66):
        g[x] = GROUND_ROW - 2
    for x in range(96, 108):
        g[x] = GROUND_ROW - 3
    for x in range(140, 152):
        g[x] = GROUND_ROW - 4

    _clamp_steps(g)
    _clamp_steps(g)          # second pass settles knock-on changes

    # --- chasms: never wider than MAX_GAP ----------------------------------
    for start in (40, 78, 118):
        for x in range(start, start + MAX_GAP):
            g[x] = -1
        # the far lip must not be higher than the near lip, or the jump turns
        # into a wall the moment the gap is added
        near = g[start - 1]
        for x in range(start + MAX_GAP, min(WIDTH, start + MAX_GAP + 4)):
            if g[x] >= 0 and g[x] < near:
                g[x] = near

    _clamp_steps(g)

    # --- platforms ---------------------------------------------------------
    # Every platform is placed at most MAX_STEP_UP above the surface beneath
    # it, so a single hop always lands.  Platform tops are also kept clear of
    # each other so they do not form accidental ceilings.
    def safe_row(col: int, above: int) -> int:
        """Row `above` tiles over the surface at col, clamped to the map."""
        base = _solid_row(L, col)
        return _clamp_row(base - above)

    L.platforms += [
        Platform(38, safe_row(38, 2), 3, 1),     # stepping stones, chasm 1
        Platform(76, safe_row(76, 2), 3, 1),     # chasm 2
        Platform(116, safe_row(116, 2), 3, 1),   # chasm 3
        Platform(50, safe_row(50, 3), 4, 1),     # low ledge, coins
        Platform(60, safe_row(60, 3), 3, 1),
        Platform(70, safe_row(70, 3), 4, 1),
        Platform(88, safe_row(88, 3), 4, 1),
        Platform(103, safe_row(103, 3), 4, 1),
        Platform(130, safe_row(130, 3), 4, 1),
        Platform(155, safe_row(155, 3), 4, 1),   # approach to the arena
        Platform(170, safe_row(170, 2), 3, 1),
    ]
    # Moving platforms sit LOWER than the static ones on purpose: the vertical
    # traveller spends half its cycle above its spawn row, so a lift that
    # starts at the jump limit would be unreachable at the top of its travel.
    # Keeping it at 2 tiles leaves room for the range below.
    L.platforms += [
        Platform(64,  safe_row(64, 2),  3, 1, moving=True, range=96,  speed=70, vertical=False),
        Platform(100, safe_row(100, 2), 3, 1, moving=True, range=64,  speed=80, vertical=True),
        Platform(134, safe_row(134, 2), 3, 1, moving=True, range=96,  speed=85, vertical=False),
    ]

    # --- hazards: spike strips on the near lip of each chasm ---------------
    L.hazards += [
        Hazard("spikes", 36, 2, 1),
        Hazard("spikes", 43, 2, 1),
        Hazard("spikes", 74, 2, 1),
        Hazard("spikes", 81, 2, 1),
        Hazard("spikes", 114, 2, 1),
        Hazard("spikes", 121, 2, 1),
    ]

    # --- facilities --------------------------------------------------------
    L.facilities += [
        Facility("checkpoint", 30),
        Facility("checkpoint", 70),
        Facility("checkpoint", 110),
        Facility("checkpoint", 150),
        Facility("shrine",     55),
        Facility("shrine",     100),
        Facility("shrine",     145),
        Facility("key",        143),
        Facility("door",       175),
        # Batch B: a checkpoint just inside the gate, so a boss retry does not
        # start with a 2600 px walk back from checkpoint 150.
        Facility("checkpoint", 178),
    ]

    # --- enemies -----------------------------------------------------------
    L.enemies += [
        EnemySpawn("skeleton", 18),
        EnemySpawn("skeleton", 26),         # was 28: on checkpoint 30's respawn
        EnemySpawn("skeleton", 50),
        EnemySpawn("skeleton", 60),
        EnemySpawn("skeleton", 73),
        EnemySpawn("skeleton", 93),         # was 90: 5 enemies in cols 80..91
        EnemySpawn("skeleton", 106),        # was 108: on checkpoint 110's respawn
        EnemySpawn("skeleton", 132),
        EnemySpawn("skeleton", 158),
        EnemySpawn("slime", 35),
        EnemySpawn("slime", 88),
        EnemySpawn("slime", 128),
        EnemySpawn("bomber", 46),
        EnemySpawn("bomber", 84),
        EnemySpawn("bomber", 126),
        EnemySpawn("archer", 66),
        EnemySpawn("archer", 116),          # was 112: on checkpoint 110's respawn (114-115 are spikes)
        EnemySpawn("archer", 160),
        EnemySpawn("bat", 48, 22, stands_on="air"),
        EnemySpawn("bat", 80, 18, stands_on="air"),
        EnemySpawn("bat", 115, 19, stands_on="air"),
        EnemySpawn("bat", 138, 20, stands_on="air"),
        EnemySpawn("mage", 61, stands_on="platform"),   # was 71: ledge over checkpoint 70
        EnemySpawn("mage", 89, stands_on="platform"),
        EnemySpawn("mage", 131, stands_on="platform"),
        EnemySpawn("boss", 192, 30),
        # --- Batch A: +11 (26 -> 37), placed as readable patterns ---------
        # meadow: a slime joins the first skeleton pair; a bat meets you on
        # the flat before the first spikes-and-chasm run (at col 37 its
        # knockback threw you into the pit - unfair in the tutorial zone)
        EnemySpawn("slime", 22),
        EnemySpawn("bat", 24, 22, stands_on="air"),
        # ruins: a skeleton pair; a second bat over the approach to chasm 2;
        # an archer covering the far side of the ruins
        EnemySpawn("skeleton", 63),
        EnemySpawn("bat", 75, 19, stands_on="air"),
        EnemySpawn("archer", 98),
        # late ruins: a slime pack after the shrine; sunken: an archer
        # guarding the key
        EnemySpawn("slime", 101),
        EnemySpawn("slime", 103),
        EnemySpawn("archer", 140),
        # arena approach: the gauntlet before the door, a mage on the ledge
        EnemySpawn("skeleton", 165),
        EnemySpawn("bomber", 168),
        EnemySpawn("mage", 171, stands_on="platform"),
    ]

    # --- powerups ----------------------------------------------------------
    L.powerups += [
        Powerup("Health",      22),
        Powerup("Speed",       45),
        Powerup("Invincible",  65),
        Powerup("Health",      85),
        Powerup("DoubleJump",  103),
        Powerup("Damage",      125),
        Powerup("Magnet",      92),
        Powerup("Shield",      133),
        Powerup("Health",      145),
        Powerup("CoinX2",      168),
        Powerup("Speed",       186),
    ]

    # --- scenery: whole objects, not tile ids ------------------------------
    #
    # This used to drop raw tile ids into the decor layer, and those ids were
    # simply wrong: tile 40 is the BOTTOM HALF of the green tree and tile 32 is
    # the TOP HALF of the autumn tree, so the level was littered with severed
    # tree parts standing in for "crystals" and "lamps".
    #
    # Everything below names an assembled object from tools/build_objects.py
    # instead.  Placement is deterministic (a small LCG keyed on the column) so
    # the level looks the same every run without depending on random state.
    #
    # Objects only go on stretches of ground that are flat for three columns in
    # a row -- a prop standing on a step edge looks like it is floating.
    def _is_chasm(col: int) -> bool:
        if not 0 <= col < WIDTH:
            return True
        v = L.ground[col]
        return v is None or v < 0

    def _flat_run(col: int) -> bool:
        # Chasms are stored as -1, so they must be excluded explicitly --
        # testing only for None let a run of chasm columns look like flat
        # ground and put props floating over the water.
        if any(_is_chasm(c) for c in (col - 1, col, col + 1)):
            return False
        rows = [L.ground[c] for c in (col - 1, col, col + 1)]
        return all(r == rows[1] for r in rows)

    def _lcg(i: int) -> float:
        """Deterministic 0..1 hash.

        A plain multiply-modulo LCG is no good here: stepping the column by a
        fixed stride made the high bits advance almost linearly, so the
        "random" value simply counted down across the level and whole prop
        categories never appeared.  A murmur-style finalizer avalanches the
        input first, which decorrelates neighbouring columns.
        """
        x = (i * 0x9E3779B1) & 0xFFFFFFFF
        x ^= x >> 15
        x = (x * 0x85EBCA6B) & 0xFFFFFFFF
        x ^= x >> 13
        x = (x * 0xC2B2AE35) & 0xFFFFFFFF
        x ^= x >> 16
        return (x & 0xFFFFFF) / float(0x1000000)

    # tile_for() fills rows <= ground[col] with sky and puts the surface cap at
    # row ground[col] + 1, so the walkable surface is the TOP EDGE of that row.
    # An object standing on the ground is therefore anchored at ground+1 --
    # anchoring at ground-1 (the obvious reading of "one above the ground")
    # leaves every prop floating a full tile in the air.
    def _surface_row(col: int) -> int:
        return L.ground[col] + 1

    # Props carry no collider -- they are drawn decor, so density is purely a
    # look decision and cannot trap or block the player.
    #
    # One pass rather than several per-type passes.  Four independent loops
    # with their own spacing rules fought each other: whichever ran first
    # claimed the good columns and the later ones were filtered down to
    # almost nothing (boulder_deep and shard each collapsed to a single
    # placement on a 200-tile level).  Walking the columns once and advancing
    # by a kind-dependent stride spaces props evenly by construction and gives
    # every type a fair share.
    # --- scenery: whole objects, zoned by biome with clustering & breathing room ---
    # Objects allowed per zone:
    #   Meadow:       tree_green, bush, boulder_teal
    #   Moss Ruins:   tree_autumn, bush, crystal, branch_red, shard
    #   Sunken Water: boulder_deep, crystal, branch_red, shard
    #   Boss Arena:   framing crystals & deep boulders at gate & throne, floor clear
    scenery_specs = [
        # --- Meadow (0..50) ---
        SceneryItem("boulder_teal",  7, _surface_row(7),  scale=0.95),
        SceneryItem("tree_green",   14, _surface_row(14), scale=1.05),
        SceneryItem("bush",         15, _surface_row(15), scale=0.85),
        SceneryItem("tree_green",   22, _surface_row(22), scale=0.98, flip=True),
        SceneryItem("bush",         23, _surface_row(23), scale=0.80),
        SceneryItem("boulder_teal", 30, _surface_row(30), scale=1.00),
        SceneryItem("tree_green",   34, _surface_row(34), scale=1.10),
        SceneryItem("bush",         35, _surface_row(35), scale=0.90),

        # --- Moss Ruins (50..120) ---
        SceneryItem("tree_autumn",  58, _surface_row(58), scale=1.05),
        SceneryItem("bush",         59, _surface_row(59), scale=0.85),
        SceneryItem("crystal",      72, _surface_row(72), scale=1.00),
        SceneryItem("branch_red",   73, _surface_row(73), scale=0.95),
        SceneryItem("crystal",      86, _surface_row(86), scale=1.10),
        SceneryItem("shard",        87, _surface_row(87), scale=0.85),
        SceneryItem("tree_autumn",  92, _surface_row(92), scale=1.00, flip=True),
        SceneryItem("bush",         93, _surface_row(93), scale=0.80),
        SceneryItem("tree_autumn", 104, _surface_row(104), scale=0.95),
        SceneryItem("crystal",     105, _surface_row(105), scale=1.00),
        SceneryItem("shard",       106, _surface_row(106), scale=0.85),

        # --- Sunken Water (120..175) ---
        SceneryItem("boulder_deep", 123, _surface_row(123), scale=1.05),
        SceneryItem("crystal",      124, _surface_row(124), scale=1.15),
        SceneryItem("shard",        125, _surface_row(125), scale=0.90),
        SceneryItem("boulder_deep", 135, _surface_row(135), scale=1.00),
        SceneryItem("branch_red",   136, _surface_row(136), scale=0.95),
        SceneryItem("crystal",      144, _surface_row(144), scale=1.10),
        SceneryItem("shard",        145, _surface_row(145), scale=0.85),
        SceneryItem("boulder_deep", 156, _surface_row(156), scale=1.00),
        SceneryItem("branch_red",   157, _surface_row(157), scale=0.90),
        SceneryItem("crystal",      164, _surface_row(164), scale=1.05),
        SceneryItem("shard",        165, _surface_row(165), scale=0.85),

        # --- Boss Arena (175..200) ---
        # Arena Gate framing
        SceneryItem("boulder_deep", 176, _surface_row(176), scale=1.10),
        SceneryItem("crystal",      177, _surface_row(177), scale=1.20),
        SceneryItem("shard",        178, _surface_row(178), scale=0.90),
        # Arena Floor (180..196) is kept clear for the boss battle!
        # Arena Throne framing
        SceneryItem("crystal",      197, _surface_row(197), scale=1.25),
        SceneryItem("branch_red",   198, _surface_row(198), scale=1.05),
    ]
    # Filter so every scenery item is guaranteed to stand on a valid _flat_run
    # Batch A: "bush" (a mushroom-planter block) and the two "boulder" blobs
    # are platformer.png terrain cells, not props; next to terrain_v2 they
    # read as stray chunks of ground.  Their columns go to the prop clusters.
    L.scenery = [s for s in scenery_specs
                 if _flat_run(s.col) and s.kind not in SCENERY_DROP]

    # --- decor layer: the detail that sits on the terrain ------------------
    # Runs last because it reads the finished heightmap, the platform rows and
    # the chasms to decide what belongs where.
    scatter_decor(L)

    # --- coins: exactly 129 coins with arcs to teach jumps, platform rewards & secrets ---
    coin_list: List[Tuple[int, int]] = []

    # Zone 1: Meadow (cols 0-50)
    # Safe tutorial path
    for c in (4, 7, 10):
        coin_list.append((c, g[c] - 1))  # 3
    # Hop over hill arc
    coin_list += [(14, g[14] - 1), (15, g[15] - 2), (16, g[16] - 1)]  # 3
    # Gentle valley arc
    coin_list += [(24, g[24] - 1), (25, g[25] - 2), (26, g[26] - 1)]  # 3
    # Checkpoint 30 arc
    coin_list += [(29, g[29] - 1), (30, g[30] - 2), (31, g[31] - 1)]  # 3
    # Hop to stepping stone platform 38
    coin_list += [(36, g[36] - 1), (37, g[37] - 2), (38, safe_row(38, 2) - 1)]  # 3
    # Parabolic arc OVER chasm 40 (gap at 40-41)
    coin_list += [
        (39, safe_row(38, 2) - 2),
        (40, safe_row(38, 2) - 3),
        (41, safe_row(38, 2) - 3),
        (42, g[42] - 2),
    ]  # 4
    # Platform 50 reward
    for c in range(50, 54):
        coin_list.append((c, safe_row(50, 3) - 1))  # 4
    # Meadow subtotal: 23 coins

    # Zone 2: Moss Ruins (cols 50-120)
    # Hop onto rise 56
    coin_list += [(54, g[54] - 1), (55, g[55] - 2), (56, g[56] - 1)]  # 3
    # Platform 60 (w=3)
    for c in range(60, 63):
        coin_list.append((c, safe_row(60, 3) - 1))  # 3
    # Jump arc to moving platform 64
    coin_list += [(63, safe_row(60, 3) - 1), (64, safe_row(64, 2) - 1), (65, safe_row(64, 2) - 1)]  # 3
    coin_list.append((66, safe_row(64, 2) - 1))  # 1
    # Platform 70 (w=4)
    for c in range(70, 74):
        coin_list.append((c, safe_row(70, 3) - 1))  # 4
    # Approach to stepping stone 76
    coin_list += [(74, g[74] - 1), (75, g[75] - 2), (76, safe_row(76, 2) - 1)]  # 3
    # Parabolic arc OVER chasm 78 (gap at 78-79)
    coin_list += [
        (77, safe_row(76, 2) - 2),
        (78, safe_row(76, 2) - 3),
        (79, safe_row(76, 2) - 3),
        (80, g[80] - 2),
    ]  # 4
    # Platform 88 (w=4)
    for c in range(88, 92):
        coin_list.append((c, safe_row(88, 3) - 1))  # 4
    # Secret cache beneath platform 88
    coin_list += [(89, g[89] - 1), (90, g[90] - 1)]  # 2
    # Approach to rise 96
    coin_list += [(94, g[94] - 1), (95, g[95] - 2), (96, g[96] - 1)]  # 3
    # Vertical elevator 100 lift trail
    coin_list += [(100, safe_row(100, 2) - 1), (100, safe_row(100, 2) - 2), (100, safe_row(100, 2) - 3)]  # 3
    # High platform 103 reward (w=4)
    for c in range(103, 107):
        coin_list.append((c, safe_row(103, 3) - 1))  # 4
    # Checkpoint 110 arc
    coin_list += [(109, g[109] - 1), (110, g[110] - 2), (111, g[111] - 1)]  # 3
    # Ruins subtotal: 40 coins (Running: 63)

    # Zone 3: Sunken Water (cols 120-175)
    # Approach to stepping stone 116
    coin_list += [(114, g[114] - 1), (115, g[115] - 2), (116, safe_row(116, 2) - 1)]  # 3
    # Parabolic arc OVER chasm 118 (gap at 118-119)
    coin_list += [
        (117, safe_row(116, 2) - 2),
        (118, safe_row(116, 2) - 3),
        (119, safe_row(116, 2) - 3),
        (120, g[120] - 2),
    ]  # 4
    # Cavern trail
    for c in (122, 124, 126, 128):
        coin_list.append((c, g[c] - 1))  # 4
    # Platform 130 reward (w=4)
    for c in range(130, 134):
        coin_list.append((c, safe_row(130, 3) - 1))  # 4
    # Moving ferry platform 134 ride
    for c in range(134, 137):
        coin_list.append((c, safe_row(134, 2) - 1))  # 3
    # Jump arc off ferry onto rise 140
    coin_list += [(137, g[137] - 1), (138, g[138] - 2), (139, g[139] - 3), (140, g[140] - 1)]  # 4
    # Key chamber 143: Crown of gold around the Key
    coin_list += [
        (142, g[142] - 1), (142, g[142] - 2),
        (143, g[143] - 3),
        (144, g[144] - 2), (144, g[144] - 1),
    ]  # 5
    # Shrine 145 blessing arc
    coin_list += [(145, g[145] - 2), (146, g[146] - 2), (147, g[147] - 1)]  # 3
    # Checkpoint 150 trail
    coin_list += [(149, g[149] - 1), (150, g[150] - 2), (151, g[151] - 1)]  # 3
    # Platform 155 approach to arena (w=4)
    for c in range(155, 159):
        coin_list.append((c, safe_row(155, 3) - 1))  # 4
    # High-stakes leap trail
    for c in (160, 162, 164, 166, 168):
        coin_list.append((c, g[c] - 1))  # 5
    # Platform 170 final ledge (w=3)
    for c in range(170, 173):
        coin_list.append((c, safe_row(170, 2) - 1))  # 3
    # Boss door 175 threshold arch
    coin_list += [
        (173, g[173] - 1), (174, g[174] - 2), (175, g[175] - 3),
        (176, g[176] - 2), (177, g[177] - 1),
    ]  # 5
    # Sunken subtotal: 50 coins (Running: 113)

    # Zone 4: Boss Arena (cols 175-200)
    # Entrance constellation
    coin_list += [
        (180, 30), (181, 29), (182, 28), (183, 29), (184, 30),
        (186, 30), (188, 30), (190, 30),
    ]  # 8
    # Arena victory crown behind boss
    coin_list += [
        (194, 30), (195, 29), (196, 28), (197, 27),
        (198, 27), (198, 28), (199, 29), (199, 30),
    ]  # 8
    # Arena subtotal: 16 coins. Total = 113 + 16 = 129 coins.
    assert len(coin_list) == 129, f"Expected 129 coins, got {len(coin_list)}"
    # LVL-04: nothing on the spawn tile, so a fresh run starts at score 0
    # (a coin there was collected on frame 1).
    px, py = L.player
    L.coins = [(c, r) for (c, r) in coin_list if not (abs(c - px) <= 1 and abs(r - py) <= 1)]

    resolve_rows(L)

    # pin the mages onto the platform nearest their column
    for e in L.enemies:
        if e.stands_on == "platform":
            best = min(L.platforms,
                       key=lambda p: abs(p.col + p.w // 2 - e.col))
            e.col = best.col + best.w // 2
            e.row = best.row - 1

    return L


# ---------------------------------------------------------------------------
# Stages 2 and 3 (Batch B): built from a recipe instead of by hand
# ---------------------------------------------------------------------------
@dataclass
class StageSpec:
    """Everything that makes a stage different; the rules are shared.

    zones       (name, first col, end col) - drives materials, props, ambient
    hills       (amp1, freq1, amp2, freq2) of the rolling ground
    rises       (first col, end col, rows up) authored bumps
    chasms      first column of each 2-wide water gap
    ledges      (col, width) static platforms 3 rows over the ground
    movers      (col, vertical) moving platforms 2 rows over the ground
    mix         enemy kind -> weight; n_enemies is the target count
    """
    name: str
    folder: str
    theme: int
    zones: List[Tuple[str, int, int]]
    seed: float
    hills: Tuple[float, float, float, float]
    rises: List[Tuple[int, int, int]]
    chasms: List[int]
    ledges: List[Tuple[int, int]]
    movers: List[Tuple[int, bool]]
    checkpoints: List[int]
    shrines: List[int]
    key_col: int
    mix: Dict[str, int]
    n_enemies: int


STAGE_SPECS = [
    StageSpec(
        name="Jungle Falls", folder="stage2", theme=1,
        zones=[("meadow", 0, 36), ("sunken", 36, 110), ("ruins", 110, 175), ("arena", 175, 200)],
        seed=1.7, hills=(1.8, 0.071, 1.0, 0.029),
        rises=[(48, 58, 2), (84, 96, 3), (128, 140, 3), (156, 166, 2)],
        chasms=[34, 66, 100, 120, 146],
        ledges=[(52, 4), (74, 3), (88, 4), (108, 3), (134, 4), (160, 3), (170, 3)],
        movers=[(62, False), (116, True), (142, False)],
        checkpoints=[30, 72, 112, 152], shrines=[58, 128], key_col=164,
        mix={"slime": 4, "bat": 4, "archer": 3, "skeleton": 3, "bomber": 1, "mage": 2},
        n_enemies=38),
    StageSpec(
        name="Ember Canyon", folder="stage3", theme=2,
        zones=[("ruins", 0, 70), ("arena", 70, 112), ("ruins", 112, 175), ("arena", 175, 200)],
        seed=4.2, hills=(1.2, 0.093, 1.4, 0.041),
        rises=[(30, 38, 2), (58, 70, 3), (92, 104, 4), (124, 134, 3), (150, 162, 4)],
        chasms=[44, 80, 112, 140],
        ledges=[(26, 3), (40, 4), (54, 3), (64, 3), (86, 4), (98, 3), (118, 4), (130, 3),
                (146, 4), (158, 3), (170, 3)],
        movers=[(76, True), (108, False), (136, True)],
        checkpoints=[34, 74, 116, 154], shrines=[60, 126], key_col=166,
        mix={"skeleton": 4, "mage": 3, "bomber": 3, "archer": 3, "bat": 2, "slime": 1},
        n_enemies=40),
]


def build_stage(spec: StageSpec) -> Level:
    """A full stage from a recipe; validate_level() has the last word."""
    L = Level(name=spec.name, width=WIDTH, height=HEIGHT, ground=[GROUND_ROW] * WIDTH)
    g = L.ground
    a1, f1, a2, f2 = spec.hills
    for x in range(WIDTH):
        g[x] = GROUND_ROW + int(round(a1 * math.sin(x * f1 + spec.seed)
                                      + a2 * math.sin(x * f2 + 2 * spec.seed)))
    for x in range(0, 10):                    # the same gentle opening ramp
        g[x] = GROUND_ROW - min(3, x // 3)
    for x in range(180, WIDTH):               # flat boss arena
        g[x] = 31
    for a, b, up in spec.rises:
        for x in range(a, b):
            g[x] = GROUND_ROW - up
    _clamp_steps(g)
    _clamp_steps(g)
    for start in spec.chasms:
        for x in range(start, start + MAX_GAP):
            g[x] = -1
        near = g[start - 1]
        for x in range(start + MAX_GAP, min(WIDTH, start + MAX_GAP + 4)):
            if g[x] >= 0 and g[x] < near:
                g[x] = near
    _clamp_steps(g)
    # The gate run (168..180) is one flat stretch at the approach's height,
    # as in stage 1: a step under the gate tower's 64 px opening left no
    # headroom, and the fullrun bot stalled there with the door open.
    gate = g[167] if g[167] is not None and g[167] >= 0 else 28
    for x in range(168, 181):
        g[x] = gate
    _clamp_steps(g)

    def surf(c: int) -> int:
        return _solid_row(L, c)

    def flat(c: int, n: int = 1) -> bool:
        return all(0 <= c + k < WIDTH and g[c + k] is not None and g[c + k] >= 0
                   and g[c + k] == g[c] for k in range(-n, n + 1))

    chasm_cols = {c for s0 in spec.chasms for c in range(s0, s0 + MAX_GAP)}

    # platforms: a stepping stone before every chasm, ledges, movers
    # Stepping stones end one column BEFORE the lip: a stone over the take-off
    # column leaves 78 px of headroom, so a jump from the lip hit its
    # underside and dropped into the pit (the fullrun bot found this).
    for s0 in spec.chasms:
        L.platforms.append(Platform(s0 - 4, _clamp_row(surf(s0 - 4) - 2), 3, 1))
    for c, w in spec.ledges:
        L.platforms.append(Platform(c, _clamp_row(surf(c) - 3), w, 1))
    for c, vert in spec.movers:
        L.platforms.append(Platform(c, _clamp_row(surf(c) - 2), 3, 1, moving=True,
                                    range=64 if vert else 96, speed=75 + (c % 3) * 5,
                                    vertical=vert))
    # spikes near every chasm, but never in the take-off run (under the stone)
    # or the landing zone on the far side
    for i, s0 in enumerate(spec.chasms):
        for c in ((s0 - 7,) if i % 2 == 0 else (s0 + 6,)):
            if flat(c, 1) and flat(c + 1, 0) and not chasm_cols & {c - 1, c, c + 1, c + 2}:
                L.hazards.append(Hazard("spikes", c, 2, 1))
    if not L.hazards:
        raise SystemExit(f"{spec.name}: no flat lip for a spike strip")

    # facilities
    def walkable_near(c: int) -> int:
        for d in (0, 1, -1, 2, -2, 3, -3):
            if 0 <= c + d < WIDTH and g[c + d] is not None and g[c + d] >= 0 \
                    and (c + d) not in chasm_cols:
                return c + d
        return c
    for c in spec.checkpoints:
        L.facilities.append(Facility("checkpoint", walkable_near(c)))
    for c in spec.shrines:
        L.facilities.append(Facility("shrine", walkable_near(c)))
    # the key is mandatory: put it on a flat stretch, where nobody (bot or
    # player) jumps over it - at the foot of a staircase a hop skipped it
    key_col = next((spec.key_col + d for d in (0, 1, -1, 2, -2, 3, -3, 4, -4, 5, -5, 6, -6)
                    if 0 < spec.key_col + d < WIDTH - 1 and (spec.key_col + d) not in chasm_cols
                    and flat(spec.key_col + d, 1)), walkable_near(spec.key_col))
    L.facilities.append(Facility("key", key_col))
    L.facilities.append(Facility("door", 175))
    L.facilities.append(Facility("checkpoint", 178))

    # enemies: a weighted mix on a jittered grid, the fairness rules applied
    # while placing (validate_level() checks them again)
    kinds = [k for k, w in sorted(spec.mix.items()) for _ in range(w)]
    hazard_cols = {c for h in L.hazards for c in range(h.col, h.col + h.w)}
    cp_cols = [f.col for f in L.facilities if f.kind == "checkpoint"]
    static_ledges = [p for p in L.platforms if not p.moving and (p.col, p.w) in spec.ledges]
    placed: List[int] = []
    step = (172 - 14) / spec.n_enemies
    for i in range(spec.n_enemies):
        c = int(14 + i * step + _h(i * 31 + int(spec.seed * 100)) * 3)
        kind = kinds[int(_h(i * 17 + int(spec.seed * 10)) * len(kinds))]
        offsets = (0, 1, -1, 2, -2, 3, -3)
        if kind == "mage":
            # a mage takes the nearest ledge whose centre is free - never a
            # jittered column, which the final snap would move again
            free = [p for p in static_ledges if (p.col + p.w // 2) not in placed]
            if not free:
                continue
            ledge = min(free, key=lambda p: abs(p.col + p.w // 2 - c))
            c, offsets = ledge.col + ledge.w // 2, (0,)
        for d in offsets:
            cc = c + d
            if not (10 <= cc <= 172) or abs(cc - L.player[0]) < 8:
                continue
            if any(abs(cc - k) <= 2 for k in cp_cols) or abs(cc - key_col) <= 1:
                continue
            ground = kind not in ("bat", "mage")
            if ground and (cc in chasm_cols or cc in hazard_cols or g[cc] is None or g[cc] < 0):
                continue
            if sum(1 for x in placed if cc - 11 <= x <= cc + 11) >= 4:
                continue
            if cc in placed:
                continue
            if kind == "bat":
                L.enemies.append(EnemySpawn("bat", cc, _clamp_row(surf(cc) - 5 - int(_h(cc) * 3)),
                                            stands_on="air"))
            elif kind == "mage":
                L.enemies.append(EnemySpawn("mage", cc, stands_on="platform"))
            else:
                L.enemies.append(EnemySpawn(kind, cc))
            placed.append(cc)
            break
    L.enemies.append(EnemySpawn("boss", 192, 30))

    # powerups every 16 columns, on solid ground
    cycle = ["Health", "Speed", "Shield", "Damage", "Magnet", "Health", "DoubleJump",
             "CoinX2", "Invincible", "Health"]
    for i, c in enumerate(range(20, 172, 16)):
        L.powerups.append(Powerup(cycle[i % len(cycle)], walkable_near(c)))

    # scenery: zone-themed whole objects on flat runs, clear of the action
    busy_cols = hazard_cols | {f.col + d for f in L.facilities for d in (-1, 0, 1)} \
        | set(range(0, 8))
    by_zone = {"meadow": ("tree_green", "tree_green", "crystal"),
               "ruins": ("tree_autumn", "branch_red", "crystal"),
               "sunken": ("crystal", "shard", "branch_red"),
               "arena": ("shard", "crystal", "branch_red")}
    for c in range(10, WIDTH - 3, 6):
        c2 = c + int(_h(c * 7 + int(spec.seed * 10)) * 3)
        if c2 in busy_cols or c2 in chasm_cols or not flat(c2, 1):
            continue
        opts = by_zone[get_zone(c2)]
        kind = opts[int(_h(c2 * 13) * len(opts))]
        L.scenery.append(SceneryItem(kind, c2, surf(c2) + 1,
                                     scale=0.9 + _h(c2 * 3) * 0.25, flip=_h(c2 * 5) < 0.5))

    scatter_decor(L)

    # coins: hop arcs on flat ground, arcs over every chasm, ledge rewards
    coins: List[Tuple[int, int]] = []
    for c in range(12, 172, 5):
        if flat(c, 1) and not ({c - 1, c, c + 1} & (hazard_cols | chasm_cols)):
            coins += [(c - 1, g[c - 1] - 1), (c, g[c] - 2), (c + 1, g[c + 1] - 1)]
    for s0 in spec.chasms:
        top = _clamp_row(surf(s0 - 4) - 2)
        coins += [(s0 - 1, top - 2), (s0, top - 3), (s0 + 1, top - 3), (s0 + 2, g[s0 + 2] - 2)]
    for p in L.platforms:
        if not p.moving and (p.col, p.w) in spec.ledges:
            coins += [(x, p.row - 1) for x in range(p.col, p.col + p.w)]
    coins += [(194, 30), (195, 29), (196, 28), (197, 27), (198, 28), (199, 29)]
    seen = set()
    px, py = L.player
    L.coins = []
    for (c, r) in coins:
        if (c, r) in seen or not (0 <= c < WIDTH and 0 <= r < HEIGHT):
            continue
        if abs(c - px) <= 1 and abs(r - py) <= 1:
            continue
        seen.add((c, r))
        L.coins.append((c, r))

    resolve_rows(L)
    for e in L.enemies:
        if e.stands_on == "platform":
            # the same ledges the placement used, so the density check holds
            best = min(static_ledges, key=lambda p: abs(p.col + p.w // 2 - e.col))
            e.col = best.col + best.w // 2
            e.row = best.row - 1
    return L


# ---------------------------------------------------------------------------
# Validation — refuse to emit a level the hero cannot finish
# ---------------------------------------------------------------------------

def validate_level(L: Level) -> List[str]:
    """Return a list of playability violations; empty means the level is fine."""
    bad: List[str] = []
    g = L.ground

    # 1. no step up steeper than the jump height
    for c in range(WIDTH - 1):
        a, b = g[c], g[c + 1]
        if a is None or b is None or a < 0 or b < 0:
            continue
        if a - b > MAX_STEP_UP:
            bad.append(f"col {c}->{c+1}: step up {a-b} tiles "
                       f"(> {MAX_STEP_UP}) is a wall")

    # 2. no gap wider than the jump length
    run = 0
    for c in range(WIDTH):
        if g[c] is None or g[c] < 0:
            run += 1
            continue
        if run > MAX_GAP:
            bad.append(f"gap ending at col {c}: {run} tiles wide "
                       f"(> {MAX_GAP}) cannot be jumped")
        run = 0
    if run > MAX_GAP:
        bad.append(f"trailing gap of {run} tiles at the map edge")

    # 3. every platform must be reachable from the surface under it
    for p in L.platforms:
        if not (0 <= p.col < WIDTH):
            bad.append(f"platform at col {p.col} is off the map")
            continue
        base = _solid_row(L, p.col)
        rise = base - p.row                 # rows saved by standing on it
        if rise > MAX_STEP_UP + 1:
            bad.append(f"platform at col {p.col}: {rise} tiles above the "
                       f"ground below - out of jump range")

    # 4. the whole map must be walkable start to finish
    x = 0
    guard = 0
    while x < WIDTH - 1 and guard < WIDTH * 4:
        guard += 1
        if g[x] is None or g[x] < 0:
            # hop the gap
            run = 0
            while x < WIDTH and (g[x] is None or g[x] < 0):
                x += 1
                run += 1
            if run > MAX_GAP:
                bad.append(f"unwalkable: {run}-tile gap before col {x}")
                break
            continue
        if g[x + 1] is not None and g[x + 1] >= 0 and g[x] - g[x + 1] > MAX_STEP_UP:
            bad.append(f"unwalkable: {g[x]-g[x+1]}-tile wall at col {x}")
            break
        x += 1

    # 5. spawn must be on solid ground
    pc, pr = L.player
    if g[pc] is None or g[pc] < 0:
        bad.append(f"player spawns over a chasm at col {pc}")

    # 6. enemies (Batch A): fair placement, never a wall of them
    checkpoints = [f.col for f in L.facilities if f.kind == "checkpoint"]
    ground_kinds = [e for e in L.enemies if e.stands_on == "ground"]
    for e in L.enemies:
        if e.kind == "boss":
            continue
        if abs(e.col - pc) < 8:
            bad.append(f"{e.kind} at col {e.col} is within 8 columns of spawn")
        near = [c for c in checkpoints if abs(e.col - c) <= 2]
        if near:
            bad.append(f"{e.kind} at col {e.col} would hit a respawn at checkpoint {near[0]}")
        if e.stands_on == "ground" and (g[e.col] is None or g[e.col] < 0):
            bad.append(f"{e.kind} at col {e.col} stands over a chasm")
        if e.stands_on == "ground" and any(h.col <= e.col < h.col + h.w for h in L.hazards):
            bad.append(f"{e.kind} at col {e.col} stands on a hazard")
        if e.stands_on == "platform":
            on = [p for p in L.platforms if p.col <= e.col < p.col + p.w and p.row == e.row + 1]
            if not on or on[0].moving:
                bad.append(f"{e.kind} at col {e.col} is not on a static platform")
    cols = sorted(e.col for e in L.enemies if e.kind != "boss")
    for i, c in enumerate(cols):
        window = [x for x in cols if c <= x < c + 12]
        if len(window) > 4:
            bad.append(f"{len(window)} enemies in cols {c}..{c + 11} (max 4 per 12)")
            break

    return bad


# ---------------------------------------------------------------------------
# Emit tile maps
# ---------------------------------------------------------------------------

def _solid_at(col: int, row: int, g: List[Optional[int]]) -> bool:
    """Is (col,row) inside the ground mass?

    g[col] is the last EMPTY row above the surface, so solid rows are
    row > top -- the surface tile itself is row top+1.
    """
    if not (0 <= col < len(g)):
        return False
    v = g[col]
    return v is not None and v >= 0 and row > v


def _is_water_col(col: int, g: List[Optional[int]]) -> bool:
    if not (0 <= col < len(g)):
        return False
    v = g[col]
    return v is not None and v < 0


def _edge_index(col: int, inside) -> int:
    """0 = left cap, 1 = middle fill, 2 = right cap.

    `inside(c)` says whether column c continues the same run at this row.
    Only the middle tile is seamless, so it is the sole tile used to fill a
    run; the caps appear strictly at the exposed ends.
    """
    left  = inside(col - 1)
    right = inside(col + 1)
    if not left and not right:
        return 1          # isolated column: neither side has anything to join
    if not left:
        return 0
    if not right:
        return 2
    return 1


def _ground_material(col: int, row: int, top: int) -> str:
    """Material of a solid ground cell: the zoning the old tile_for() used.

    Surface row is always teal; below it each zone keeps its own strata.  The
    old code put a fringe cell on the map's last row only because it had no
    way to close a mass; with real autotiling the bottom is interior stone.
    """
    if row == top + 1:
        return SURFACE_MATERIAL
    d = row - top
    zone = get_zone(col)
    if zone == "meadow":
        return "teal" if d <= 3 else ("soil" if d <= 5 else "stone")
    if zone == "ruins":
        return "soil" if d <= 3 else "stone"
    if zone == "sunken":
        return "navy" if d <= 3 else ("soil" if d == 4 else "stone")
    return "soil" if d == 3 else "stone"           # arena


def _depth_band(row: int, top: int) -> int:
    """ART_STYLE_V2 4.3: x0.85 below top+3, x0.72 below top+6."""
    if row > top + 6:
        return 2
    if row > top + 3:
        return 1
    return 0


def _variant(col: int, row: int) -> int:
    """0..11 from a deterministic hash of (col, row); interiors take % 4, edges % 3."""
    return int(_h(col * 7919 + row * 104729 + 17) * 12)


def gate_wall(L: Level) -> List[Tuple[int, int]]:
    """Batch B: the stone tower the arena door is set into.

    The door is 88 px tall and the hero jumps 202 px - from the ledge at col
    170 he could even land above it - so a lone door is no barrier.  A solid
    column from the top of the map down to the row above the door closes
    that; it overlaps the door's top and leaves a 64 px opening (the hero is
    50 px) that the door fills until it is unlocked.
    """
    cells = []
    for f in L.facilities:
        if f.kind == "door" and f.row is not None:
            cells += [(f.col, r) for r in range(0, f.row)]
    return cells


def terrain_cells(L: Level) -> Dict[Tuple[int, int], Tuple[str, str, int]]:
    """(col,row) -> (kind, material, band) for every foreground cell.

    kind is "g" for solid ground (heightmap + static platforms) and "w" for
    chasm water.  Static platforms are teal slabs at band 0; moving ones are
    entities only (the old writer baked their spawn rect in as a ghost slab).
    """
    g = L.ground
    cells: Dict[Tuple[int, int], Tuple[str, str, int]] = {}
    for col in range(L.width):
        top = g[col]
        if top is None:
            continue
        for row in range(L.height):
            if top < 0:
                if row > WATER_LINE:
                    cells[(col, row)] = ("w", "water",
                                         _depth_band(row, WATER_LINE))
            elif row > top:
                cells[(col, row)] = ("g", _ground_material(col, row, top),
                                     _depth_band(row, top))
    for p in L.platforms:
        if p.moving:          # the game spawns it as an entity; a static copy
            continue          # here would leave a solid ghost slab at its start
        for x in range(p.col, p.col + p.w):
            for y in range(p.row, p.row + p.h):
                if 0 <= x < L.width and 0 <= y < L.height:
                    cells[(x, y)] = ("g", SURFACE_MATERIAL, 0)
    for (x, y) in gate_wall(L):
        cells[(x, y)] = ("g", "stone", 0)
    return cells


def tile_for(col: int, row: int, cells, width: int, height: int) -> int:
    """terrain_v2 id for (col,row): 4-neighbour bitmask autotiling.

    A ground cell connects to ground; a water cell connects to water and to
    ground (the chasm wall draws the edge, the water runs up to it).  Below
    the map counts as more of the same mass, and the left/right map edges
    repeat the edge column, so nothing is outlined at the border of the world.
    """
    me = cells.get((col, row))
    if me is None:
        return EMPTY
    kind, material, band = me

    def k(c: int, r: int) -> Optional[str]:
        if r >= height:
            return "g"
        if r < 0:
            return None
        c = min(max(c, 0), width - 1)
        v = cells.get((c, r))
        return v[0] if v else None

    def joins(c: int, r: int) -> bool:
        n = k(c, r)
        return n == "g" or (kind == "w" and n == "w")

    mask = ((BT.N if joins(col, row - 1) else 0) |
            (BT.E if joins(col + 1, row) else 0) |
            (BT.S if joins(col, row + 1) else 0) |
            (BT.W if joins(col - 1, row) else 0))
    diag = set()
    if mask == 15:
        for name, dc, dr in (("NE", 1, -1), ("SE", 1, 1),
                             ("SW", -1, 1), ("NW", -1, -1)):
            if not joins(col + dc, row + dr):
                diag.add(name)
    piece = BT.piece_for(mask, diag, _variant(col, row))
    return BT.terrain_id(material, band, piece)


# The water surface row used by the chasms.  tile_for() puts the bright water
# tile at WATER_LINE + 1; that same value is exported to Level.h as
# WATER_SURFACE_ROW so Parallax::drawWater animates the crest on the exact row
# the tiles occupy.  Deriving it per column would put chasm water at row 3 —
# GROUND_TOP is -1 there — i.e. a column of water hanging in the sky.
WATER_LINE = GROUND_ROW + 3
WATER_ROW  = WATER_LINE + 1


def write_layer(path: str, L: Level, fn) -> None:
    rows = []
    for y in range(L.height):
        rows.append(",".join(str(fn(x, y)) for x in range(L.width)))
    with open(path, "w") as f:
        f.write("\n".join(rows) + "\n")


def write_foreground(path: str, L: Level) -> None:
    # Floating platforms are folded into terrain_cells(), so a slab is
    # autotiled exactly like the ground: end caps, top, underside, and the
    # inner corner where a slab rests on a step.
    cells = terrain_cells(L)

    def fn(x: int, y: int) -> int:
        return tile_for(x, y, cells, L.width, L.height)
    write_layer(path, L, fn)


# --- distant silhouettes ---------------------------------------------------
#
# The background layer used to be the playable heightmap shifted down one row
# and repainted navy.  The foreground covers that pixel for pixel, so the
# layer drew nothing at all -- 2000 tiles of invisible work.
#
# A backdrop has to sit HIGHER in frame than the ground the hero walks on, or
# there is nothing for it to be seen against.  Both profiles below are
# independent of L.ground on purpose: an echo of the playable hills reads as a
# second copy of the same hill, not as distance.  Longer wavelengths and a
# higher baseline are what put them further back.

def _far_profile(x: int) -> int:
    """Ridgeline for the far layer (parallax 0.30), shaped per biome zone."""
    if x < 50:
        # Meadow: gentle rolling horizon
        return GROUND_ROW - 5 + int(round(2.0 * math.sin(x * 0.025 + 0.5)))
    elif x < 120:
        # Moss Ruins: ancient ruined peaks and broken spires
        return GROUND_ROW - 7 + int(round(2.6 * math.sin(x * 0.045 + 1.2) + 1.2 * math.sin(x * 0.09)))
    elif x < 175:
        # Sunken Water: deep basin dipping low into the earth
        return GROUND_ROW - 4 + int(round(2.2 * math.sin(x * 0.022 + 2.0)))
    else:
        # Boss Arena: towering citadel walls enclosing the arena
        return GROUND_ROW - 8 + int(round(2.5 * math.sin(x * 0.06 + 0.8)))


def _mid_profile(x: int) -> int:
    """Ridgeline for the mid layer (parallax 0.62), between far and near."""
    if x < 50:
        return GROUND_ROW - 2 + int(round(1.6 * math.sin(x * 0.035 + 1.8)))
    elif x < 120:
        return GROUND_ROW - 3 + int(round(2.2 * math.sin(x * 0.05 + 0.4)))
    elif x < 175:
        return GROUND_ROW - 1 + int(round(1.5 * math.sin(x * 0.03 + 2.8)))
    else:
        return GROUND_ROW - 4 + int(round(2.0 * math.sin(x * 0.07 + 1.5)))


def _ridge_props(width: int, prof: List[int], density: float
                 ) -> Dict[Tuple[int, int], int]:
    """Scatter trees, spires and rocks along the flat stretches of a ridge, zoned by biome.

    Only columns level with both neighbours get anything -- a prop on a slope
    looks like it is sliding off.  The two-cell trees are placed as a matched
    pair with the trunk cell (40/41) on the ridge and the canopy cell
    (31/32) directly above it.
    """
    cells: Dict[Tuple[int, int], int] = {}
    last = -99
    for x in range(2, width - 2):
        if prof[x - 1] != prof[x] or prof[x + 1] != prof[x]:
            continue
        if x - last < 5:
            continue
        v = _h(x * 89 + 51)
        if v > density:
            continue
        zone = get_zone(x)
        k = v / density if density else 0.0
        if zone == "meadow":
            if k < 0.45:
                cells[(x, prof[x] - 1)] = TREE_G_TOP
                cells[(x, prof[x])] = TREE_G_BOT
            elif k < 0.75:
                cells[(x, prof[x])] = BUSH
            else:
                cells[(x, prof[x])] = BOULDER_TEAL
        elif zone == "ruins":
            if k < 0.45:
                cells[(x, prof[x] - 1)] = TREE_A_TOP
                cells[(x, prof[x])] = TREE_A_BOT
            elif k < 0.75:
                cells[(x, prof[x])] = CRYSTAL
            else:
                cells[(x, prof[x])] = SHARD
        elif zone == "sunken":
            if k < 0.40:
                cells[(x, prof[x])] = BOULDER_NAVY
            elif k < 0.75:
                cells[(x, prof[x])] = CRYSTAL
            else:
                cells[(x, prof[x])] = SHARD
        else:  # arena
            if k < 0.50:
                cells[(x, prof[x])] = CRYSTAL
            else:
                cells[(x, prof[x])] = SHARD
        last = x
    return cells


def _ridge_layer(L: Level, prof: List[int], props: Dict[Tuple[int, int], int],
                 fill: int, surf_set: Tuple[int, int, int],
                 bot_set: Tuple[int, int, int]):
    def fn(x: int, y: int) -> int:
        if not (0 <= x < len(prof)):
            return EMPTY
        if y < prof[x] and (x, y) in props:
            return props[(x, y)]
        if y <= prof[x]:
            return EMPTY

        def inside(c: int) -> bool:
            return 0 <= c < len(prof) and y > prof[c]
        i = _edge_index(x, inside)
        if y == prof[x] + 1:
            return surf_set[i]
        if y == L.height - 1:
            return bot_set[i]
        return fill
    return fn


BG_PARALLAX = 0.30   # must match Depth::BG_TILES in src/Depth.h


def _read_config_int(name: str, default: int) -> int:
    try:
        with open(CONFIG_H) as f:
            m = re.search(rf"constexpr\s+int\s+{name}\s*=\s*([0-9]+)\s*;", f.read())
        return int(m.group(1)) if m else default
    except OSError:
        return default


def _visible_extent(L: Level, parallax: float) -> Tuple[int, int]:
    """Last col/row of a layer at `parallax` that the clamped camera can show.

    A tile at world (x, y) draws at (x - cam.x * p, y - cam.y * p) and the
    camera never leaves the map, so nothing past these can reach the screen.
    """
    win_w = _read_config_int("WINDOW_WIDTH", 1350)
    win_h = _read_config_int("WINDOW_HEIGHT", 850)
    max_cam_x = L.width * TILE - win_w
    max_cam_y = L.height * TILE - win_h
    last_col = min(L.width - 1, int(math.ceil((max_cam_x * parallax + win_w) / TILE)))
    last_row = min(L.height - 1, int(math.ceil((max_cam_y * parallax + win_h) / TILE)) - 1)
    return last_col, last_row


def background_cells(L: Level) -> Dict[Tuple[int, int], int]:
    """Hazy ridge silhouette for the 0.30 parallax plane (ART_STYLE_V2 4.4).

    Designed in the layer's OWN space (see _visible_extent): TEETH (spires) or
    RIDGE_TOP on a skyline between rows 11 and 14, MASS below it, then the
    DEEP body down to the last row the camera can reach.  The old layer put
    its ridge at row ~24, below the frame at every camera height.  The canopy
    cells in terrain_v2 belong to the foreground silhouette plane (ART-13).
    """
    last_col, last_row = _visible_extent(L, BG_PARALLAX)
    cells: Dict[Tuple[int, int], int] = {}
    for x in range(last_col + 1):
        v = int(_h(x * 131 + 7) * 3)
        top = 12 + int(round(1.6 * math.sin(x * 0.11 + 0.7)
                             + 1.0 * math.sin(x * 0.29 + 2.0)))
        top = max(11, min(14, top))
        cells[(x, top)] = (BT.BG["TEETH_%d" % v] if _h(x * 71 + 5) < 0.6
                           else BT.BG["RIDGE_TOP"])
        for y in range(top + 1, last_row + 1):
            band = "MASS" if y <= top + 3 else "DEEP"
            cells[(x, y)] = BT.BG["%s_%d" % (band, int(_h(x * 37 + y * 41) * 3))]
    return cells


def write_background(path: str, L: Level) -> None:
    """Far plane: canopy + ridge silhouettes from terrain_v2, parallax 0.30."""
    cells = background_cells(L)

    def fn(x: int, y: int) -> int:
        return cells.get((x, y), EMPTY)
    write_layer(path, L, fn)


def mid_cells(L: Level) -> Dict[Tuple[int, int], int]:
    """Mid plane (0.55): dark foliage mounds between the haze and the play line.

    Built from terrain_v2's FOLIAGE_TOP / CANOPY cells - reference 1's mid
    plane is leaf mass, not a second copy of the walkable ground (the old
    layer was platformer.png's teal fill, a flat slab).  The authored
    _mid_profile sat in world rows two tiles above the ground; it is shifted
    up by cam.y * (1 - p) / TILE at the lowest camera so it still reads as
    "just above the ground line" at 0.55, and clipped to what can be seen.
    """
    last_col, last_row = _visible_extent(L, MID_PARALLAX)
    max_cam_y = L.height * TILE - _read_config_int("WINDOW_HEIGHT", 850)
    shift = int(round(max_cam_y * (1.0 - MID_PARALLAX) / TILE))
    cells: Dict[Tuple[int, int], int] = {}
    for x in range(last_col + 1):
        top = _mid_profile(x) - shift
        cells[(x, top)] = BT.BG["FOLIAGE_TOP_%d" % int(_h(x * 61 + 9) * 3)]
        for y in range(top + 1, last_row + 1):
            cells[(x, y)] = BT.BG["CANOPY_%d" % int(_h(x * 23 + y * 47) * 3)]
    return cells


MID_PARALLAX = 0.55   # must match Depth::MID in src/Depth.h


def write_mid(path: str, L: Level) -> None:
    cells = mid_cells(L)

    def fn(x: int, y: int) -> int:
        return cells.get((x, y), EMPTY)
    write_layer(path, L, fn)


def write_decor(path: str, L: Level) -> None:
    decor = {(c, r): t for (c, r, t) in L.decor}
    def fn(x: int, y: int) -> int:
        return decor.get((x, y), EMPTY)
    write_layer(path, L, fn)


def place_props(L: Level):
    """Clusters of zone-themed props on walkable surfaces (Batch A, LVL-01).

    Returns (props, side): centred props and the small side props that tuck
    in beside a centred item - two layers, so one cell can show both.

    A prop occupies the empty cell directly above a surface, or hangs in the
    cell under a static platform.  It never shares a cell with decor, a coin
    or a hazard, keeps clear of spawn, facilities, enemy spawns, scenery and
    chasm edges, and props that read as solid (crates, barrels, pillars,
    stumps) stay off step edges so nobody mistakes one for a ledge.
    Deterministic: every choice is _h() of the column.
    """
    g = L.ground
    busy = {(c, r) for (c, r, _) in L.decor}
    coins = set(L.coins)
    low = {BP.PROP[n] for n in BP.LOW}       # may sit under a floating coin
    blocked_cols = set()
    pc = L.player[0]
    blocked_cols |= set(range(pc - 2, pc + 3))
    for f in L.facilities:
        blocked_cols |= {f.col - 1, f.col, f.col + 1}
    for s_ in L.scenery:
        blocked_cols.add(s_.col)
    for hz in L.hazards:
        blocked_cols |= set(range(hz.col - 1, hz.col + hz.w + 1))

    def surface(c: int) -> Optional[int]:
        if not (0 <= c < L.width) or g[c] is None or g[c] < 0:
            return None
        return g[c]

    props: Dict[Tuple[int, int], int] = {}
    col, last_zone = 1, None
    while col < L.width - 1:
        zone = get_zone(col)
        if zone != last_zone and last_zone is not None:
            # a signpost marks every zone entrance (first free surface cell)
            for c in range(col, min(col + 6, L.width - 1)):
                r = surface(c)
                if r is not None and c not in blocked_cols and (c, r) not in busy \
                        and (c, r) not in coins \
                        and surface(c - 1) is not None and surface(c + 1) is not None:
                    props[(c, r)] = BP.PROP["SIGNPOST"]
                    break
        last_zone = zone
        if _h(col * 97 + 13) < 0.85:                       # start a cluster
            length = 3 + int(_h(col * 89 + 5) * 3)        # 3..5 columns
            for c in range(col, min(col + length, L.width - 1)):
                r = surface(c)
                if r is None or c in blocked_cols or (c, r) in busy or (c, r) in props:
                    continue
                if surface(c - 1) is None or surface(c + 1) is None:
                    continue                               # chasm edge
                pool = BP.ZONE_SET[get_zone(c)]
                # earlier entries are commoner: weight n, n-1, ... 1
                weights = list(range(len(pool), 0, -1))
                pick = _h(c * 131 + 71) * sum(weights)
                for name, w in zip(pool, weights):
                    pick -= w
                    if pick < 0:
                        break
                step = surface(c - 1) != r or surface(c + 1) != r
                if step and name in BP.SOLID_LOOKING:
                    name = "ROCKS"
                if (c, r) in coins and BP.PROP[name] not in low:
                    name = "GRASS_TALL" if zone != "sunken" else "FERN"
                props[(c, r)] = BP.PROP[name]
            col += length + 1 + int(_h(col * 71 + 3) * 2)  # gap 1..2
        else:
            col += 1
    # side props beside the existing decor: same cell, one quarter of it
    keep_clear = set(range(pc - 2, pc + 3))
    for f in L.facilities:
        keep_clear |= {f.col - 1, f.col, f.col + 1}
    side: Dict[Tuple[int, int], int] = {}
    every = [(c, surface(c)) for c in range(1, L.width - 1) if surface(c) is not None]
    for (c, r) in every:
        if c in keep_clear or any(hz.col <= c < hz.col + hz.w for hz in L.hazards):
            continue
        beside = (c, r) in busy or (c, r) in props
        if _h(c * 59 + 23) < (0.75 if beside else 0.45):
            kinds = BP.SIDE_SET[get_zone(c)]
            kind = kinds[int(_h(c * 61 + 7) * len(kinds))]
            sd = "_R" if _h(c * 67 + 1) < 0.5 else "_L"
            side[(c, r)] = BP.PROP[kind + sd]
    # static platforms: something small on top, vines or roots underneath
    small = ("ROCKS", "GRASS_TALL", "MUSHROOMS", "FLOWERS", "FERN")
    for p in L.platforms:
        if p.moving:
            continue
        for c in range(p.col, p.col + p.w):
            top = (c, p.row - 1)
            if top not in busy and top not in props and top not in coins \
                    and _h(c * 41 + p.row) < 0.45:
                props[top] = BP.PROP[small[int(_h(c * 43 + 1) * len(small))]]
            under = (c, p.row + p.h)
            gr = surface(c)
            if (gr is None or under[1] <= gr) and under not in busy \
                    and _h(c * 47 + p.row) < 0.6:
                props[under] = BP.PROP[BP.ZONE_HANG[get_zone(c)]]
    return props, side


def write_props(path: str, side_path: str, L: Level) -> None:
    props, side = place_props(L)
    write_layer(path, L, lambda x, y: props.get((x, y), EMPTY))
    write_layer(side_path, L, lambda x, y: side.get((x, y), EMPTY))


def write_coins(path: str, L: Level) -> None:
    coin_set = set(L.coins)
    def fn(x: int, y: int) -> int:
        return 1 if (x, y) in coin_set else 0
    write_layer(path, L, fn)


def write_colliders(path: str, L: Level) -> None:
    rects = []
    for y in range(L.height):
        x = 0
        while x < L.width:
            top = L.ground[x]
            if top is None or top < 0 or y <= top:
                x += 1
                continue
            run_start = x
            while x < L.width and L.ground[x] is not None \
                    and L.ground[x] >= 0 and y > L.ground[x]:
                x += 1
            rects.append((run_start, y, x - run_start, 1))
    for p in L.platforms:
        if not p.moving:      # moving platforms carry their own collider
            rects.append((p.col, p.row, p.w, p.h))
    wall = gate_wall(L)
    if wall:
        col = wall[0][0]
        rects.append((col, 0, 1, max(r for _, r in wall) + 1))
    with open(path, "w") as f:
        for (x, y, w, h) in rects:
            f.write(f"{x} {y} {w} {h}\n")


# ---------------------------------------------------------------------------
# Emit src/Level.h
# ---------------------------------------------------------------------------

HEADER = '''#pragma once
// Auto-generated by tools/build_level.py — do not edit by hand.
// All coordinates are in TILE units (1 tile = {TILE} px).
#include <cstddef>
#include <utility>

namespace Level {{
    inline constexpr int  TILE              = {TILE};
    inline constexpr int  WIDTH_TILES       = {WIDTH};
    inline constexpr int  HEIGHT_TILES      = {HEIGHT};
    inline constexpr int  PIXEL_WIDTH       = WIDTH_TILES  * TILE;
    inline constexpr int  PIXEL_HEIGHT      = HEIGHT_TILES * TILE;
    inline constexpr int  GROUND_TILE_ROW   = {GROUND_ROW};
    inline constexpr const char* NAME       = "{name}";

    // Row of the water surface inside a chasm.  tile_for() emits the bright
    // water tile here and Parallax::drawWater animates its crest at exactly
    // this row, so both read the same constant and cannot drift apart.
    inline constexpr int  WATER_SURFACE_ROW = {WATER_ROW};

    // terrain_v2.png (tools/build_terrain.py): sheet width in cells and the
    // silhouette cells the foreground-silhouette plane draws (ART-13).
    inline constexpr int  TERRAIN_V2_COLS     = {tv2_cols};

    // first column of each zone after the meadow (tools/build_level.py)
    inline constexpr int  ZONE_RUINS_COL      = {zone_ruins};
    inline constexpr int  ZONE_SUNKEN_COL     = {zone_sunken};
    inline constexpr int  ZONE_ARENA_COL      = {zone_arena};
    inline constexpr int  SIL_CANOPY[3]       = {{ {sil_canopy} }};
    inline constexpr int  SIL_CANOPY_EDGE[3]  = {{ {sil_edge} }};
    inline constexpr int  SIL_ROOTS[2]        = {{ {sil_roots} }};
    inline constexpr int  SIL_ROOT_END[2]     = {{ {sil_root_end} }};

    // Traversal limits the physics can actually clear (see tools/build_level.py)
    inline constexpr int  MAX_STEP_UP       = {MAX_STEP_UP};
    inline constexpr int  MAX_GAP           = {MAX_GAP};

    // player spawn
    inline constexpr std::pair<int,int> PLAYER_TILE = {{{pc}, {pr}}};
    inline constexpr float PLAYER_X = {px:.4f}f;
    inline constexpr float PLAYER_Y = {py:.4f}f;

    // solid ground (col -> top row, -1 for chasm)
    inline constexpr int GROUND_TOP[{WIDTH}] = {{
{ground}
    }};

    // platforms
    struct PlatformDef {{ int col, row, w, h; bool moving, vertical; float range, speed; }};
    inline constexpr int N_PLATFORMS = {np};
    inline constexpr PlatformDef PLATFORMS[{np}] = {{
{platforms}
    }};

    // hazards (spikes)
    struct HazardDef {{ const char* kind; int col, row, w, h; }};
    inline constexpr int N_HAZARDS = {nh};
    inline constexpr HazardDef HAZARDS[{nh}] = {{
{hazards}
    }};

    // facilities (checkpoint / shrine / key / door)
    struct FacilityDef {{ const char* kind; int col, row; }};
    inline constexpr int N_FACILITIES = {nf};
    inline constexpr FacilityDef FACILITIES[{nf}] = {{
{facilities}
    }};

    // powerups
    struct PowerupDef {{ const char* kind; int col, row; }};
    inline constexpr int N_POWERUPS = {nw};
    inline constexpr PowerupDef POWERUPS[{nw}] = {{
{powerups}
    }};

    // enemies
    struct EnemyDef {{ const char* kind; int col, row; }};
    inline constexpr int N_ENEMIES = {ne};
    inline constexpr EnemyDef ENEMIES[{ne}] = {{
{enemies}
    }};

    // coin anchors
    struct CoinDef {{ int col, row; }};
    inline constexpr int N_COINS = {nc};
    inline constexpr CoinDef COINS[{nc}] = {{
{coins}
    }};

    // Scenery: whole props assembled from platformer.png.  `id` indexes
    // Objects::Id from src/Objects.h (generated by tools/build_objects.py) and
    // `row` is the tile row whose top edge the object stands on -- the sprite
    // is drawn with its bottom centre on that edge, so a two-tile tree simply
    // rises above it.  These are NOT tile ids: placing them by tile id used to
    // scatter severed tree halves around the level.
    struct SceneryDef {{ int id; int col, row; float scale; bool flip; }};
    inline constexpr int N_SCENERY = {ns};
    inline constexpr SceneryDef SCENERY[{ns}] = {{
{scenery}
    }};
}}
'''


def load_object_ids(root: str) -> Dict[str, int]:
    """Parse src/Objects.h so scenery kinds map to the real enum values.

    Reading the generated header rather than hard-coding the order means the
    two generators cannot drift apart if an object is added or reordered.
    """
    import re as _re
    path = os.path.join(root, "src", "Objects.h")
    ids = {}
    with open(path) as f:
        for name, val in _re.findall(r"OBJ_(\w+) = (\d+),", f.read()):
            ids[name.lower()] = int(val)
    if not ids:
        raise SystemExit("src/Objects.h has no object ids - run tools/build_objects.py")
    return ids


def _stage_args(L: Level, zones) -> Dict[str, object]:
    """The per-stage fields of Level.h (shared by stage 1 and S2/S3)."""
    def ints(vals):
        return ",\n".join(f"        {v}" for v in vals)
    pc, pr = L.player
    zstart = {name: a for name, a, _ in reversed(zones)}
    starts = [a for _, a, _ in zones]
    return dict(
        name=L.name, pc=pc, pr=pr, px=pc * TILE, py=pr * TILE,
        zone_ruins=starts[1], zone_sunken=starts[2], zone_arena=starts[3],
        ground=ints([-1 if (v is None or v < 0) else v for v in L.ground]),
        np=len(L.platforms),
        platforms=",\n".join(
            f'        {{ {p.col}, {p.row}, {p.w}, {p.h}, '
            f'{"true" if p.moving else "false"}, '
            f'{"true" if p.vertical else "false"}, '
            f'{float(p.range):.4f}f, {float(p.speed):.4f}f }}'
            for p in L.platforms),
        nh=len(L.hazards),
        hazards=",\n".join(
            f'        {{ "{h.kind}", {h.col}, {h.row}, {h.w}, {h.h} }}' for h in L.hazards),
        nf=len(L.facilities),
        facilities=",\n".join(
            f'        {{ "{f.kind}", {f.col}, {f.row} }}' for f in L.facilities),
        nw=len(L.powerups),
        powerups=",\n".join(
            f'        {{ "{p.kind}", {p.col}, {p.row} }}' for p in L.powerups),
        ne=len(L.enemies),
        enemies=",\n".join(
            f'        {{ "{e.kind}", {e.col}, {e.row} }}' for e in L.enemies),
        nc=len(L.coins),
        coins=",\n".join(f"        {{ {c}, {r} }}" for (c, r) in L.coins),
        ns=len(L.scenery),
        scenery=",\n".join(
            f'        {{ {OBJ_IDS[s.kind]}, {s.col}, {s.row}, '
            f'{float(s.scale):.4f}f, {"true" if s.flip else "false"} }}'
            for s in L.scenery),
    )


STAGE_TEMPLATE = '''
    // ---- stage {n}: {name} (tools/build_level.py STAGE_SPECS) ----
    namespace S{n} {{
        inline constexpr const char* NAME = "{name}";
        inline constexpr float PLAYER_X = {px:.4f}f;
        inline constexpr float PLAYER_Y = {py:.4f}f;
        inline constexpr int ZONE_RUINS_COL = {zone_ruins}, ZONE_SUNKEN_COL = {zone_sunken},
                             ZONE_ARENA_COL = {zone_arena};
        inline constexpr int GROUND_TOP[WIDTH_TILES] = {{
{ground}
        }};
        inline constexpr int N_PLATFORMS = {np};
        inline constexpr PlatformDef PLATFORMS[{np}] = {{
{platforms}
        }};
        inline constexpr int N_HAZARDS = {nh};
        inline constexpr HazardDef HAZARDS[{nh}] = {{
{hazards}
        }};
        inline constexpr int N_FACILITIES = {nf};
        inline constexpr FacilityDef FACILITIES[{nf}] = {{
{facilities}
        }};
        inline constexpr int N_POWERUPS = {nw};
        inline constexpr PowerupDef POWERUPS[{nw}] = {{
{powerups}
        }};
        inline constexpr int N_ENEMIES = {ne};
        inline constexpr EnemyDef ENEMIES[{ne}] = {{
{enemies}
        }};
        inline constexpr int N_COINS = {nc};
        inline constexpr int N_SCENERY = {ns};
        inline constexpr SceneryDef SCENERY[{ns}] = {{
{scenery}
        }};
    }}
'''

STAGE_TABLE = '''
    // ---- every stage, and which one is loaded (Batch B) ----
    struct StageData {{
        const char* name;  const char* dir;  int theme;
        float playerX, playerY;
        const int* groundTop;
        int zoneRuins, zoneSunken, zoneArena;
        const PlatformDef* platforms;   int nPlatforms;
        const HazardDef* hazards;       int nHazards;
        const FacilityDef* facilities;  int nFacilities;
        const PowerupDef* powerups;     int nPowerups;
        const EnemyDef* enemies;        int nEnemies;
        const SceneryDef* scenery;      int nScenery;
        int nCoins;
    }};
#define DR_STAGE(NS, DIR, THEME) {{ NS NAME, DIR, THEME, NS PLAYER_X, NS PLAYER_Y, NS GROUND_TOP, \\
        NS ZONE_RUINS_COL, NS ZONE_SUNKEN_COL, NS ZONE_ARENA_COL, NS PLATFORMS, NS N_PLATFORMS, \\
        NS HAZARDS, NS N_HAZARDS, NS FACILITIES, NS N_FACILITIES, NS POWERUPS, NS N_POWERUPS, \\
        NS ENEMIES, NS N_ENEMIES, NS SCENERY, NS N_SCENERY, NS N_COINS }}
    inline constexpr int N_STAGES = {n_stages};
    inline constexpr StageData STAGES[N_STAGES] = {{
        DR_STAGE(, "tileset/", 0),
{rows}
    }};
#undef DR_STAGE
    // Index of the stage that is loaded; Game::loadStage() sets it.
    inline int g_stage = 0;
    inline const StageData& stage() {{ return STAGES[g_stage]; }}
'''


def emit_header(out_path: str, L: Level, extra=()) -> None:
    """Stage 1 keeps the exact Level.h it always had; `extra` is a list of
    (StageSpec, Level) for stages 2..n, appended as Level::S2... plus the
    STAGES table the game reads through Level::stage()."""
    args = _stage_args(L, STAGE1_ZONES)
    txt = HEADER.format(
        tv2_cols=BT.COLS,
        sil_canopy=", ".join(str(BT.BG["CANOPY_%d" % i]) for i in range(3)),
        sil_edge=", ".join(str(BT.BG["CANOPY_EDGE_%d" % i]) for i in range(3)),
        sil_roots=", ".join(str(BT.BG["ROOTS_%d" % i]) for i in range(2)),
        sil_root_end=", ".join(str(BT.BG["ROOT_END_%d" % i]) for i in range(2)),
        TILE=TILE, WIDTH=L.width, HEIGHT=L.height, GROUND_ROW=GROUND_ROW,
        MAX_STEP_UP=MAX_STEP_UP, MAX_GAP=MAX_GAP, WATER_ROW=WATER_ROW,
        **args)
    if extra:
        blocks = []
        rows = []
        for n, (spec, Ls) in enumerate(extra, start=2):
            blocks.append(STAGE_TEMPLATE.format(n=n, **_stage_args(Ls, spec.zones)))
            rows.append(f'        DR_STAGE(S{n}::, "tileset/{spec.folder}/", {spec.theme})')
        txt += "\nnamespace Level {\n" + "".join(blocks) + STAGE_TABLE.format(
            n_stages=1 + len(extra), rows=",\n".join(rows)) + "}\n"
    with open(out_path, "w") as f:
        f.write(txt)


# ---------------------------------------------------------------------------
# main
# ---------------------------------------------------------------------------

def main() -> int:
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    tileset = os.path.join(root, "assets", "tileset")
    src = os.path.join(root, "src")

    global OBJ_IDS
    OBJ_IDS = load_object_ids(root)

    L = build_level()

    problems = validate_level(L)
    print(f"level      : {L.name}  {L.width}x{L.height} tiles")
    print(f"jump budget: {JUMP_HEIGHT:.0f} px high ({JUMP_HEIGHT/TILE:.2f} tiles), "
          f"{JUMP_LENGTH:.0f} px long ({JUMP_LENGTH/TILE:.2f} tiles)")
    print(f"limits     : step up <= {MAX_STEP_UP} tile, gap <= {MAX_GAP} tiles")
    if problems:
        print(f"\n*** {len(problems)} PLAYABILITY VIOLATIONS ***")
        for p in problems:
            print(f"  - {p}")
        return 1
    print("validation : OK - every step and gap is within the jump budget")

    os.makedirs(tileset, exist_ok=True)
    write_foreground(os.path.join(tileset, "ForegroundLayer.map"), L)
    write_background(os.path.join(tileset, "BackgroundLayer.map"), L)
    write_mid(os.path.join(tileset, "MidLayer.map"), L)
    write_decor(os.path.join(tileset, "DecorLayer.map"), L)
    write_props(os.path.join(tileset, "PropsLayer.map"),
                os.path.join(tileset, "PropsSideLayer.map"), L)
    write_coins(os.path.join(tileset, "CoinLayer.map"), L)
    write_colliders(os.path.join(tileset, "Colliders.txt"), L)

    # Batch B: stages 2..n from their recipes, each validated by the same
    # rules and written to its own folder.  ZONES is swapped while a stage is
    # built and written, because every zone-aware step reads it.
    global ZONES, SURFACE_MATERIAL
    extra = []
    for spec in STAGE_SPECS:
        ZONES = list(spec.zones)
        SURFACE_MATERIAL = "soil" if spec.theme == 2 else "teal"
        Ls = build_stage(spec)
        bad = validate_level(Ls)
        if bad:
            print(f"\n*** {spec.name}: {len(bad)} PLAYABILITY VIOLATIONS ***")
            for p_ in bad:
                print(f"  - {p_}")
            ZONES = list(STAGE1_ZONES)
            SURFACE_MATERIAL = "teal"
            return 1
        out = os.path.join(tileset, spec.folder)
        os.makedirs(out, exist_ok=True)
        write_foreground(os.path.join(out, "ForegroundLayer.map"), Ls)
        write_background(os.path.join(out, "BackgroundLayer.map"), Ls)
        write_mid(os.path.join(out, "MidLayer.map"), Ls)
        write_decor(os.path.join(out, "DecorLayer.map"), Ls)
        write_props(os.path.join(out, "PropsLayer.map"), os.path.join(out, "PropsSideLayer.map"), Ls)
        write_coins(os.path.join(out, "CoinLayer.map"), Ls)
        write_colliders(os.path.join(out, "Colliders.txt"), Ls)
        print(f"stage      : {spec.name:<14} validation OK - {len(Ls.enemies)} enemies, "
              f"{len(Ls.coins)} coins, {len(Ls.platforms)} platforms, {len(Ls.hazards)} spike strips, "
              f"{len(Ls.scenery)} scenery")
        extra.append((spec, Ls))
    ZONES = list(STAGE1_ZONES)
    SURFACE_MATERIAL = "teal"
    emit_header(os.path.join(src, "Level.h"), L, extra)

    print(f"wrote      : {len(L.platforms)} platforms, {len(L.enemies)} enemies, "
          f"{len(L.coins)} coins, {len(L.scenery)} scenery objects, "
          f"{len(L.decor)} decor tiles")

    # Coverage report: which of the sheet's 36 usable cells the level actually
    # draws.  The point of the exercise is to use the art that exists, so an
    # unused tile is a bug report rather than a curiosity.
    used: set = set()
    for rel in ("DecorLayer.map",):
        p = os.path.join(tileset, rel)
        if not os.path.exists(p):
            continue
        with open(p) as fh:
            for line in fh:
                for tok in line.split(","):
                    tok = tok.strip()
                    if tok and tok != str(EMPTY):
                        used.add(int(tok))
    missing = [t for t in ALL_USABLE_TILES if t not in used]
    print(f"tile usage : {len(used)} platformer.png cells still drawn by decor "
          f"(the rest is superseded by terrain_v2 / props_v2)")
    report_density(L)
    return report_v2(L)


def report_density(L: Level) -> None:
    """Visual items per walkable column, and how they cluster (LVL-01)."""
    props, side = place_props(L)
    surf = [c for c in range(L.width) if L.ground[c] is not None and L.ground[c] >= 0]
    per = {c: 0 for c in surf}
    for (c, _r, _t) in L.decor:
        if c in per: per[c] += 1
    for (c, _r) in list(props) + list(side):
        if c in per: per[c] += 1
    for s_ in L.scenery:
        if s_.col in per: per[s_.col] += 1
    total = sum(per.values())
    runs, run = [], 0
    for c in surf:
        if per[c]:
            run += 1
        elif run:
            runs.append(run); run = 0
    if run: runs.append(run)
    empty = sum(1 for c in surf if not per[c])
    print(f"density    : {total} items over {len(surf)} walkable columns = "
          f"{total / len(surf):.2f} per column (target 1.2-1.8); "
          f"{len(props)} props + {len(side)} side props, {len(L.decor)} decor, "
          f"{len(L.scenery)} scenery")
    print(f"clusters   : {len(runs)} runs, mean {sum(runs) / max(1, len(runs)):.1f} columns, "
          f"{empty} bare columns")


def report_v2(L: Level) -> int:
    """ART_STYLE_V2 6.5/6.6 measured on what was just written.

    Returns 1 (and the generator fails) if the background breaks its rule;
    the per-material foreground numbers are reported, not enforced, because
    a material's share depends on the layout as much as on the tile set.
    """
    from collections import Counter
    cells = terrain_cells(L)
    by_mat: Dict[str, Counter] = {}
    for (x, y), (_, material, _) in cells.items():
        by_mat.setdefault(material, Counter())[tile_for(x, y, cells, L.width, L.height)] += 1
    for material in BT.MATERIALS:
        c = by_mat.get(material, Counter())
        n = sum(c.values())
        if not n:
            continue
        tid, top = c.most_common(1)[0]
        print(f"fg {material:<6}  : {n:4d} cells, {len(c):2d} ids, "
              f"max share {100.0 * top / n:4.1f} % (id {tid})")
    bg = Counter(background_cells(L).values())
    n = sum(bg.values())
    tid, top = bg.most_common(1)[0]
    share = 100.0 * top / n
    print(f"background : {n:4d} cells, {len(bg):2d} ids, max share {share:4.1f} % (id {tid})")
    if len(bg) < 8 or share > 40.0:
        print("*** background layer breaks ART_STYLE_V2 6.6 (>= 8 ids, <= 40 %) ***")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
