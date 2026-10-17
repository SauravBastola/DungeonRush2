#!/usr/bin/env python3
"""
DungeonRush2 terrain tileset v2 (ART-10).

Writes assets/tileset/terrain_v2.png and owns the tile-id table that
tools/build_level.py imports, so the ids have exactly one home.

Why v2 exists
-------------
platformer.png has only three seamless cells (2, 5, 8), so the old generator
could only slice a run into left cap / fill / right cap.  91 % of the terrain
ended up as three ids and the background as one.  This sheet is generated, not
painted: every cell is a function of (material, depth band, piece, variant),
every pixel is a PAL colour or alpha 0, and two runs are byte-identical
(INV-11 - the only "randomness" is the integer hash _h()).

Sheet layout
------------
2048 x 1088 px = 32 columns x 17 rows of 64 px cells.  id = row * 32 + col.

    rows 0..14   terrain: row = material * 3 + band
                 material  0 teal  1 soil  2 stone  3 navy  4 water
                 band      0 = x1.00   1 = x0.85   2 = x0.72 mean luminance
    rows 15..16  background silhouettes (BG_* below)

So a terrain id is   (material * 3 + band) * 32 + piece.

Piece table (the column inside a terrain row)
---------------------------------------------
`mask` has a bit set for every side whose neighbour CONNECTS (is solid):
N = 1, E = 2, S = 4, W = 8.  A clear bit is an exposed side.

    piece  mask  exposed sides      reads as
    -----  ----  -----------------  ------------------------------
      0    0000  N E S W            isolated block
      1    0001  E S W              stub hanging from above
      2    0010  N S W              left end of a 1-high run
      3    0011  S W                bottom-left outer corner
      4    0100  N E W              stub standing on a mass
      5    0101  E W                1-wide vertical pillar
      6    0110  N W                top-left outer corner
      7    0111  W                  left wall            (variant 0)
      8    1000  N E S              right end of a 1-high run
      9    1001  E S                bottom-right outer corner
     10    1010  N S                1-high horizontal slab
     11    1011  S                  underside / ceiling  (variant 0)
     12    1100  N E                top-right outer corner
     13    1101  E                  right wall           (variant 0)
     14    1110  N                  walkable top         (variant 0)
     15    1111  -                  interior             (variant 0)
     16    1111  NE diagonal empty  inner corner
     17    1111  SE diagonal empty  inner corner
     18    1111  SW diagonal empty  inner corner
     19    1111  NW diagonal empty  inner corner
     20    1111  -                  interior             (variant 1)
     21    1111  -                  interior             (variant 2)
     22,23 1110  N                  walkable top         (variants 1, 2)
     24,25 1101  E                  right wall           (variants 1, 2)
     26,27 1011  S                  underside            (variants 1, 2)
     28,29 0111  W                  left wall            (variants 1, 2)
     30    1111  -                  interior             (variant 3)
     31     -    -                  reserved (transparent)

Pieces 0..15 are the 16-piece N/E/S/W set.  A 4-bit mask cannot see
diagonals, so the concave corner where a step meets a wall (both cardinals
solid, the diagonal empty) gets its own four cells, 16..19 - without them the
outline breaks at every step.  Variants exist for the interior AND the four
straight edges because the walkable top alone is a third of the teal cells:
with one top cell the "no id above 25 %" rule cannot be met.

Variants share every pixel within EDGE_KEEP px of the cell border, so any
variant abuts any other without a seam; they differ only in small interior
details.  Depth bands are the same art with an ordered (Bayer 4x4) dither
toward the next-darker step of each colour's ramp, tuned per material to
the x0.85 / x0.72 mean-luminance targets.  Dithering instead of a colour
multiply keeps every pixel inside PAL (ART_STYLE_V2 section 6 item 1).

The grass lip is NOT baked into these cells: Atmosphere::drawGrass already
draws it as an animated overlay on every surface, so the terrain cells carry
material only.
"""
import math
import os
import sys

from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from palette import PAL  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_PNG = os.path.join(ROOT, "assets", "tileset", "terrain_v2.png")

CELL = 64
COLS = 32
ROWS = 17
MATERIALS = ("teal", "soil", "stone", "navy", "water")
BANDS = 3
BAND_TARGET = (1.00, 0.85, 0.72)

