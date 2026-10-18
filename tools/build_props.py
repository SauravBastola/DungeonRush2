#!/usr/bin/env python3
"""
DungeonRush2 props sheet (Batch A / LVL-01).

Writes assets/tileset/props_v2.png (+ assets/svg/props_v2.svg) and owns the
prop-id table tools/build_level.py places from.  Same 1:1 toolkit as the
enemy roster (build_art.Px): flat parts, upper-left light, INK outline grown
outside the shape, every pixel a PAL colour.

Each prop is one 64x64 cell that stands on the cell's bottom row, so the
generator drops it into the empty cell directly above a walkable surface.
HANG_* props hang from the cell's top row and go under a platform instead.

    id  name            zone use
    --  --------------  ---------------------------------------------
     0  FERN            meadow, sunken
     1  FLOWERS         meadow
     2  MUSHROOMS       meadow, sunken
     3  STUMP           meadow
     4  GRASS_TALL      meadow, ruins
     5  ROCKS           all
     6  CRATE           ruins, arena
     7  CRATES          ruins
     8  BARREL          ruins
     9  LANTERN         ruins, arena (warm light accent)
    10  SIGNPOST        ruins (also marks a zone entrance)
    11  PILLAR          ruins, arena
    12  BONES           sunken, arena
    13  CRYSTALS        sunken
    14  BANNER          arena
    15  SKULL_POST      arena
    16  HANG_VINES      under platforms (all)
    17  HANG_ROOTS      under platforms (sunken)
 18-25  *_L / *_R       small fern, grass, rocks, mushrooms in the left or
                        right quarter of the cell, beside a centred decor item
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from palette import PAL                                        # noqa: E402
from build_art import Px, _el, _rc, _pg, _ln, _sheet          # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_PNG = os.path.join(ROOT, "assets", "tileset", "props_v2.png")
OUT_SVG = os.path.join(ROOT, "assets", "svg", "props_v2.svg")

COLS = 8
NAMES = ("FERN", "FLOWERS", "MUSHROOMS", "STUMP", "GRASS_TALL", "ROCKS",
         "CRATE", "CRATES", "BARREL", "LANTERN", "SIGNPOST", "PILLAR",
         "BONES", "CRYSTALS", "BANNER", "SKULL_POST", "HANG_VINES", "HANG_ROOTS",
         "FERN_L", "FERN_R", "GRASS_L", "GRASS_R", "ROCKS_L", "ROCKS_R",
         "SHROOM_L", "SHROOM_R",
         # Batch I - appended, so every older id keeps its number:
         # items cut out of the old tileset's packed cells (they were placed
         # as whole 64 px tiles, which left the top-row stones floating
         # above the bottom-row boulder and staff) ...
         "BOULDER", "STONE", "ORE_ROCK", "STAFF", "CRYSTAL_ROCK",
         "PINE_TOP", "PINE_BOT", "AUTUMN_TOP", "AUTUMN_BOT",
         # ... and new ones drawn here
         "LOG", "BUSH", "MENHIR", "WHEEL")
PROP = {n: i for i, n in enumerate(NAMES)}

# What each zone draws from, most common first (the placer weights by order).
ZONE_SET = {
    "meadow": ("FERN", "FLOWERS", "BUSH", "GRASS_TALL", "PINE", "MUSHROOMS", "LOG",
               "ROCKS", "STUMP", "BOULDER"),
    "ruins":  ("ROCKS", "GRASS_TALL", "AUTUMN", "CRATE", "PILLAR", "WHEEL", "BARREL",
               "MENHIR", "LANTERN", "STONE", "CRATES", "SIGNPOST"),
    "sunken": ("ROCKS", "FERN", "CRYSTAL_ROCK", "PINE", "CRYSTALS", "ORE_ROCK",
               "MUSHROOMS", "BOULDER", "BONES"),
    "arena":  ("ROCKS", "BONES", "ORE_ROCK", "PILLAR", "STAFF", "LANTERN", "BOULDER",
               "BANNER", "SKULL_POST", "CRATE"),
}
# Two-cell props: the name in ZONE_SET places <NAME>_BOT on the surface cell
# and <NAME>_TOP in the cell above - only where both cells are free.
TALL = {"PINE", "AUTUMN"}
ZONE_HANG = {"meadow": "HANG_VINES", "ruins": "HANG_VINES",
             "sunken": "HANG_ROOTS", "arena": "HANG_ROOTS"}
# Props a player could mistake for a platform are kept off jump take-offs.
SOLID_LOOKING = {"CRATE", "CRATES", "BARREL", "PILLAR", "STUMP",
                 "BOULDER", "ORE_ROCK", "MENHIR", "LOG"}
# Side props: drawn in one quarter of the cell, so they tuck in beside a
# centred decor item instead of replacing it.  Zone -> (left, right) names.
SIDE_SET = {
    "meadow": ("FERN", "GRASS", "SHROOM"),
    "ruins":  ("GRASS", "ROCKS"),
    "sunken": ("FERN", "ROCKS", "SHROOM"),
    "arena":  ("ROCKS", "GRASS"),
}
# Props that fill only the bottom of the cell; allowed under a floating coin.
LOW = {"FERN", "FLOWERS", "GRASS_TALL", "ROCKS", "MUSHROOMS", "BONES",
       "STONE", "BUSH", "LOG"}

B = 62          # last opaque row of the shape; the outline lands on 63


def _fern():
    c = Px(64, 64)
    for i, (dx, h, lean) in enumerate(((-10, 18, -7), (-4, 24, -3), (2, 26, 2),
                                       (8, 22, 6), (14, 15, 9))):
        x0 = 30 + dx
        pts = [(x0, B), (x0 + lean // 2, B - h // 2), (x0 + lean, B - h)]
        c.part(_ln(pts, 3), PAL.GRASS_MID, PAL.GRASS, PAL.TEAL)
        for k in range(3):                    # leaflets along the frond
            y = B - h // 3 - k * h // 4
            x = x0 + lean * (h - (B - y)) // h // 2
            c.part(_ln([(x, y), (x + (4 if lean >= 0 else -4), y - 2)], 2),
                   PAL.GRASS_MID, PAL.GRASS, PAL.TEAL)
    return c.outline()


def _flowers():
    c = Px(64, 64)
    c.part(_el(12, B - 7, 52, B + 1), PAL.GRASS_MID, PAL.GRASS, PAL.TEAL)
    for i, (x, h, col) in enumerate(((18, 12, PAL.PINK), (26, 16, PAL.YELLOW),
                                     (34, 11, PAL.CREAM), (41, 15, PAL.PINK),
                                     (48, 10, PAL.YELLOW))):
        c.part(_ln([(x, B - 3), (x, B - h)], 1), PAL.TEAL)
        c.part(_el(x - 2, B - h - 2, x + 2, B - h + 2), col)
        c.dot(x, B - h, PAL.ORANGE if col != PAL.CREAM else PAL.GOLD)
    return c.outline()


def _mushrooms():
    c = Px(64, 64)
    for x, h, r in ((22, 14, 6), (34, 20, 8), (44, 10, 5)):
        c.part(_rc(x - 1, B - h, x + 2, B), PAL.CREAM, None, PAL.TAN)
        c.part(_pg([(x - r, B - h + 1), (x - r + 2, B - h - r + 1), (x + r - 1, B - h - r + 1),
                    (x + r + 1, B - h + 1)]), PAL.RED, PAL.PINK, PAL.RED_DK)
        c.dot(x - 2, B - h - r // 2, PAL.CREAM); c.dot(x + 2, B - h - 2, PAL.CREAM)
    return c.outline()


def _stump():
    c = Px(64, 64)
    c.part(_rc(20, B - 16, 44, B), PAL.BROWN, PAL.TAN, PAL.BROWN_DK)
    c.part(_pg([(16, B), (20, B - 5), (20, B)]), PAL.BROWN, None, PAL.BROWN_DK)
    c.part(_pg([(48, B), (44, B - 5), (44, B)]), PAL.BROWN_DK)
    c.part(_el(20, B - 20, 44, B - 13), PAL.TAN, PAL.TAN_LT, PAL.BROWN)
    c.part(_el(27, B - 18, 37, B - 15), PAL.TAN_LT)
    c.part(_ln([(42, B - 24), (46, B - 30)], 2), PAL.GRASS_MID, PAL.GRASS, PAL.TEAL)
    return c.outline()


def _grass_tall():
    c = Px(64, 64)
    for i in range(9):
        x = 14 + i * 4
        h = 12 + (i * 7) % 13
        lean = ((i * 5) % 7) - 3
        c.part(_ln([(x, B), (x + lean, B - h)], 2),
               PAL.GRASS_MID if i % 3 else PAL.GRASS, PAL.GRASS, PAL.TEAL)
    return c.outline()


def _rocks():
    c = Px(64, 64)
    c.part(_el(14, B - 14, 38, B + 2), PAL.SLATE, PAL.SLATE_LT, PAL.SLATE_XD)
    c.part(_el(32, B - 9, 50, B + 2), PAL.SLATE, PAL.SLATE_LT, PAL.SLATE_XD, sep=True)
    c.part(_el(24, B - 6, 32, B + 1), PAL.SLATE_XD, PAL.SLATE, PAL.NAVY_XD, sep=True)
    c.part(_ln([(18, B - 9), (22, B - 11)], 1), PAL.GRASS_MID)
    return c.outline()


def _crate(c, x0, y1, w=22):
    y0 = y1 - w
    c.part(_rc(x0, y0, x0 + w, y1), PAL.BROWN, PAL.TAN, PAL.BROWN_DK, sep=True)
    c.part(_rc(x0 + 2, y0 + 2, x0 + w - 2, y1 - 2), PAL.BROWN_DK)
    c.part(_ln([(x0 + 3, y1 - 3), (x0 + w - 3, y0 + 3)], 3), PAL.BROWN, PAL.TAN, PAL.BROWN_DK)
    for (px, py) in ((x0 + 2, y0 + 2), (x0 + w - 2, y0 + 2), (x0 + 2, y1 - 2), (x0 + w - 2, y1 - 2)):
        c.dot(px, py, PAL.SLATE_LT)


def _crate1():
    c = Px(64, 64)
    _crate(c, 20, B, 24)
    return c.outline()


def _crates():
    c = Px(64, 64)
    _crate(c, 10, B, 24)
    _crate(c, 34, B, 20)
    _crate(c, 20, B - 25, 20)
    return c.outline()


def _barrel():
    c = Px(64, 64)
    c.part(_el(20, B - 30, 44, B + 1), PAL.BROWN, PAL.TAN, PAL.BROWN_DK)
    c.clear(_rc(18, B - 33, 46, B - 27))
    c.part(_rc(21, B - 27, 43, B - 25), PAL.BROWN, PAL.TAN, PAL.BROWN_DK)
    for y in (B - 22, B - 6):
        c.part(_rc(20, y, 44, y + 2), PAL.SLATE, PAL.SLATE_LT, PAL.SLATE_XD)
    c.part(_ln([(28, B - 24), (28, B - 2)], 1), PAL.BROWN_DK)
    c.part(_ln([(36, B - 24), (36, B - 2)], 1), PAL.BROWN_DK)
    return c.outline()


def _lantern():
    c = Px(64, 64)
    c.part(_rc(30, B - 50, 33, B), PAL.BROWN_DK, PAL.BROWN, PAL.BROWN_XD)
    c.part(_rc(26, B - 2, 37, B), PAL.SLATE, PAL.SLATE_LT, PAL.SLATE_XD)
    c.part(_ln([(31, B - 50), (40, B - 50), (40, B - 46)], 2), PAL.BROWN_DK)
    c.part(_rc(36, B - 46, 44, B - 34), PAL.ORANGE, PAL.YELLOW, PAL.ORANGE_DK)
    c.part(_rc(38, B - 43, 42, B - 37), PAL.YELLOW, PAL.CREAM, PAL.GOLD)
    c.part(_rc(35, B - 48, 45, B - 46), PAL.SLATE_XD, PAL.SLATE)
    c.part(_rc(35, B - 34, 45, B - 33), PAL.SLATE_XD)
    return c.outline()


def _signpost():
    c = Px(64, 64)
    c.part(_rc(29, B - 40, 32, B), PAL.BROWN_DK, PAL.BROWN, PAL.BROWN_XD)
    c.part(_pg([(16, B - 40), (42, B - 40), (48, B - 35), (42, B - 30), (16, B - 30)]),
           PAL.TAN, PAL.TAN_LT, PAL.BROWN)
    c.part(_ln([(21, B - 35), (39, B - 35)], 1), PAL.BROWN_DK)
    c.part(_ln([(24, B), (36, B)], 2), PAL.GRASS_MID, PAL.GRASS)
    return c.outline()


def _pillar():
    c = Px(64, 64)
    c.part(_rc(22, B - 44, 42, B), PAL.SLATE_LT, PAL.PLATE, PAL.SLATE)
    c.part(_rc(18, B - 4, 46, B), PAL.SLATE, PAL.SLATE_LT, PAL.SLATE_XD, sep=True)
    for x in (27, 32, 37):
        c.part(_ln([(x, B - 40), (x, B - 6)], 1), PAL.SLATE)
    c.clear(_pg([(22, B - 44), (31, B - 44), (26, B - 38), (22, B - 41)]))   # broken top
    c.part(_pg([(31, B - 44), (42, B - 44), (42, B - 48), (36, B - 47)]),
           PAL.SLATE_LT, PAL.PLATE, PAL.SLATE)
    c.part(_ln([(24, B - 20), (30, B - 26)], 2), PAL.GRASS_MID, PAL.GRASS)
    return c.outline()


def _bones():
    c = Px(64, 64)
    c.part(_ln([(14, B - 2), (34, B - 6)], 3), PAL.CREAM, None, PAL.TAN)
    c.part(_ln([(30, B - 1), (48, B - 3)], 3), PAL.CREAM, None, PAL.TAN, sep=True)
    c.part(_el(20, B - 14, 32, B - 3), PAL.CREAM, None, PAL.TAN, sep=True)
    c.dot(23, B - 9, PAL.INK); c.dot(27, B - 9, PAL.INK); c.dot(25, B - 6, PAL.INK)
    return c.outline()


def _crystals():
    c = Px(64, 64)
    for x, h, w in ((22, 18, 5), (32, 30, 7), (42, 20, 5), (27, 12, 4)):
        c.part(_pg([(x - w, B), (x - w + 1, B - h + w), (x, B - h), (x + w - 1, B - h + w), (x + w, B)]),
               PAL.VIOLET, PAL.ORCHID, PAL.NAVY_DK, sep=True)
        c.part(_ln([(x - 1, B - h + 3), (x - 1, B - 3)], 1), PAL.BLUSH)
    return c.outline()


def _banner():
    c = Px(64, 64)
    c.part(_rc(22, B - 54, 25, B), PAL.BROWN_DK, PAL.BROWN, PAL.BROWN_XD)
    c.part(_pg([(25, B - 52), (46, B - 52), (42, B - 40), (46, B - 28), (25, B - 30)]),
           PAL.RED, PAL.PINK, PAL.RED_DK)
    c.part(_el(31, B - 46, 39, B - 38), PAL.GOLD, PAL.YELLOW, PAL.ORANGE_DK)
    return c.outline()


def _skull_post():
    c = Px(64, 64)
    c.part(_rc(30, B - 34, 33, B), PAL.BROWN_DK, PAL.BROWN, PAL.BROWN_XD)
    c.part(_el(25, B - 46, 39, B - 33), PAL.CREAM, None, PAL.TAN)
    c.part(_rc(28, B - 35, 36, B - 31), PAL.CREAM, None, PAL.TAN)
    for x in (29, 34):
        c.dot(x, B - 40, PAL.INK); c.dot(x + 1, B - 40, PAL.INK); c.dot(x, B - 39, PAL.RED)
    return c.outline()


def _hang(roots):
    """Hangs from the cell's top row: vines (meadow/ruins) or roots."""
    c = Px(64, 64)
    stem, hi, sh = ((PAL.BROWN_DK, PAL.BROWN, PAL.BROWN_XD) if roots
                    else (PAL.GRASS_MID, PAL.GRASS, PAL.TEAL))
    for x0, length, ph in ((14, 30, 0.0), (24, 44, 1.3), (37, 26, 2.1), (48, 38, 0.7)):
        pts = [(x0 + int(2 * math.sin(y / 8.0 + ph)), y) for y in range(0, length, 3)]
        c.part(_ln(pts, 2), stem, hi, sh)
        if not roots:
            for y in range(6, length, 9):
                x = x0 + int(2 * math.sin(y / 8.0 + ph))
                c.part(_el(x - 3, y - 1, x + 1, y + 2), PAL.GRASS_MID, PAL.GRASS, PAL.TEAL)
    return c.outline()


def _side(kind, right):
    """A small plant or stone in one quarter of the cell (x 2..22 or 41..61)."""
    c = Px(64, 64)
    x0 = 44 if right else 4
    if kind == "FERN":
        for dx, h, lean in ((2, 10, -3), (7, 14, 0), (12, 11, 3)):
            c.part(_ln([(x0 + dx, B), (x0 + dx + lean, B - h)], 2), PAL.GRASS_MID, PAL.GRASS, PAL.TEAL)
    elif kind == "GRASS":
        for i in range(5):
            c.part(_ln([(x0 + i * 3, B), (x0 + i * 3 + (i % 3) - 1, B - 6 - (i * 5) % 7)], 2),
                   PAL.GRASS_MID if i % 2 else PAL.GRASS, PAL.GRASS, PAL.TEAL)
    elif kind == "ROCKS":
        c.part(_el(x0, B - 7, x0 + 12, B + 1), PAL.SLATE, PAL.SLATE_LT, PAL.SLATE_XD)
        c.part(_el(x0 + 9, B - 4, x0 + 16, B + 1), PAL.SLATE_XD, PAL.SLATE, PAL.NAVY_XD, sep=True)
    else:                                  # SHROOM
        for dx, h, r in ((3, 7, 3), (10, 10, 4)):
            x = x0 + dx
            c.part(_rc(x, B - h, x + 1, B), PAL.CREAM, None, PAL.TAN)
            c.part(_el(x - r, B - h - r + 1, x + r + 1, B - h + 2), PAL.RED, PAL.PINK, PAL.RED_DK)
            c.dot(x, B - h - 1, PAL.CREAM)
    return c.outline()