N, E, S, W = 1, 2, 4, 8
EDGE_KEEP = 8                     # px of border every variant shares

# --- piece numbering (see the table in the docstring) -----------------------
INNER = {"NE": 16, "SE": 17, "SW": 18, "NW": 19}
INTERIOR_VARIANTS = (15, 20, 21, 30)
EDGE_VARIANTS = {14: (14, 22, 23), 13: (13, 24, 25),
                 11: (11, 26, 27), 7: (7, 28, 29)}
N_PIECES = 31


def terrain_id(material: str, band: int, piece: int) -> int:
    return (MATERIALS.index(material) * BANDS + band) * COLS + piece


def piece_for(mask: int, diag_empty, variant: int) -> int:
    """Select the piece for a 4-bit connect mask.

    `diag_empty` is the set of empty diagonals among {"NE","SE","SW","NW"};
    it only matters for a fully enclosed cell.  `variant` is 0..11 (12 buckets keep both % 4 and % 3 uniform):
    interiors use all four, straight edges use variant % 3.
    """
    if mask == 15:
        for k in ("NW", "NE", "SW", "SE"):       # lit corner wins a tie
            if k in diag_empty:
                return INNER[k]
        return INTERIOR_VARIANTS[variant % 4]
    if mask in EDGE_VARIANTS:
        return EDGE_VARIANTS[mask][variant % 3]
    return mask


# --- background silhouettes (rows 15..16) ---------------------------------
BG_ROW0 = 15
_BG_NAMES = (
    "CANOPY_0", "CANOPY_1", "CANOPY_2",          # dense hanging foliage
    "CANOPY_EDGE_0", "CANOPY_EDGE_1", "CANOPY_EDGE_2",   # scalloped underside
    "ROOTS_0", "ROOTS_1", "ROOT_END_0", "ROOT_END_1",     # hanging roots
    "TEETH_0", "TEETH_1", "TEETH_2", "RIDGE_TOP",        # ridge skyline
    "MASS_0", "MASS_1", "MASS_2",                        # ridge body
    "DEEP_0", "DEEP_1", "DEEP_2",                        # ridge body, lower
    "FOLIAGE_TOP_0", "FOLIAGE_TOP_1", "FOLIAGE_TOP_2",   # mid-layer mound tops
)
BG = {name: BG_ROW0 * COLS + i for i, name in enumerate(_BG_NAMES)}


# ---------------------------------------------------------------------------
# deterministic helpers
# ---------------------------------------------------------------------------
def _h(i: int) -> float:
    """Same murmur-style finalizer as build_level._h: 0..1, no RNG."""
    x = (i * 0x9E3779B1) & 0xFFFFFFFF
    x ^= x >> 15
    x = (x * 0x85EBCA6B) & 0xFFFFFFFF
    x ^= x >> 13
    x = (x * 0xC2B2AE35) & 0xFFFFFFFF
    x ^= x >> 16
    return (x & 0xFFFFFF) / float(0x1000000)


def _lum(c) -> float:
    def lin(v):
        v /= 255.0
        return v / 12.92 if v <= 0.04045 else ((v + 0.055) / 1.055) ** 2.4
    return 0.2126 * lin(c[0]) + 0.7152 * lin(c[1]) + 0.0722 * lin(c[2])


BAYER4 = ((0, 8, 2, 10), (12, 4, 14, 6), (3, 11, 1, 9), (15, 7, 13, 5))

# One step down each colour's own ramp.  Every value is a PAL colour, and
# every chain ends in INK, so a dithered band can never leave the palette.
DARKER = {
    PAL.INK: PAL.INK, PAL.NAVY_XD: PAL.INK, PAL.BROWN_XD: PAL.INK,
    PAL.NAVY: PAL.NAVY_XD, PAL.SLATE_XD: PAL.NAVY_XD, PAL.NAVY_DK: PAL.NAVY,
    PAL.BROWN_DK: PAL.BROWN_XD, PAL.TEAL_DK: PAL.NAVY, PAL.SLATE: PAL.SLATE_XD,
    PAL.BROWN: PAL.BROWN_DK, PAL.TEAL: PAL.TEAL_DK, PAL.WATER_DK: PAL.NAVY_DK,
    PAL.SLATE_LT: PAL.SLATE, PAL.TAN: PAL.BROWN, PAL.GRASS_MID: PAL.TEAL,
    PAL.WATER: PAL.WATER_DK, PAL.TAN_LT: PAL.TAN, PAL.GRASS: PAL.GRASS_MID,
    PAL.PLATE: PAL.SLATE_LT, PAL.CREAM: PAL.TAN_LT,
}


def _profile(t: int, seed: int) -> int:
    """Edge intrusion 0..3 px, periodic in 64 so stacked edges join."""
    a = 2 * math.pi * t / CELL
    v = (1.4 + 0.9 * math.sin(2 * a + seed * 1.7)
         + 0.6 * math.sin(5 * a + seed * 0.9)
         + 0.35 * math.sin(11 * a + seed * 2.3))
    return max(0, min(3, int(round(v))))


PROFILE = {side: [_profile(t, seed) for t in range(CELL)]
           for side, seed in ((N, 1), (E, 2), (S, 3), (W, 4))}
CORNER_R = 9


# ---------------------------------------------------------------------------
# geometry: which pixels of a piece are outline / rim / interior
# ---------------------------------------------------------------------------
def _model(piece: int):
    """3x3 solidity around the centre cell for a piece."""
    if piece in INNER.values():
        mask = 15
        diag = {v: k for k, v in INNER.items()}[piece]
        empty_diag = {diag}
    else:
        base = piece
        for m, vs in EDGE_VARIANTS.items():
            if piece in vs:
                base = m
        if piece in INTERIOR_VARIANTS:
            base = 15
        mask = base
        # a diagonal is empty when either adjacent side is exposed
        empty_diag = set()
        for d, (a, b) in {"NE": (N, E), "SE": (S, E),
                          "SW": (S, W), "NW": (N, W)}.items():
            if not (mask & a) or not (mask & b):
                empty_diag.add(d)
    solid = {(0, 0): True,
             (0, -1): bool(mask & N), (1, 0): bool(mask & E),
             (0, 1): bool(mask & S), (-1, 0): bool(mask & W),
             (1, -1): "NE" not in empty_diag, (1, 1): "SE" not in empty_diag,
             (-1, 1): "SW" not in empty_diag, (-1, -1): "NW" not in empty_diag}
    return solid


def _outside_fn(solid):
    def is_solid(cx, cy):
        return solid.get((cx, cy), True)          # beyond the model: solid

    def outside(x, y):
        cx, cy = x // CELL, y // CELL
        if not is_solid(cx, cy):
            return True
        lx, ly = x - cx * CELL, y - cy * CELL
        en = not is_solid(cx, cy - 1)
        ee = not is_solid(cx + 1, cy)
        es = not is_solid(cx, cy + 1)
        ew = not is_solid(cx - 1, cy)
        if en and ly < PROFILE[N][lx]:
            return True
        if es and (CELL - 1 - ly) < PROFILE[S][lx]:
            return True
        if ew and lx < PROFILE[W][ly]:
            return True
        if ee and (CELL - 1 - lx) < PROFILE[E][ly]:
            return True
        R = CORNER_R
        for exp_v, exp_h, ox, oy in ((en, ew, lx, ly),
                                     (en, ee, CELL - 1 - lx, ly),
                                     (es, ew, lx, CELL - 1 - ly),
                                     (es, ee, CELL - 1 - lx, CELL - 1 - ly)):
            if exp_v and exp_h and ox < R and oy < R:
                if (R - ox - 0.5) ** 2 + (R - oy - 0.5) ** 2 > R * R:
                    return True
        return False
    return outside


RIM_MAX = 4