# ---------------------------------------------------------------- Batch I
# Items from the old tileset (assets/platformer.png, 9 x 5 cells of 64 px).
# Several of its cells are little atlases of 32 px items; each item is cut
# out by its own pixels, snapped to the palette, and stood on the cell floor.
OLD = os.path.join(ROOT, "assets", "platformer.png")
_old_img = None


def _old():
    global _old_img
    if _old_img is None:
        from PIL import Image
        _old_img = Image.open(OLD).convert("RGBA")
    return _old_img


def _snap(rgb):
    """Nearest SAFE palette colour (the old art is close to it, not always on
    it) - the same set the palette check accepts."""
    from palette import SAFE
    return min(SAFE, key=lambda v: sum((a - b) ** 2 for a, b in zip(rgb, v)))


def _cut(box, floor=B + 1, bottom_pad=0):
    """Copy old-tileset pixels in `box` (x0, y0, x1, y1), trimmed to their own
    bounds, centred and standing on the cell floor (or ending on its last row)."""
    c = Px(64, 64)
    src = _old().crop(box)
    bb = src.getbbox()
    if not bb:
        return c
    src = src.crop(bb)
    ox = (64 - src.width) // 2
    oy = floor - src.height - bottom_pad
    px, dst = src.load(), c.img.load()
    for y in range(src.height):
        for x in range(src.width):
            r, g, b, a = px[x, y]
            if a >= 128 and 0 <= oy + y < 64:
                dst[ox + x, oy + y] = _snap((r, g, b)) + (255,)
    return c