def piece_classes(piece: int):
    """64x64 list of class codes.

    'o' outside, 'k' outline, 'L' lit rim, 'l' soft lit rim,
    'D' shaded rim, 'i' interior.  Light comes from the upper left.
    """
    if piece >= N_PIECES:
        return [["o"] * CELL for _ in range(CELL)]
    out = _outside_fn(_model(piece))
    pad = RIM_MAX + 1
    grid = {}
    for y in range(-pad, CELL + pad):
        for x in range(-pad, CELL + pad):
            grid[(x, y)] = out(x, y)
    cls = []
    for y in range(CELL):
        row = []
        for x in range(CELL):
            if grid[(x, y)]:
                row.append("o")
                continue
            code = "i"
            for r in range(1, RIM_MAX + 1):
                sx = sy = hits = 0
                for dy in range(-r, r + 1):
                    for dx in range(-r, r + 1):
                        if max(abs(dx), abs(dy)) != r:
                            continue
                        if grid[(x + dx, y + dy)]:
                            sx += dx
                            sy += dy
                            hits += 1
                if hits:
                    lit = (sx + sy) < 0 or ((sx + sy) == 0 and sy < 0)
                    if r == 1:
                        code = "k"
                    elif r == 2:
                        code = "L" if lit else "D"
                    elif r == 3:
                        code = "l" if lit else "D"
                    else:
                        code = "l" if lit else "i"
                    break
            row.append(code)
        cls.append(row)
    return cls


# ---------------------------------------------------------------------------
# materials: interior texture (64-periodic) and rim colours
# ---------------------------------------------------------------------------
def _wrap_blob(tex, cx, cy, rx, ry, paint):
    """Call paint(x, y, u, v) for every pixel of a wrapped ellipse.

    u, v are -1..1 offsets inside the ellipse so the painter can shade.
    """
    for dy in range(-ry - 1, ry + 2):
        for dx in range(-rx - 1, rx + 2):
            u = dx / max(rx, 0.5)
            v = dy / max(ry, 0.5)
            d = u * u + v * v
            if d <= 1.0:
                paint((cx + dx) % CELL, (cy + dy) % CELL, u, v, d)


def _pebbles(tex, seed, count, rmin, rmax, body, hi, shade, line, keep=None):
    for k in range(count):
        cx = int(_h(seed + k * 7) * CELL)
        cy = int(_h(seed + k * 7 + 1) * CELL)
        rx = rmin + int(_h(seed + k * 7 + 2) * (rmax - rmin + 1))
        ry = max(2, rx - int(_h(seed + k * 7 + 3) * 2))
        if keep and not keep(cx, cy, rx, ry):
            continue

        def paint(x, y, u, v, d, rx=rx, ry=ry):
            edge = d > (1.0 - 2.2 / max(rx, ry))
            if edge and (u + v) > -0.2:
                tex[y][x] = line
            elif (u + v) < -0.75 and d < 0.75:
                tex[y][x] = hi
            elif (u + v) > 0.55:
                tex[y][x] = shade
            else:
                tex[y][x] = body
        _wrap_blob(tex, cx, cy, rx, ry, paint)


def _specks(tex, seed, count, colour, keep=None):
    for k in range(count):
        x = int(_h(seed + k * 3) * CELL)
        y = int(_h(seed + k * 3 + 1) * CELL)
        if keep and not keep(x, y, 1, 1):
            continue
        tex[y][x] = colour
        if _h(seed + k * 3 + 2) < 0.5:
            tex[y][(x + 1) % CELL] = colour


def _inside(cx, cy, rx, ry):
    """Detail fully inside the variant zone (never touches EDGE_KEEP)."""
    return (EDGE_KEEP + rx + 2 <= cx <= CELL - EDGE_KEEP - rx - 3 and
            EDGE_KEEP + ry + 2 <= cy <= CELL - EDGE_KEEP - ry - 3)


def _touches(cx, cy, rx, ry):
    return not _inside(cx, cy, rx, ry)


def _base(colour):
    return [[colour] * CELL for _ in range(CELL)]


def _layered(base, features, variant):
    """Shared border, per-variant interior.

    `features(tex, seed, keep)` draws a material's whole feature set.  Every
    variant draws the seed-0 features that touch the border band, so the
    outer EDGE_KEEP px are identical across variants and any variant abuts
    any other; the interior is a different seed per variant, which is what
    breaks the 64 px repeat (variant 0 is simply seed 0 throughout).
    """
    t = _base(base)
    features(t, 0, _touches)
    features(t, variant * 7919, _inside)
    return t


def _moss(t, seed, keep):
    for k in range(14):
        cx = int(_h(101 + seed + k * 5) * CELL)
        cy = int(_h(102 + seed + k * 5) * CELL)
        r = 3 + int(_h(103 + seed + k * 5) * 4)
        if not keep(cx, cy, r, r):
            continue

        def paint(x, y, u, v, d):
            if d > 0.55 or (u + v) > -0.3:
                t[y][x] = PAL.TEAL_DK
        _wrap_blob(t, cx, cy, r, max(2, r - 1), paint)
        t[(cy - r) % CELL][(cx - 1) % CELL] = PAL.GRASS_MID
        t[(cy - r) % CELL][cx % CELL] = PAL.GRASS_MID
    _pebbles(t, 900 + seed, 4, 2, 3, PAL.TEAL_DK, PAL.GRASS_MID, PAL.NAVY,
             PAL.NAVY, keep=keep)
    _specks(t, 140 + seed, 14, PAL.NAVY, keep=keep)
    _specks(t, 170 + seed, 8, PAL.GRASS_MID, keep=keep)


def _tex_teal(variant):
    return _layered(PAL.TEAL, _moss, variant)


def _tex_soil(variant):
    def f(t, seed, keep):
        _pebbles(t, 200 + seed, 16, 3, 6, PAL.BROWN, PAL.TAN, PAL.BROWN_DK,
                 PAL.INK, keep=keep)
        _specks(t, 260 + seed, 14, PAL.BROWN_DK, keep=keep)
    return _layered(PAL.BROWN_XD, f, variant)


def _tex_stone(variant):
    def f(t, seed, keep):
        _pebbles(t, 300 + seed, 13, 4, 7, PAL.SLATE_XD, PAL.SLATE, PAL.NAVY_XD,
                 PAL.INK, keep=keep)
        _specks(t, 360 + seed, 16, PAL.INK, keep=keep)
        _specks(t, 380 + seed, 4, PAL.SLATE, keep=keep)
    return _layered(PAL.NAVY_XD, f, variant)


def _tex_navy(variant):
    def f(t, seed, keep):
        _pebbles(t, 400 + seed, 12, 3, 6, PAL.SLATE_XD, PAL.SLATE, PAL.NAVY_XD,
                 PAL.NAVY_XD, keep=keep)
        _specks(t, 460 + seed, 12, PAL.WATER_DK, keep=keep)
        _specks(t, 480 + seed, 10, PAL.NAVY_XD, keep=keep)
    return _layered(PAL.NAVY, f, variant)


def _tex_water(variant):
    t = _base(PAL.NAVY_DK)
    for y in range(CELL):
        for x in range(CELL):
            a = 2 * math.pi * x / CELL
            w = y + 2.0 * math.sin(2 * a) + 1.0 * math.sin(3 * a + 1.0)
            if int(w) % 16 in (0, 1):
                t[y][x] = PAL.WATER_DK
            elif int(w) % 16 == 8:
                t[y][x] = PAL.NAVY
    if variant:
        _specks(t, 2100 + variant * 50, 6, PAL.WATER, keep=_inside)
    return t


TEXTURE = {"teal": _tex_teal, "soil": _tex_soil, "stone": _tex_stone,
           "navy": _tex_navy, "water": _tex_water}

# outline, lit rim, soft lit rim, shaded rim
RIM = {
    "teal":  (PAL.INK, PAL.GRASS_MID, PAL.TEAL, PAL.TEAL_DK),
    "soil":  (PAL.INK, PAL.TAN, PAL.BROWN, PAL.BROWN_DK),
    "stone": (PAL.INK, PAL.SLATE, PAL.SLATE_XD, PAL.INK),
    "navy":  (PAL.INK, PAL.SLATE, PAL.SLATE_XD, PAL.NAVY_XD),
    # water has no ink line: its exposed top is the bright surface
    "water": (PAL.WATER, PAL.WATER, PAL.WATER_DK, PAL.NAVY),
}


def _variant_of(piece: int) -> int:
    if piece in INTERIOR_VARIANTS:
        return INTERIOR_VARIANTS.index(piece)
    for vs in EDGE_VARIANTS.values():
        if piece in vs:
            return vs.index(piece)
    return 0


def paint_piece(material: str, piece: int, classes, textures):
    tex = textures[_variant_of(piece)]
    ink, lit, soft, shade = RIM[material]
    out = []
    for y in range(CELL):
        row = []
        for x in range(CELL):
            c = classes[y][x]
            if c == "o":
                row.append(None)
            elif c == "k":
                row.append(ink)
            elif c == "L":
                row.append(lit)
            elif c == "l":
                row.append(soft if tex[y][x] not in (PAL.INK,) else tex[y][x])
            elif c == "D":
                row.append(shade)
            else:
                row.append(tex[y][x])
        out.append(row)
    return out