def _cell(i):
    """Pixel box of old-tileset cell i (9 columns of 64)."""
    return ((i % 9) * 64, (i // 9) * 64, (i % 9) * 64 + 64, (i // 9) * 64 + 64)


def _quad(i, qx, qy):
    x0, y0, _, _ = _cell(i)
    return (x0 + qx * 32, y0 + qy * 32, x0 + qx * 32 + 32, y0 + qy * 32 + 32)


def _tree(top_cell, part):
    """A two-cell pine: the crown is old cell `top_cell`, the trunk the cell
    below it.  Cut as one 64 x 128 image, then split at the cell line, so the
    two halves meet exactly when placed one above the other."""
    c = Px(64, 64)
    x0, y0, x1, y1 = _cell(top_cell)
    full = _old().crop((x0, y0, x1, y1 + 64))
    half = full.crop((0, 0, 64, 64)) if part == "top" else full.crop((0, 64, 64, 128))
    px, dst = half.load(), c.img.load()
    for y in range(64):
        for x in range(64):
            r, g, b, a = px[x, y]
            if a >= 128:
                dst[x, y] = _snap((r, g, b)) + (255,)
    return c


def _log():
    c = Px(64, 64)
    c.part(_rc(10, B - 11, 52, B), PAL.BROWN, PAL.TAN, PAL.BROWN_DK)
    for x in (18, 30, 42):
        c.part(_ln([(x, B - 9), (x + 3, B - 3)], 1), PAL.BROWN_DK)          # bark
    c.part(_el(46, B - 12, 56, B + 1), PAL.TAN, PAL.TAN_LT, PAL.BROWN, sep=True)
    c.part(_el(49, B - 8, 53, B - 3), PAL.BROWN)                            # rings
    c.part(_ln([(14, B - 11), (20, B - 11)], 2), PAL.GRASS_MID, PAL.GRASS, PAL.TEAL)   # moss
    c.part(_ln([(24, B - 11), (24, B - 17)], 1), PAL.GRASS)                 # a sprout
    return c.outline()


def _bush():
    c = Px(64, 64)
    for (x0, y0, x1, y1) in ((12, B - 18, 34, B + 1), (26, B - 24, 50, B + 1), (36, B - 15, 54, B + 1)):
        c.part(_el(x0, y0, x1, y1), PAL.GRASS_MID, PAL.GRASS, PAL.TEAL, sep=True)
    for (x, y) in ((20, B - 10), (31, B - 17), (40, B - 12), (46, B - 7), (27, B - 5), (36, B - 20)):
        c.part(_el(x - 1, y - 1, x + 1, y + 1), PAL.RED, PAL.PINK)          # berries
    return c.outline()


def _menhir():
    c = Px(64, 64)
    c.part(_pg([(22, B), (24, B - 34), (31, B - 40), (39, B - 35), (42, B)]),
           PAL.SLATE, PAL.SLATE_LT, PAL.SLATE_XD)
    c.part(_ln([(28, B - 30), (32, B - 22), (29, B - 14)], 1), PAL.SLATE_XD)   # crack
    c.part(_pg([(22, B), (23, B - 12), (28, B - 8), (30, B)]), PAL.GRASS_MID, PAL.GRASS, PAL.TEAL)  # moss
    c.part(_ln([(36, B - 30), (40, B - 26)], 1), PAL.GRASS)
    return c.outline()


def _wheel():
    c = Px(64, 64)
    cx, cy, r = 32, B - 13, 13
    c.part(_el(cx - r, cy - r, cx + r, cy + r), PAL.BROWN, PAL.TAN, PAL.BROWN_DK)
    c.part(_el(cx - r + 3, cy - r + 3, cx + r - 3, cy + r - 3), PAL.INK)       # inside of the rim
    for a in (0, 60, 120, 240, 300):                                           # one spoke missing
        x = cx + int(round((r - 3) * math.cos(math.radians(a))))
        y = cy + int(round((r - 3) * math.sin(math.radians(a))))
        c.part(_ln([(cx, cy), (x, y)], 2), PAL.BROWN, PAL.TAN, PAL.BROWN_DK)
    c.part(_el(cx - 2, cy - 2, cx + 2, cy + 2), PAL.SLATE, PAL.SLATE_LT)      # hub
    c.part(_rc(cx - r - 2, B - 2, cx + r + 2, B), PAL.GRASS_MID, PAL.GRASS)    # half sunk in grass
    return c.outline()


DRAW = {"FERN": _fern, "FLOWERS": _flowers, "MUSHROOMS": _mushrooms,
        "STUMP": _stump, "GRASS_TALL": _grass_tall, "ROCKS": _rocks,
        "CRATE": _crate1, "CRATES": _crates, "BARREL": _barrel,
        "LANTERN": _lantern, "SIGNPOST": _signpost, "PILLAR": _pillar,
        "BONES": _bones, "CRYSTALS": _crystals, "BANNER": _banner,
        "SKULL_POST": _skull_post, "HANG_VINES": lambda: _hang(False),
        "HANG_ROOTS": lambda: _hang(True)}
for _k in ("FERN", "GRASS", "ROCKS", "SHROOM"):
    DRAW[_k + "_L"] = (lambda k=_k: _side(k, False))
    DRAW[_k + "_R"] = (lambda k=_k: _side(k, True))
DRAW.update({
    "BOULDER":      lambda: _cut(_quad(22, 0, 0)),
    "STONE":        lambda: _cut(_quad(22, 1, 0)),
    "ORE_ROCK":     lambda: _cut(_quad(22, 0, 1)),
    "STAFF":        lambda: _cut(_quad(22, 1, 1)),
    # the crystal sits in the right half of cell 21; its left third is a
    # terrain blob spilling over from cell 20, so cut tight around it
    "CRYSTAL_ROCK": lambda: _cut((_cell(21)[0] + 32, _cell(21)[1] + 32, _cell(21)[0] + 58, _cell(21)[1] + 64)),
    "PINE_TOP":     lambda: _tree(31, "top"),   "PINE_BOT":   lambda: _tree(31, "bot"),
    "AUTUMN_TOP":   lambda: _tree(32, "top"),   "AUTUMN_BOT": lambda: _tree(32, "bot"),
    "LOG": _log, "BUSH": _bush, "MENHIR": _menhir, "WHEEL": _wheel,
})


def main() -> int:
    cells = [DRAW[n]() for n in NAMES]
    rows = [cells[i:i + COLS] for i in range(0, len(cells), COLS)]
    os.makedirs(os.path.dirname(OUT_SVG), exist_ok=True)
    _sheet(rows, 64, 64, OUT_PNG, OUT_SVG)
    print("props      : %s  %d props in %d x %d cells of 64 px"
          % (os.path.relpath(OUT_PNG, ROOT), len(NAMES), COLS, len(rows)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