def darken(cell, coverage: float):
    """Ordered dither one ramp step darker over `coverage` of the pixels."""
    if coverage <= 0:
        return cell
    out = []
    for y, row in enumerate(cell):
        nr = []
        for x, c in enumerate(row):
            if c is not None and BAYER4[y % 4][x % 4] < coverage * 16:
                c = DARKER.get(c, c)
            nr.append(c)
        out.append(nr)
    return out


def _mean_lum(cell) -> float:
    vals = [_lum(c) for row in cell for c in row if c is not None]
    return sum(vals) / len(vals)


def band_coverage(material: str, interior) -> list:
    """Per band, the dither coverage whose mean luminance is closest to target."""
    l0 = _mean_lum(interior)
    res = [0.0]
    for target in BAND_TARGET[1:]:
        best, bd = 0.0, None
        for k in range(17):
            f = k / 16.0
            r = _mean_lum(darken(interior, f)) / l0
            if bd is None or abs(r - target) < bd:
                best, bd = f, abs(r - target)
        res.append(best)
    return res


# ---------------------------------------------------------------------------
# background silhouettes
# ---------------------------------------------------------------------------
def _bg_cells():
    """20 cells: canopy, hanging roots, ridge teeth and ridge body.

    Ridge cells are the background layer (hazy); canopy and root cells are
    the foreground silhouette plane (Game::drawForegroundSilhouettes).  Horizontally periodic in 64 so any two cells of a row abut.
    """
    cells = {}
    # ART-13: the ridge is light haze (Depth::BG_TILES lifts it further); the
    # canopy cells stay dark - they are drawn by the foreground-silhouette
    # plane, not by the background layer.
    hi, body, dk, xd = PAL.PLATE, PAL.SLATE_LT, PAL.SLATE, PAL.SLATE_XD
    leaf, leaf_dk = PAL.TEAL, PAL.TEAL_DK

    def canopy(v):
        t = _base(leaf_dk)
        for k in range(12):
            cx = int(_h(3000 + v * 97 + k * 5) * CELL)
            cy = int(_h(3001 + v * 97 + k * 5) * CELL)
            r = 4 + int(_h(3002 + v * 97 + k * 5) * 5)

            def paint(x, y, u, vv, d):
                t[y][x] = leaf if (u + vv) < -0.4 else (
                    PAL.NAVY if d > 0.7 and (u + vv) > 0.3 else leaf_dk)
            _wrap_blob(t, cx, cy, r, r - 1, paint)
        return t

    for v in range(3):
        cells["CANOPY_%d" % v] = canopy(v)
        t = canopy(v)
        # scalloped lower edge: leaf clumps hanging to different depths
        for x in range(CELL):
            a = 2 * math.pi * x / CELL
            edge = 34 + 7 * math.sin(4 * a + v) + 4 * math.sin(9 * a + 2 * v)
            edge += 6 * max(0.0, math.sin(2 * a + v * 2.1))
            for y in range(CELL):
                if y > edge:
                    t[y][x] = None
                elif y > edge - 2:
                    t[y][x] = PAL.NAVY
        cells["CANOPY_EDGE_%d" % v] = t

    def roots(v, end):
        t = [[None] * CELL for _ in range(CELL)]
        for k in range(5):
            x0 = int(_h(3500 + v * 31 + k * 3) * CELL)
            length = CELL if not end else 20 + int(_h(3501 + v * 31 + k * 3) * 36)
            for y in range(length):
                x = (x0 + int(round(1.5 * math.sin(y / 9.0 + k)))) % CELL
                t[y][x] = leaf_dk
                t[y][(x + 1) % CELL] = PAL.NAVY
                if (y + k * 7) % 13 == 0:
                    t[y][(x - 1) % CELL] = leaf
                    t[y][(x + 2) % CELL] = leaf
        return t

    for v in range(2):
        cells["ROOTS_%d" % v] = roots(v, False)
        cells["ROOT_END_%d" % v] = roots(v, True)

    def mass(v, deep):
        t = _base(xd if deep else dk)
        # tree trunks and rock seams: vertical stripes, periodic
        for k in range(6):
            x0 = int(_h(4000 + v * 53 + k * 3 + deep * 500) * CELL)
            w = 2 + int(_h(4001 + v * 53 + k * 3) * 3)
            for y in range(CELL):
                for dx in range(w):
                    t[y][(x0 + dx) % CELL] = dk if deep else body
                t[y][(x0 - 1) % CELL] = xd if deep else dk
        _specks(t, 4200 + v * 40 + deep * 300, 10, body if deep else hi)
        return t

    for v in range(3):
        cells["MASS_%d" % v] = mass(v, 0)
        cells["DEEP_%d" % v] = mass(v, 1)

    def skyline(v, teeth):
        t = mass(v, 0)
        for x in range(CELL):
            a = 2 * math.pi * x / CELL
            if teeth:
                # conifer / spire tips: sharp triangles of varying height
                tri = abs(((x + v * 21) % 32) - 16) / 16.0
                top = 6 + 30 * tri + 6 * math.sin(3 * a + v)
            else:
                top = 30 + 4 * math.sin(2 * a + v) + 2 * math.sin(7 * a)
            for y in range(CELL):
                if y < top:
                    t[y][x] = None
                elif y < top + 2:
                    t[y][x] = hi
                elif y < top + 4:
                    t[y][x] = body
        return t

    # mid-layer foliage mounds: canopy texture under a scalloped, lit crown
    for v in range(3):
        t = canopy(v)
        for x in range(CELL):
            a = 2 * math.pi * x / CELL
            top = 26 + 8 * math.sin(3 * a + v * 1.3) + 4 * math.sin(7 * a + v)
            top -= 6 * max(0.0, math.sin(a + v * 2.0))
            for y in range(CELL):
                if y < top:
                    t[y][x] = None
                elif y < top + 1:
                    t[y][x] = PAL.INK
                elif y < top + 3 and t[y][x] != PAL.NAVY:
                    t[y][x] = PAL.GRASS_MID
        cells["FOLIAGE_TOP_%d" % v] = t

    for v in range(3):
        cells["TEETH_%d" % v] = skyline(v, True)
    cells["RIDGE_TOP"] = skyline(1, False)
    return cells


# ---------------------------------------------------------------------------
# sheet
# ---------------------------------------------------------------------------
def build():
    sheet = Image.new("RGBA", (COLS * CELL, ROWS * CELL), (0, 0, 0, 0))
    px = sheet.load()

    def blit(cell, col, row):
        for y in range(CELL):
            for x in range(CELL):
                c = cell[y][x]
                if c is not None:
                    px[col * CELL + x, row * CELL + y] = (c[0], c[1], c[2], 255)

    classes = [piece_classes(p) for p in range(N_PIECES)]
    report = []
    for m, material in enumerate(MATERIALS):
        textures = [TEXTURE[material](v) for v in range(4)]
        cov = band_coverage(material, textures[0])
        l0 = _mean_lum(textures[0])
        achieved = [_mean_lum(darken(textures[0], f)) / l0 for f in cov]
        report.append((material, cov, achieved))
        for band in range(BANDS):
            for p in range(N_PIECES):
                cell = paint_piece(material, p, classes[p], textures)
                blit(darken(cell, cov[band]), p, m * BANDS + band)
    bg = _bg_cells()
    for name, tid in BG.items():
        blit(bg[name], tid % COLS, tid // COLS)
    return sheet, report


def main() -> int:
    sheet, report = build()
    os.makedirs(os.path.dirname(OUT_PNG), exist_ok=True)
    sheet.save(OUT_PNG, optimize=False)
    print("sheet      : %s  %dx%d  (%d x %d cells of %d px)"
          % (os.path.relpath(OUT_PNG, ROOT), sheet.width, sheet.height,
             COLS, ROWS, CELL))
    print("terrain    : %d materials x %d bands x %d pieces = %d cells"
          % (len(MATERIALS), BANDS, N_PIECES, len(MATERIALS) * BANDS * N_PIECES))
    print("background : %d silhouette cells, ids %d..%d"
          % (len(BG), min(BG.values()), max(BG.values())))
    for material, cov, achieved in report:
        print("depth      : %-6s dither %s -> mean luminance x%s"
              % (material, "/".join("%.3f" % c for c in cov),
                 "/".join("%.2f" % a for a in achieved)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
