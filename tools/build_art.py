#!/usr/bin/env python3
"""
Generate DungeonRush2's sprite art in the platformer.png house style.

Every shape is described once against tools/artdraw.py and emitted as BOTH
a PNG (what the game loads) and an SVG (what a human edits).  See artdraw.py
for why both come out of the same primitives.

Style rules, taken from measuring platformer.png rather than guessed:

  * 34 colours, all of them sampled from that sheet (see palette.py).
  * Flat fills.  No gradients, no airbrush -- shading is a second hard-edged
    shape, not a ramp.
  * A heavy #1c121c rim.  It is 23.5% of the source sheet's pixels; it is the
    single most load-bearing part of the look and omitting it is why art
    drawn for this game previously floated off the terrain.
  * Chunky and slightly angular.  Details smaller than ~3px vanish at the
    sizes these sprites are drawn at (a 64px powerup renders at 28px), so
    anything that cannot be read at final size is left out.

Sheet geometry MUST match src/SpriteSpec.h.  If a size changes here, change
it there too.
"""
import math
import os
import sys

from artdraw import Canvas, Sheet
from palette import PAL

HERE = os.path.dirname(os.path.abspath(__file__))
SPRITES = os.path.abspath(os.path.join(HERE, "..", "assets", "sprites"))
SVG = os.path.abspath(os.path.join(HERE, "..", "assets", "svg"))

INK = PAL.INK


# ===========================================================================
# shared helpers
# ===========================================================================
def badge(c, tint, rim=6):
    """The rounded-hex backing every 64px powerup sits on.

    Giving all eight powerups one silhouette is what makes them read as a
    family; previously each was a differently-shaped blob.

    The dark inner recess is not decoration.  Without it the glyph sits
    directly on the tint, and two of the pairings are near-invisible:
    PAL.RED on PAL.TEAL differ by two luminance points (85 vs 83), so the
    health heart vanished into its own badge.  A recess of one consistent
    dark value guarantees every glyph has somewhere to read against.
    """
    s = c.w
    hexpts = lambda k: [
        (s * .5, rim * .5 * k + s * (1 - k) / 2 * .5),
        (s - rim * .5 * k - s * (1 - k) / 2 * .5, s * .25 * k + s * (1 - k) / 2 * .25),
        (s - rim * .5 * k - s * (1 - k) / 2 * .5, s * .75 * k + s * (1 - k) / 2 * .75),
        (s * .5, s - rim * .5 * k - s * (1 - k) / 2 * .5),
        (rim * .5 * k + s * (1 - k) / 2 * .5, s * .75 * k + s * (1 - k) / 2 * .75),
        (rim * .5 * k + s * (1 - k) / 2 * .5, s * .25 * k + s * (1 - k) / 2 * .25),
    ]
    c.poly(hexpts(1.0), tint, outline=INK, lw=3)
    # One recess colour for all eight badges.  Deriving it per tint looked
    # cleverer but every `shade(tint, .66)` snapped back to INK anyway -- the
    # palette has no dark tinted browns -- so the per-badge variety was an
    # illusion.  A single uniform recess is honest about that and is what
    # actually makes the eight read as one family.
    c.poly(hexpts(0.74), PAL.NAVY_XD)
    # Inner bevel: a lighter facet along the top-left, hard-edged.  Snapped
    # to a real sheet colour (see palette.snap for why).
    c.poly([(s * .5, rim * .5 + 3), (s - rim * .5 - 3, s * .25 + 4),
            (s - rim * .5 - 3, s * .55), (s * .5, s * .62),
            (rim * .5 + 3, s * .55), (rim * .5 + 3, s * .25 + 4)],
           PAL.light(tint, .22))


def chunky_heart(c, cx, cy, w, h, fill, shade=None, hi=None):
    """A flat, angular heart -- not the bezier valentine.

    Drawn as two blocks plus a wedge so the edges stay hard at 28px.
    """
    shade = shade or PAL.shade(fill, .34)
    hi = hi or PAL.light(fill, .30)
    x, y = cx - w / 2, cy - h / 2
    c.poly([(x, y + h * .28), (x + w * .18, y), (x + w * .5, y + h * .12),
            (x + w * .82, y), (x + w, y + h * .28),
            (x + w * .5, y + h)], fill, outline=INK, lw=2)
    # right side in shade
    c.poly([(x + w * .5, y + h * .12), (x + w * .82, y), (x + w, y + h * .28),
            (x + w * .5, y + h)], shade)
    # highlight facet
    c.poly([(x + w * .06, y + h * .22), (x + w * .24, y + h * .04),
            (x + w * .40, y + h * .14), (x + w * .22, y + h * .34)], hi)


def bolt(c, cx, cy, w, h, fill, shade=None):
    """Lightning bolt: a zig-zag polygon, hard corners."""
    shade = shade or PAL.shade(fill, .30)
    x, y = cx - w / 2, cy - h / 2
    c.poly([(x + w * .62, y), (x + w * .18, y + h * .52),
            (x + w * .46, y + h * .52), (x + w * .30, y + h),
            (x + w * .86, y + h * .42), (x + w * .54, y + h * .42),
            (x + w * .82, y)], fill, outline=INK, lw=2)
    c.poly([(x + w * .30, y + h), (x + w * .86, y + h * .42),
            (x + w * .54, y + h * .42)], shade)


def shield_glyph(c, cx, cy, w, h, fill, shade=None, hi=None):
    """Heater shield with a pale cross."""
    shade = shade or PAL.shade(fill, .32)
    hi = hi or PAL.light(fill, .34)
    x, y = cx - w / 2, cy - h / 2
    c.poly([(x, y), (x + w, y), (x + w, y + h * .58),
            (x + w * .5, y + h), (x, y + h * .58)],
           fill, outline=INK, lw=2)
    c.poly([(x + w * .5, y), (x + w, y), (x + w, y + h * .58),
            (x + w * .5, y + h)], shade)
    bw = w * .16
    c.rect(cx - bw / 2, y + h * .16, bw, h * .62, hi)
    c.rect(x + w * .14, cy - bw / 2 + h * .04, w * .72, bw, hi)


# ===========================================================================
# pickups -- 64x64, drawn at 0.44 scale (28px on screen), so BOLD and SIMPLE
# ===========================================================================
def power_health(path, svg):
    c = Canvas(64, 64, "power_health")
    badge(c, PAL.TEAL)
    chunky_heart(c, 32, 33, 40, 38, PAL.RED)
    c.save(path, svg, scale=3)


def power_speed(path, svg):
    c = Canvas(64, 64, "power_speed")
    badge(c, PAL.WATER_DK)
    bolt(c, 32, 33, 34, 44, PAL.YELLOW)
    c.save(path, svg, scale=3)


def power_invincible(path, svg):
    c = Canvas(64, 64, "power_invincible")
    badge(c, PAL.SLATE)
    shield_glyph(c, 32, 33, 38, 44, PAL.GOLD)
    c.save(path, svg, scale=3)


def power_magnet(path, svg):
    """Horseshoe magnet: two poles, red and pale."""
    c = Canvas(64, 64, "power_magnet")
    badge(c, PAL.BROWN_XD)
    # body as a thick U, drawn as nested polygons so the hole shows through
    outer = [(18, 20), (46, 20), (46, 40), (38, 40), (38, 28),
             (26, 28), (26, 40), (18, 40)]
    c.poly(outer, PAL.PLATE, outline=INK, lw=3)
    c.poly([(32, 20), (46, 20), (46, 40), (38, 40), (38, 28), (32, 28)],
           PAL.SLATE_LT)
    c.rect(18, 38, 10, 10, PAL.RED, outline=INK, lw=2)
    c.rect(36, 38, 10, 10, PAL.RED, outline=INK, lw=2)
    c.save(path, svg, scale=3)


def power_shield(path, svg):
    c = Canvas(64, 64, "power_shield")
    badge(c, PAL.NAVY)
    shield_glyph(c, 32, 33, 38, 44, PAL.PLATE)
    c.save(path, svg, scale=3)


def power_coins(path, svg):
    """A stack of three coins."""
    c = Canvas(64, 64, "power_coins")
    badge(c, PAL.BROWN)
    for i, (cy, rx, ry) in enumerate(((44, 15, 7), (36, 15, 7), (28, 15, 7))):
        c.ellipse(32, cy + 3, rx, ry, PAL.shade(PAL.GOLD, .40))
        c.ellipse(32, cy, rx, ry, PAL.GOLD, outline=INK, lw=2)
        c.ellipse(32, cy - 1.5, rx * .55, ry * .5, PAL.YELLOW)
    c.save(path, svg, scale=3)


# --- 32x32 pickups (no room for a badge; glyph only) ----------------------
def power_jump(path, svg):
    c = Canvas(32, 32, "power_jump")
    # Double chevron pointing up, with a base bar.
    c.poly([(16, 3), (28, 15), (22, 15), (22, 21), (10, 21), (10, 15), (4, 15)],
           PAL.GRASS, outline=INK, lw=2)
    c.poly([(16, 3), (28, 15), (22, 15), (16, 9)], PAL.light(PAL.GRASS, .30))
    c.rect(7, 24, 18, 5, PAL.TAN, outline=INK, lw=2)
    c.save(path, svg, scale=4)


def power_damage(path, svg):
    """A blade:angular, drawn tip-up."""
    c = Canvas(32, 32, "power_damage")
    c.poly([(16, 2), (21, 9), (21, 22), (16, 27), (11, 22), (11, 9)],
           PAL.PLATE, outline=INK, lw=2)
    c.poly([(16, 2), (21, 9), (21, 22), (16, 27)], PAL.SLATE_LT)
    c.rect(6, 24, 20, 4, PAL.BROWN, outline=INK, lw=2)
    c.rect(14, 20, 4, 5, PAL.BROWN_XD)
    c.save(path, svg, scale=4)


def heart_icon(path, svg):
    c = Canvas(32, 32, "heart")
    chunky_heart(c, 16, 16, 26, 24, PAL.RED)
    c.save(path, svg, scale=4)


# ===========================================================================
# projectiles
# ===========================================================================
def projectile(path, svg):
    """The player's bolt: a pale core in a water-blue shell, with a tail."""
    c = Canvas(32, 32, "projectile")
    c.poly([(4, 16), (13, 9), (13, 16)], PAL.WATER_DK)
    c.poly([(4, 16), (13, 23), (13, 16)], PAL.WATER_DK)
    c.ellipse(19, 16, 11, 7, PAL.WATER, outline=INK, lw=2)
    c.ellipse(17, 14, 6, 3.2, PAL.CREAM)
    c.save(path, svg, scale=4)


def fireball(path, svg):
    """Enemy fireball: an ember orb with a hot core."""
    c = Canvas(32, 32, "fireball")
    c.circle(16, 16, 14, PAL.EMBER, outline=INK, lw=2)
    c.circle(16, 16, 10, PAL.ORANGE_DK)
    c.circle(16, 16, 6, PAL.ORANGE)
    c.circle(15, 14, 3, PAL.YELLOW)
    c.save(path, svg, scale=4)


def shockwave(path, svg):
    """4 cells: an expanding ring that thins and fades as it grows."""
    s = Sheet(4, 1, 32, 32)
    for i in range(4):
        c = s.sub(i, 0, "shockwave %d" % i)
        t = i / 3.0
        r = 6 + t * 9
        # Explicit ramp steps rather than a mix().  `mix` was producing
        # #bdc1ca -- a grey that is 40 units off anything on the sheet and
        # that made the shockwave look like it belonged to a different game.
        col = (PAL.CREAM, PAL.PLATE, PAL.SLATE_LT, PAL.SLATE)[i]
        c.circle(16, 16, r, None, outline=col, lw=3)
        c.circle(16, 16, r * .62, None,
                 outline=(PAL.WATER, PAL.WATER_DK, PAL.NAVY_DK, PAL.NAVY)[i], lw=2)
    s.save(path, svg, scale=3)


# ===========================================================================
# enemies
# ===========================================================================
def archer(path, svg):
    """3 rows x 4 cols @ 48x48: idle, shoot, die.

    A hooded archer in the terrain palette: teal hood, brown cloak, tan bow.
    The old art used a purple hood and blue cloth that matched nothing.
    """
    s = Sheet(4, 3, 48, 48)

    def figure(c, bow_draw, lean=0.0, topple=0.0):
        # legs
        c.rect(18, 34, 5, 11, PAL.BROWN_XD, outline=INK, lw=2)
        c.rect(25, 34, 5, 11, PAL.BROWN_XD, outline=INK, lw=2)
        # cloak body
        c.poly([(16 + lean, 20), (32 + lean, 20), (34 + lean, 36),
                (14 + lean, 36)], PAL.BROWN, outline=INK, lw=2)
        c.poly([(24 + lean, 20), (32 + lean, 20), (34 + lean, 36),
                (24 + lean, 36)], PAL.BROWN_DK)
        # hood
        c.poly([(16 + lean, 20), (24 + lean, 6), (32 + lean, 20)],
               PAL.TEAL, outline=INK, lw=2)
        c.poly([(24 + lean, 6), (32 + lean, 20), (26 + lean, 20)], PAL.TEAL_DK)
        # face slit
        c.rect(19 + lean, 14, 10, 4, PAL.INK)
        # eye
        c.rect(21 + lean, 15, 3, 2, PAL.YELLOW)
        # Bow: an arc of segments.  `bow_draw` pulls the belly back and the
        # string with it, so the four shoot cells read as a real draw.
        bx = 36 - bow_draw * 4
        c.line([(bx, 8), (bx + 5 - bow_draw, 20), (bx, 34)], PAL.TAN, lw=3)
        c.line([(bx, 8), (bx - 5 + bow_draw * 6, 21), (bx, 34)], PAL.CREAM, lw=1)
        if bow_draw > 0:
            # nocked arrow
            c.rect(bx - 8, 20, 14, 2, PAL.TAN_LT, outline=INK, lw=1)

    for i in range(4):
        # Idle breathes by shifting the legs a pixel per cell; a static
        # four-cell idle reads as a frozen statue.
        c = s.sub(i, 0, "idle %d" % i)
        figure(c, bow_draw=0.0)
        c.rect(17, 44, 6, 1 + (i % 2), PAL.BROWN_XD)
    for i in range(4):
        c = s.sub(i, 1, "shoot %d" % i)
        figure(c, bow_draw=i / 3.0)
    for i in range(4):
        c = s.sub(i, 2, "die %d" % i)
        # The first version drew a vertical sliver that shrank to nothing,
        # which at 1.15 scale is two visible pixels -- the archer looked like
        # it was deleted rather than killed.  A prone figure reads at any
        # size: fall first, then settle.
        t = min(1.0, i / 2.0)          # fully prone by cell 2
        lift = (1.0 - t) * 14          # how far above the ground it still is
        y = 34 - lift
        # torso lying along the ground
        c.poly([(10, y + 6), (30, y + 2), (34, y + 9), (14, y + 13)],
               PAL.BROWN, outline=INK, lw=2)
        c.poly([(22, y + 4), (30, y + 2), (34, y + 9), (24, y + 11)],
               PAL.BROWN_DK)
        # hooded head, tipped to the left
        c.poly([(8, y + 2), (16, y), (20, y + 8), (12, y + 11)],
               PAL.TEAL, outline=INK, lw=2)
        c.rect(11, y + 4, 6, 3, PAL.INK)
        # legs trailing
        c.rect(30, y + 8, 10, 5, PAL.BROWN_XD, outline=INK, lw=2)
        # discarded bow, tumbling away as the frame advances
        bx, by = 34 + t * 8, 40 - t * 10
        c.line([(bx, by - 8), (bx + 4, by), (bx, by + 8)], PAL.TAN, lw=3)
    s.save(path, svg, scale=3)


def bomber(path, svg):
    """3 rows x 4 cols @ 32x32: walk, fuse, explode."""
    s = Sheet(4, 3, 32, 32)

    def body(c, squash=0, fuse_len=6, spark=0):
        w, h = 18 + squash, 16 - squash
        c.ellipse(16, 22 - squash, w / 2, h / 2, PAL.BROWN_XD, outline=INK, lw=2)
        c.ellipse(14, 20 - squash, w / 3, h / 3, PAL.SLATE)
        # fuse
        c.line([(16, 14 - squash), (19, 10 - squash), (17, 6 - squash)],
               PAL.TAN, lw=2)
        if spark:
            c.circle(17, 5 - squash, 2 + spark, PAL.YELLOW, outline=INK, lw=1)
            c.circle(17, 5 - squash, 1 + spark * .5, PAL.CREAM)

    for i in range(4):
        c = s.sub(i, 0, "walk %d" % i)
        body(c, squash=(0, 2, 0, 2)[i], fuse_len=6, spark=(i % 2))
    for i in range(4):
        c = s.sub(i, 1, "fuse %d" % i)
        body(c, squash=i, fuse_len=6, spark=1 + i * .8)
    for i in range(4):
        c = s.sub(i, 2, "explode %d" % i)
        t = i / 3.0
        r = 5 + t * 11
        c.circle(16, 16, r, PAL.EMBER, outline=INK, lw=2)
        c.circle(16, 16, r * .72, PAL.ORANGE)
        c.circle(16, 16, r * .44, PAL.YELLOW)
        if t < .8:
            c.circle(16, 16, r * .22, PAL.CREAM)
    s.save(path, svg, scale=4)



# ===========================================================================
# ART-11 -- 1:1 pixel sprites (ART_STYLE_V2).  No supersampling, no resize:
# every pixel is placed, so the PNG holds only PAL colours and alpha 0/255.
# The SVG twin is the same pixels as run-length <rect>s - exact and editable.
# ===========================================================================
def save_pixels(img, png_path, svg_path):
    img.save(png_path)
    W, H = img.size
    px = img.load()
    out = ['<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" '
           'viewBox="0 0 %d %d" shape-rendering="crispEdges">' % (W, H, W, H)]
    for y in range(H):
        x = 0
        while x < W:
            c = px[x, y]
            if c[3] == 0:
                x += 1
                continue
            x0 = x
            while x < W and px[x, y] == c:
                x += 1
            out.append('  <rect x="%d" y="%d" width="%d" height="1" fill="%s"/>'
                       % (x0, y, x - x0, PAL.hex(c[:3])))
    out.append("</svg>")
    with open(svg_path, "w") as f:
        f.write("\n".join(out) + "\n")


def _blob_cell(w, h, mouth=False, eyes=True, fade=0):
    """One slime pose in a 32x32 cell, feet on the last row, facing right.

    A superellipse dome with a flat base: wide at the floor, rounded on top.
    1 px INK outline, light from the upper left (GRASS lit, GRASS_MID body,
    TEAL_DK shade), a CREAM glint, two INK eyes towards the facing side.
    `mouth` is the hop tell: a RED_DK/RED gash of >= 4 px (ART_STYLE_V2 3).
    `fade` 1..3 walks the colours down the ramp for the death frames.
    """
    CW = CH = 32
    cell = [[None] * CW for _ in range(CH)]
    a, cx, base = w / 2.0, CW / 2.0, CH - 1

    def inside(x, y):
        if y > base or y < base - h + 1:
            return False
        nx = abs((x + 0.5) - cx) / a
        ny = (base - y + 0.5) / h
        return nx ** 2.4 + ny ** 2.4 <= 1.0

    ramp = [PAL.GRASS, PAL.GRASS_MID, PAL.TEAL, PAL.TEAL_DK, PAL.NAVY]
    lit, body, shade = ramp[min(fade, 2)], ramp[min(fade + 1, 3)], ramp[min(fade + 3, 4)]
    for y in range(CH):
        for x in range(CW):
            if not inside(x, y):
                continue
            edge = (y == base or not inside(x - 1, y) or not inside(x + 1, y)
                    or not inside(x, y - 1) or not inside(x, y + 1))
            if edge:
                cell[y][x] = PAL.INK
                continue
            # distance from a light spot up and to the left: a round lit cap,
            # a body ring, and shade on the lower right and along the base
            lx = ((x + 0.5) - (cx - a * 0.30)) / a
            ly = ((y + 0.5) - (base - h * 0.78)) / h
            d = (lx * lx + ly * ly) ** 0.5
            cell[y][x] = (lit if d < 0.52 else
                          body if d < 0.92 and y < base - 2 else shade)
    top = base - h + 1
    if fade == 0:
        gx, gy = int(cx - a * 0.45), top + max(2, h // 5)
        for dx, dy in ((0, 0), (1, 0), (0, 1)):
            if cell[gy + dy][gx + dx] not in (None, PAL.INK):
                cell[gy + dy][gx + dx] = PAL.CREAM
    if eyes:
        ey = top + max(3, int(h * 0.38))
        for ex in (int(cx + a * 0.18), int(cx + a * 0.52)):
            for dy in (0, 1):
                if cell[ey + dy][ex] not in (None,):
                    cell[ey + dy][ex] = PAL.INK
        if mouth:
            my = ey + 3
            for mx in range(int(cx + a * 0.10), int(cx + a * 0.10) + 5):
                if cell[my][mx] not in (None, PAL.INK):
                    cell[my][mx] = PAL.RED_DK
            for mx in range(int(cx + a * 0.10) + 1, int(cx + a * 0.10) + 3):
                if cell[my + 1][mx] not in (None, PAL.INK):
                    cell[my + 1][mx] = PAL.RED
    return cell


def slime(path, svg):
    """enemy_slime.png: 4 cols x 3 rows of 32x32 -- idle 4, hop 4, die 4.

    Figure 26x20 on a 26x20 collider (ART_STYLE_V2 2.5).  Hop = tell, tell,
    stretch, land: the flattened tell with the open mouth is held for two
    frames, twice the active frame (section 3).  Death sinks into a puddle.
    """
    from PIL import Image
    poses = [
        [(26, 20, False), (27, 19, False), (26, 20, False), (25, 21, False)],   # idle
        [(28, 18, True),  (28, 18, True),  (22, 24, False), (28, 18, False)],   # hop
    ]
    die = [(28, 14, 1), (30, 9, 2), (30, 5, 3), (24, 3, 3)]
    img = Image.new("RGBA", (4 * 32, 3 * 32), (0, 0, 0, 0))
    px = img.load()

    def put(cell, col, row):
        for y in range(32):
            for x in range(32):
                c = cell[y][x]
                if c is not None:
                    px[col * 32 + x, row * 32 + y] = (c[0], c[1], c[2], 255)

    for row, line in enumerate(poses):
        for col, (w, h, m) in enumerate(line):
            put(_blob_cell(w, h, mouth=m), col, row)
    for col, (w, h, f) in enumerate(die):
        put(_blob_cell(w, h, eyes=(f < 3), fade=f), col, 2)
    save_pixels(img, path, svg)


# ---------------------------------------------------------------------------
# ART-11 roster toolkit: 1:1 pixel parts, outline grown outside the figure
# ---------------------------------------------------------------------------
class Px:
    """One sprite cell drawn as flat parts.  Light from the upper left."""

    def __init__(self, w, h):
        from PIL import Image
        self.w, self.h = w, h
        self.img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
        self.px = self.img.load()
        self.owner = [[0] * w for _ in range(h)]
        self.n = 0

    def _mask(self, fn):
        from PIL import Image, ImageDraw
        m = Image.new("1", (self.w, self.h), 0)
        fn(ImageDraw.Draw(m))
        return m.load()

    def part(self, fn, base, hi=None, sh=None, sep=False):
        """fn(draw) paints the part's shape in 1s on a 1-bit mask."""
        m = self._mask(fn)
        self.n += 1
        W, H = self.w, self.h

        def ins(x, y):
            return 0 <= x < W and 0 <= y < H and m[x, y]
        for y in range(H):
            for x in range(W):
                if not m[x, y]:
                    continue
                c = base
                if sh is not None and not ins(x + 1, y + 1):
                    c = sh
                elif hi is not None and not ins(x - 1, y - 1):
                    c = hi
                if sep and any(0 <= x + dx < W and 0 <= y + dy < H and not m[x + dx, y + dy]
                               and self.px[x + dx, y + dy][3] for dx, dy in
                               ((1, 0), (-1, 0), (0, 1), (0, -1))):
                    c = PAL.INK
                self.px[x, y] = c + (255,)
                self.owner[y][x] = self.n

    def dot(self, x, y, c):
        if 0 <= x < self.w and 0 <= y < self.h:
            self.px[x, y] = c + (255,)

    def clear(self, fn):
        m = self._mask(fn)
        for y in range(self.h):
            for x in range(self.w):
                if m[x, y]:
                    self.px[x, y] = (0, 0, 0, 0)

    def outline(self):
        W, H, p = self.w, self.h, self.px
        add = []
        for y in range(H):
            for x in range(W):
                if p[x, y][3]:
                    continue
                if any(0 <= x + dx < W and 0 <= y + dy < H and p[x + dx, y + dy][3]
                       and p[x + dx, y + dy][:3] != PAL.INK
                       for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))):
                    add.append((x, y))
        for x, y in add:
            p[x, y] = PAL.INK + (255,)
        return self


def _sheet(cells, cw, ch, path, svg):
    """cells: list of rows, each a list of Px; writes PNG + SVG twin."""
    from PIL import Image
    cols = max(len(r) for r in cells)
    img = Image.new("RGBA", (cols * cw, len(cells) * ch), (0, 0, 0, 0))
    for r, row in enumerate(cells):
        for c, cell in enumerate(row):
            img.paste(cell.img, (c * cw, r * ch), cell.img)
    save_pixels(img, path, svg)


def _ln(pts, w):
    return lambda d: d.line(pts, fill=1, width=w)


def _el(x0, y0, x1, y1):
    return lambda d: d.ellipse([x0, y0, x1, y1], fill=1)


def _rc(x0, y0, x1, y1):
    return lambda d: d.rectangle([x0, y0, x1, y1], fill=1)


def _pg(pts):
    return lambda d: d.polygon(pts, fill=1)


# ----------------------------------------------------------------- skeleton
def _skel(pose="idle", t=0, bob=0, lean=0, tell=False, swing=0, fall=0):
    c = Px(40, 56)
    B, BS, BH = PAL.CREAM, PAL.TAN, PAL.CREAM
    by = 54                      # feet row (outline lands on 55)
    hx = 19 + lean               # hip x
    hy = 36 + bob + fall
    sy = 25 + bob + fall         # shoulder line
    if fall >= 12:               # collapsed pile
        c.part(_el(8, 47, 32, 54), B, BH, BS)
        c.clear(_rc(12, 49, 28, 50))
        c.part(_el(22, 42, 33, 52), B, BH, BS)
        c.dot(26, 46, PAL.INK); c.dot(29, 46, PAL.INK)
        c.part(_ln([(4, 53), (16, 49)], 2), PAL.SLATE, PAL.SLATE_LT, PAL.SLATE_XD)
        return c.outline()
    # legs: thigh + shin per leg, phase t
    for side, ph in ((-1, 0.0), (1, 3.1416)):
        a = math.sin(t + ph)
        kx, ky = hx + side * 2 + int(round(a * 3)), hy + 8
        fx, fy = hx + side * 3 + int(round(a * 6)), by - (2 if a > 0.6 else 0) + fall // 3
        c.part(_ln([(hx + side * 2, hy), (kx, ky)], 3), B, BH, BS)
        c.part(_ln([(kx, ky), (fx, min(fy, by))], 3), B, BH, BS)
        c.part(_rc(fx - 1, min(fy, by) - 1, fx + 3, min(fy, by)), B, BH, BS)
    # pelvis, spine, ribcage with gaps
    c.part(_rc(hx - 5, hy - 2, hx + 5, hy + 1), B, BH, BS)
    c.part(_ln([(hx, hy - 2), (hx + lean // 2, sy)], 2), BS)
    for i, ry in enumerate((sy, sy + 3, sy + 6)):
        w = 6 - i
        c.part(_rc(hx - w + lean // 2, ry, hx + w + lean // 2, ry + 1), B, BH, BS)
    # back arm (reaches forward as a counterbalance on the wind-up)
    back = (hx + 13, sy + 2) if tell else ((hx - 14, sy + 3) if swing else (hx - 12, sy + 8))
    c.part(_ln([(hx - 5, sy), back], 2), BS)
    # skull + jaw
    sx, skx = hx + 2 + lean, sy - 10
    c.part(_el(sx - 7, skx - 7, sx + 7, skx + 6), B, BH, BS)
    c.part(_rc(sx - 3, skx + 5, sx + 5, skx + 8), B, BH, BS)
    c.dot(sx + 1, skx + 7, PAL.INK); c.dot(sx + 3, skx + 7, PAL.INK)
    for ex in (sx, sx + 4):
        for dy in (0, 1):
            c.dot(ex, skx + dy, PAL.INK); c.dot(ex + 1, skx + dy, PAL.INK)
    c.dot(sx + 4, skx, PAL.RED)
    if tell:                                     # eye flare: >= 4 px of RED
        for dx, dy in ((4, 1), (5, 0), (5, 1), (6, 0), (3, -1)):
            c.dot(sx + dx, skx + dy, PAL.RED)
    # front arm + notched rusty blade
    shx = hx + 4 + lean // 2
    if tell:                 # sword raised behind the head
        hand = (shx - 3, sy - 12)
        tip = (hand[0] - 19, hand[1] + 2)
    elif swing:              # mid swing / follow-through
        hand = (shx + 5, sy + 2)
        tip = (hand[0] + 9, hand[1] + (2 if swing == 1 else 9))
    else:
        hand = (shx + 6, sy + 8)
        tip = (hand[0] + 8, hand[1] - 11)
    c.part(_ln([(shx, sy), hand], 2), B, BH, BS)
    c.part(_ln([hand, tip], 3), PAL.SLATE, PAL.SLATE_LT, PAL.SLATE_XD, sep=True)
    c.dot((hand[0] + tip[0]) // 2, (hand[1] + tip[1]) // 2, PAL.RUST)
    c.part(_rc(hand[0] - 1, hand[1] - 1, hand[0] + 1, hand[1] + 1), PAL.BROWN, None, PAL.BROWN_DK)
    return c.outline()


def skeleton(path, svg):
    """enemy_skeleton.png 6x5 of 40x56: idle 4, walk 6, attack 4, hurt 2, die 5."""
    rows = [
        [_skel(bob=b) for b in (0, 0, 1, 1)],
        [_skel(t=i * 1.047, bob=(i % 3 == 1)) for i in range(6)],
        [_skel(tell=True), _skel(tell=True), _skel(swing=1), _skel(swing=2)],
        [_skel(lean=-3, bob=1), _skel(lean=-2)],
        [_skel(fall=f) for f in (0, 3, 6, 9, 12)],
    ]
    _sheet(rows, 40, 56, path, svg)


# ---------------------------------------------------------------------- bat
def _bat(up=True, dead=0):
    c = Px(32, 32)
    body, mem, bone = PAL.BROWN_XD, PAL.SLATE, PAL.BROWN_XD
    if dead:
        # tumbling: wings folded, body sinking towards the bottom row
        y = 18 + dead * 3
        c.part(_el(10, y - 4, 22, y + 5), body, PAL.BROWN_DK, PAL.INK)
        c.part(_pg([(9, y - 2), (4, y + 6), (12, y + 4)]), mem, PAL.SLATE_LT, PAL.SLATE_XD)
        c.part(_pg([(23, y - 2), (28, y + 6), (20, y + 4)]), mem, PAL.SLATE_LT, PAL.SLATE_XD)
        if dead < 3:
            c.dot(14, y, PAL.INK); c.dot(18, y, PAL.INK)
        return c.outline()
    if up:
        lw = [(14, 19), (2, 5), (4, 12), (1, 16), (6, 15), (5, 21), (10, 18), (12, 23)]
    else:
        lw = [(14, 19), (1, 27), (6, 27), (4, 30), (10, 27), (13, 24)]
    rw = [(31 - x, y) for x, y in lw]
    c.part(_pg(lw), mem, PAL.SLATE_LT, PAL.SLATE_XD)
    c.part(_pg(rw), mem, PAL.SLATE_LT, PAL.SLATE_XD)
    c.part(_ln([lw[0], lw[1]], 1), bone); c.part(_ln([rw[0], rw[1]], 1), bone)
    c.part(_el(12, 14, 19, 27), body, PAL.BROWN_DK, PAL.INK)
    c.part(_pg([(12, 16), (12, 7), (15, 15)]), body)        # ears
    c.part(_pg([(19, 16), (19, 7), (16, 15)]), body)
    for ex in (14, 17):
        c.dot(ex, 18, PAL.RED); c.dot(ex, 19, PAL.RED)
    c.dot(15, 22, PAL.CREAM); c.dot(17, 22, PAL.CREAM)        # fangs
    c.part(_rc(13, 27, 14, 30), body, PAL.BROWN_DK); c.part(_rc(17, 27, 18, 30), body, PAL.BROWN_DK)   # feet
    return c.outline()


def bat(path, svg):
    """enemy_bat.png 3x2 of 32x32: fly 2 (wings up = the hold), die 3."""
    _sheet([[_bat(True), _bat(False)], [_bat(dead=d) for d in (1, 2, 3)]],
           32, 32, path, svg)


# --------------------------------------------------------------------- mage
def _mage(orb=3, sway=0, arm=0, die=0):
    c = Px(40, 56)
    robe, trim, staff, glow = PAL.NAVY, PAL.SLATE, PAL.BROWN, PAL.ORCHID
    base, cx = 54, 18
    if die >= 3:
        c.part(_pg([(4, base), (10, base - 9), (28, base - 8), (34, base)]), robe, PAL.SLATE_XD, PAL.NAVY_XD)
        c.part(_ln([(6, base - 1), (36, base - 4)], 2), staff, PAL.TAN, PAL.BROWN_DK)
        c.part(_el(34, base - 8, 38, base - 4), PAL.SLATE_LT if die == 4 else glow)
        return c.outline()
    h = 46 - die * 9
    top = base - h
    # robe: a long triangle flaring to the floor, no feet
    c.part(_pg([(cx - 13 + sway, base), (cx - 3, top + 12), (cx + 5, top + 12),
                (cx + 13 + sway, base)]), robe, PAL.SLATE_XD, PAL.NAVY_XD)
    c.part(_rc(cx - 13 + sway, base - 2, cx + 13 + sway, base), trim, PAL.SLATE_LT, PAL.SLATE_XD)
    # tall pointed hood, face in shadow, two ORCHID eyes
    c.part(_pg([(cx - 7, top + 15), (cx - 1, top), (cx + 8, top + 15)]), robe, PAL.SLATE, PAL.NAVY_XD)
    c.part(_el(cx - 3, top + 8, cx + 6, top + 15), PAL.INK)
    c.dot(cx + 2, top + 11, glow); c.dot(cx + 4, top + 11, glow)
    # staff taller than the body, orb on top
    sx = cx + 12
    oy = top + 1                              # orb seat; the orb grows round it
    c.part(_ln([(sx, oy), (sx - 1, base - 1)], 2), staff, PAL.TAN, PAL.BROWN_DK, sep=True)
    c.part(_ln([(cx + 3, top + 18), (sx, top + 20 - arm)], 3), robe, PAL.SLATE, PAL.NAVY_XD, sep=True)
    if die == 0:
        c.part(_el(sx - orb, oy - orb, sx + orb, oy + orb), glow, PAL.BLUSH, PAL.VIOLET)
    return c.outline()


def mage(path, svg):
    """enemy_mage.png 4x3 of 40x56: idle 4, cast 4 (orb grows 3 frames), die 4."""
    _sheet([[_mage(sway=s) for s in (0, 1, 0, -1)],
            [_mage(orb=4, arm=2), _mage(orb=5, arm=4), _mage(orb=6, arm=4), _mage(orb=3, arm=0)],
            [_mage(die=d) for d in (1, 2, 3, 4)]], 40, 56, path, svg)


# ------------------------------------------------------------------- archer
def _archer(draw=0, bob=0, loosed=False, die=0):
    c = Px(40, 56)
    cloak, bow, base = PAL.TEAL_DK, PAL.BROWN, 54
    if die >= 3:
        c.part(_el(4, base - 8, 26, base), cloak, PAL.TEAL, PAL.NAVY)
        c.part(_ln([(14, base - 2), (36, base - 6)], 2), bow, PAL.TAN, PAL.BROWN_DK)
        return c.outline()
    y0 = 4 + bob + die * 7
    hx = 15
    # legs with a clear gap, boots
    for lx in (hx - 4, hx + 3):
        c.part(_rc(lx, y0 + 32, lx + 2, base - 2), PAL.BROWN_XD, PAL.BROWN_DK, PAL.INK)
        c.part(_rc(lx - 1, base - 2, lx + 3, base), PAL.BROWN_DK, None, PAL.BROWN_XD)
    # cloak body + hood
    c.part(_pg([(hx - 8, y0 + 34), (hx - 5, y0 + 12), (hx + 6, y0 + 12), (hx + 8, y0 + 34)]),
           cloak, PAL.TEAL, PAL.NAVY)
    c.part(_el(hx - 6, y0 + 1, hx + 6, y0 + 14), cloak, PAL.TEAL, PAL.NAVY)
    c.part(_el(hx - 1, y0 + 6, hx + 5, y0 + 12), PAL.INK)
    c.dot(hx + 3, y0 + 8, PAL.CREAM)
    # bow: tall arc held well in front, a clear gap to the body
    bx = 31 - (0 if loosed else draw) * 2
    top, bot = y0 + 8, y0 + 40
    c.part(lambda d: d.arc([bx - 6, top, bx + 6, bot], -90, 90, fill=1, width=2),
           bow, PAL.TAN, PAL.BROWN_DK)
    sx = bx - draw * 4                      # string pulled back while drawing
    c.part(_ln([(bx, top), (sx, (top + bot) // 2), (bx, bot)], 1), PAL.CREAM)
    c.part(_ln([(hx + 5, y0 + 18), (bx + 4, (top + bot) // 2)], 2), cloak, PAL.TEAL, PAL.NAVY)
    if draw and not loosed:                  # nocked arrow, RED head = the tell
        ay = (top + bot) // 2
        c.part(_ln([(sx, ay), (bx + 7, ay)], 1), PAL.CREAM)
        for dx, dy in ((8, 0), (7, -1), (7, 1), (9, 0)):
            c.dot(bx + dx, ay + dy, PAL.RED)
    return c.outline()


def enemy_archer(path, svg):
    """enemy_archer.png 4x4 of 40x56: idle 4, aim 3, shoot 3, die 4."""
    _sheet([[_archer(bob=b) for b in (0, 0, 1, 1)],
            [_archer(draw=1), _archer(draw=2), _archer(draw=2)],
            [_archer(draw=2, loosed=True), _archer(draw=1, loosed=True), _archer()],
            [_archer(die=d) for d in (1, 2, 3, 4)]], 40, 56, path, svg)


# ------------------------------------------------------------------- bomber
def _bomber(step=0, spark=0, boom=0):
    c = Px(32, 32)
    if boom:
        r = int(4 + boom * 2.5)
        for col, k in ((PAL.EMBER, 1.0), (PAL.ORANGE, 0.72), (PAL.YELLOW, 0.45), (PAL.CREAM, 0.2)):
            if boom == 4 and col == PAL.CREAM:
                continue
            rr = max(1, int(r * k))
            c.part(_el(16 - rr, 29 - 2 * rr, 16 + rr, 29), col)
        if boom == 4:
            c.clear(_el(10, 29 - 2 * r + 6, 22, 24))
        return c.outline()
    by = 31
    # short legs on a clear gap, alternating
    for lx, lift in ((10, step == 1), (18, step == 3)):
        c.part(_rc(lx, by - 7 - lift, lx + 2, by - lift), PAL.BROWN_XD, None, PAL.INK)
    # round body with a TAN band, carried high on the legs
    c.part(_el(5, 7, 25, 24 - (step % 2)), PAL.BROWN, PAL.TAN, PAL.BROWN_DK)
    c.part(_rc(6, 14, 24, 16), PAL.TAN, PAL.TAN_LT, PAL.BROWN)
    c.dot(19, 11, PAL.INK); c.dot(21, 11, PAL.INK)
    # oversized fuse arcing forward, and its spark (doubles on the tell)
    c.part(_ln([(17, 8), (21, 5), (25, 5)], 2), PAL.SLATE, PAL.SLATE_LT, PAL.SLATE_XD)
    s = (1, 2, 3, 4)[spark]          # the tell: the spark doubles, 2 -> 4 -> 6 -> 8 px
    c.part(_el(26 - s, 5 - s, 26 + s, 5 + s), PAL.EMBER if spark else PAL.ORANGE)
    c.dot(26, 5, PAL.YELLOW)
    return c.outline()


def enemy_bomber(path, svg):
    """enemy_bomber.png 4x3 of 32x32: walk 4, fuse 3 (spark doubles), die 4."""
    _sheet([[_bomber(step=i, spark=0) for i in range(4)],
            [_bomber(spark=1), _bomber(spark=2), _bomber(spark=3)],
            [_bomber(boom=b) for b in (1, 2, 3, 4)]], 32, 32, path, svg)


# --------------------------------------------------------------------- boss
def _boss(t=0.0, bob=0, charge=0, arms=0, hurt=0, die=0):
    c = Px(96, 104)
    hide, plate = PAL.BROWN_XD, PAL.SLATE_XD
    base, cx = 103, 46 - hurt
    sink = die * 9
    if die >= 6:
        c.part(_el(6, base - 22, 90, base), hide, PAL.BROWN_DK, PAL.INK)
        c.part(_pg([(10, base - 18), (2, base - 34), (18, base - 20)]), PAL.TAN_LT, PAL.CREAM, PAL.TAN)
        c.part(_pg([(78, base - 20), (94, base - 30), (84, base - 14)]), PAL.TAN_LT, PAL.CREAM, PAL.TAN)
        c.part(_el(40, base - 16, 52, base - 6), PAL.BROWN_DK)
        return c.outline()
    top = 4 + bob + sink
    # legs
    for side in (-1, 1):
        a = math.sin(t + (0 if side < 0 else 3.14))
        lx = cx + side * 14 + int(a * 4)
        c.part(_rc(lx - 7, min(top + 66, base - 8), lx + 7, base - 2), hide, PAL.BROWN_DK, PAL.INK)
        c.part(_rc(lx - 9, base - 6, lx + 9, base), plate, PAL.SLATE, PAL.INK)
    # torso and chest plate
    c.part(_el(cx - 30, top + 22, cx + 30, top + 74), hide, PAL.BROWN_DK, PAL.INK)
    c.part(_pg([(cx - 18, top + 30), (cx + 18, top + 30), (cx + 14, top + 58), (cx - 14, top + 58)]),
           plate, PAL.SLATE, PAL.NAVY_XD)
    core = (PAL.RED, PAL.EMBER, PAL.ORANGE, PAL.CREAM)[charge]
    c.part(_el(cx - 6, top + 37, cx + 6, top + 49), core,
           PAL.YELLOW if charge else PAL.PINK, PAL.RED_DK)
    # heavy forearms: hanging to the knees, raised overhead on the tell
    for side in (-1, 1):
        sx = cx + side * 28
        if arms:
            fx, fy = cx + side * 20, top + 4
        else:
            fx, fy = cx + side * 36, top + 64 + int(math.sin(t + side) * 2)
        c.part(_ln([(sx, top + 30), (fx, fy)], 12), hide, PAL.BROWN_DK, PAL.INK, sep=True)
        c.part(_el(fx - 8, fy - 7, fx + 8, fy + 7), plate, PAL.SLATE, PAL.INK)
    # head and the horns, wider than the shoulders
    c.part(_el(cx - 12, top + 8, cx + 12, top + 30), hide, PAL.BROWN_DK, PAL.INK)
    for side in (-1, 1):
        c.part(_pg([(cx + side * 8, top + 14), (cx + side * 42, top + 2), (cx + side * 34, top + 8),
                    (cx + side * 12, top + 22)]), PAL.TAN_LT, PAL.CREAM, PAL.TAN)
    for ex in (cx + 2, cx + 7):
        c.dot(ex, top + 18, core if charge else PAL.RED)
        c.dot(ex, top + 19, core if charge else PAL.RED)
    return c.outline()


def boss(path, svg):
    """enemy_boss.png 6x5 of 96x104: idle 4, walk 6, attack 5, hurt 2, die 6."""
    _sheet([[_boss(bob=b) for b in (0, 1, 2, 1)],
            [_boss(t=i * 1.047, bob=i % 2) for i in range(6)],
            [_boss(charge=1, arms=1), _boss(charge=2, arms=1), _boss(charge=3, arms=1),
             _boss(charge=0, arms=0, bob=3), _boss(bob=1)],
            [_boss(hurt=1, bob=2), _boss(hurt=1)],
            [_boss(die=d) for d in (0, 1, 2, 3, 4, 6)]], 96, 104, path, svg)


# --------------------------------------------------------------------- hero
def _hero(bob=0, lean=0, crouch=0, legs=(0, 0, 0, 0), lift=0, fhand=(5, 9),
          bhand=(-4, 9), sword=70, scarf=(-9, 6), arc=None, glow=0, lie=0, kneel=0):
    """ART-12 hero at 1:1, 48x56 cell, figure ~30x48, facing right.

    Kept from the hand-made sheet: brown hair, red scarf trailing back, grey
    tunic, white sleeves and gloves, dark trousers, brown boots, steel sword
    with a gold guard.  `legs` = (front foot dx, back foot dx, front knee
    lift, back knee lift); `sword` is the blade angle in degrees (0 = right,
    90 = straight down); `arc` = (start, end) degrees of a swing trail.
    """
    c = Px(48, 56)
    if lie:                                   # last death frames: lying down
        y = 52
        c.part(_rc(6, y - 4, 22, y), PAL.NAVY_XD, PAL.SLATE_XD, PAL.INK)
        c.part(_rc(20, y - 7, 34, y), PAL.SLATE, PAL.SLATE_LT, PAL.SLATE_XD)
        c.part(_pg([(30, y - 6), (40, y - 8 + lie), (36, y - 2)]), PAL.RED, PAL.PINK, PAL.RED_DK)
        c.part(_el(33, y - 10, 44, y), PAL.TAN_LT, PAL.CREAM, PAL.TAN)
        c.part(_el(35, y - 11, 46, y - 3), PAL.BROWN, PAL.TAN, PAL.BROWN_DK)
        c.part(_rc(2, y - 2, 6, y + 1), PAL.BROWN_DK, PAL.BROWN, PAL.BROWN_XD)
        c.part(_ln([(10, y - 9), (26, y - 12)], 2), PAL.PLATE, PAL.CREAM, PAL.SLATE_LT)
        return c.outline()
    hx, hy = 23 + lean // 2, 37 + crouch + kneel
    sx, sy = 24 + lean, 22 + bob + crouch + kneel
    feet_y = 54 - lift
    # back leg, back arm, scarf tail behind the body
    ffx, bfx, fkl, bkl = legs
    for (fx, kl, col, shade) in ((bfx, bkl, PAL.NAVY_XD, PAL.INK),):
        kx, ky = hx - 1 + fx // 2, hy + 9 - kl
        c.part(_ln([(hx - 2, hy), (kx, ky)], 4), col, PAL.SLATE_XD, shade)
        c.part(_ln([(kx, ky), (hx - 2 + fx, feet_y - 2 - kl // 2)], 4), col, PAL.SLATE_XD, shade)
        c.part(_rc(hx - 4 + fx, feet_y - 3 - kl // 2, hx + 1 + fx, feet_y - kl // 2),
               PAL.BROWN_DK, PAL.BROWN, PAL.BROWN_XD)
    c.part(_ln([(sx - 4, sy + 2), (sx - 4 + bhand[0], sy + 2 + bhand[1])], 3),
           PAL.PLATE, PAL.CREAM, PAL.SLATE_LT)
    tx, ty = sx - 3 + scarf[0], sy + scarf[1]
    c.part(_pg([(sx - 3, sy - 2), (tx - 2, ty - 2), (tx + 1, ty + 2), (sx - 1, sy + 3)]),
           PAL.RED, PAL.PINK, PAL.RED_DK)
    # torso: grey tunic and a belt
    c.part(_pg([(sx - 6, sy), (sx + 5, sy), (hx + 5, hy), (hx - 5, hy)]),
           PAL.SLATE, PAL.SLATE_LT, PAL.SLATE_XD)
    c.part(_rc(hx - 5, hy - 3, hx + 5, hy - 2), PAL.BROWN, PAL.TAN, PAL.BROWN_DK)
    c.dot(hx + 1, hy - 3, PAL.GOLD)
    # front leg
    kx, ky = hx + 2 + ffx // 2, hy + 9 - fkl
    c.part(_ln([(hx + 2, hy), (kx, ky)], 4), PAL.NAVY_XD, PAL.SLATE_XD, PAL.INK, sep=True)
    c.part(_ln([(kx, ky), (hx + 2 + ffx, feet_y - 2 - fkl // 2)], 4), PAL.NAVY_XD, PAL.SLATE_XD, PAL.INK)
    c.part(_rc(hx + ffx, feet_y - 3 - fkl // 2, hx + 5 + ffx, feet_y - fkl // 2),
           PAL.BROWN_DK, PAL.BROWN, PAL.BROWN_XD)
    # head: face, hair swept back, an eye looking forward
    hc = (sx + 1, sy - 7)
    c.part(_el(hc[0] - 5, hc[1] - 6, hc[0] + 6, hc[1] + 6), PAL.TAN_LT, PAL.CREAM, PAL.TAN)
    c.part(_pg([(hc[0] - 7, hc[1] + 3), (hc[0] - 6, hc[1] - 5), (hc[0] - 1, hc[1] - 8),
                (hc[0] + 6, hc[1] - 6), (hc[0] + 7, hc[1] - 2), (hc[0] + 2, hc[1] - 3),
                (hc[0] - 1, hc[1] + 1), (hc[0] - 3, hc[1] + 5)]), PAL.BROWN, PAL.TAN, PAL.BROWN_DK)
    c.dot(hc[0] + 3, hc[1], PAL.INK); c.dot(hc[0] + 3, hc[1] + 1, PAL.INK)
    # scarf knot over the collar
    c.part(_rc(sx - 4, sy - 2, sx + 4, sy + 1), PAL.RED, PAL.PINK, PAL.RED_DK)
    # front arm, glove, sword with a gold guard
    hand = (sx + 3 + fhand[0], sy + 2 + fhand[1])
    c.part(_ln([(sx + 3, sy + 2), hand], 3), PAL.PLATE, PAL.CREAM, PAL.SLATE_LT, sep=True)
    if sword is not None:
        a = math.radians(sword)
        tip = (hand[0] + int(round(17 * math.cos(a))), hand[1] + int(round(17 * math.sin(a))))
        gx, gy = int(round(2 * math.sin(a))), int(round(-2 * math.cos(a)))
        c.part(_ln([hand, tip], 2), PAL.PLATE, PAL.CREAM, PAL.SLATE_LT, sep=True)
        c.part(_ln([(hand[0] - gx, hand[1] - gy), (hand[0] + gx, hand[1] + gy)], 2), PAL.GOLD)
    c.part(_el(hand[0] - 1, hand[1] - 1, hand[0] + 1, hand[1] + 1), PAL.CREAM)
    if glow:                                  # charged bolt in the open hand
        g = glow
        c.part(_el(hand[0] + 2 - g, hand[1] - g, hand[0] + 2 + g, hand[1] + g),
               PAL.WATER, PAL.CREAM, PAL.WATER_DK)
    c.outline()
    if arc:                                   # swing trail, drawn after the outline
        r = 18
        from PIL import ImageDraw
        d = ImageDraw.Draw(c.img)
        box = [hand[0] - r, hand[1] - r, hand[0] + r, hand[1] + r]
        d.arc(box, arc[0], arc[1], fill=PAL.CREAM + (255,), width=2)
        d.arc([box[0] + 2, box[1] + 2, box[2] - 2, box[3] - 2], arc[0], arc[1],
              fill=PAL.PLATE + (255,), width=1)
    return c


def hero(path, svg):
    """player_sprites.png v2 (ART-12): 7 cols x 8 rows of 48x56 at 1:1.

    Rows keep their meaning: idle 3, run 6, power 5, slash 6, thrust 6,
    jump 4, cast 3, die 7.  Collider stays 32x50, so no physics changes.
    """
    run = []
    for i in range(6):
        p = math.sin(i * math.pi / 3)
        q = math.cos(i * math.pi / 3)
        run.append(_hero(lean=3, bob=(1 if i % 3 == 0 else 0),
                         legs=(int(7 * p), int(-7 * p), max(0, int(6 * q)), max(0, int(-6 * q))),
                         fhand=(4 - int(3 * p), 8), bhand=(-3 + int(4 * p), 8),
                         sword=40 - int(10 * p), scarf=(-12, 2 + (i % 2))))
    rows = [
        [_hero(), _hero(bob=1), _hero(bob=1, scarf=(-9, 7))],
        run,
        [_hero(crouch=2, fhand=(-4, -12), sword=-130, scarf=(-10, 4)),
         _hero(crouch=2, fhand=(-2, -13), sword=-110, scarf=(-10, 4)),
         _hero(fhand=(6, -8), sword=-40, arc=(-150, -30), legs=(5, -4, 0, 0)),
         _hero(crouch=3, fhand=(8, 2), sword=30, arc=(-60, 40), legs=(6, -5, 0, 0)),
         _hero(crouch=3, fhand=(7, 6), sword=70, legs=(6, -5, 0, 0))],
        [_hero(fhand=(-6, 2), sword=170, legs=(3, -3, 0, 0)),
         _hero(fhand=(-5, 1), sword=180, legs=(3, -3, 0, 0)),
         _hero(lean=2, fhand=(3, 1), sword=-10, arc=(150, 230), legs=(5, -4, 0, 0)),
         _hero(lean=3, fhand=(7, 1), sword=0, arc=(200, 330), legs=(6, -5, 0, 0)),
         _hero(lean=2, fhand=(6, 3), sword=20, legs=(6, -5, 0, 0)),
         _hero(fhand=(5, 6), sword=50, legs=(4, -3, 0, 0))],
        [_hero(crouch=3, fhand=(-3, 4), sword=0, legs=(2, -4, 0, 0)),
         _hero(crouch=4, fhand=(-5, 4), sword=0, legs=(2, -5, 0, 0)),
         _hero(crouch=4, lean=4, fhand=(4, 2), sword=0, legs=(8, -7, 0, 0)),
         _hero(crouch=4, lean=5, fhand=(8, 2), sword=0, legs=(9, -8, 0, 0)),
         _hero(crouch=3, lean=4, fhand=(8, 3), sword=5, legs=(8, -7, 0, 0)),
         _hero(crouch=1, lean=2, fhand=(5, 6), sword=40, legs=(4, -3, 0, 0))],
        [_hero(crouch=4, legs=(3, -3, 0, 0), fhand=(4, 8), sword=60),
         _hero(lift=2, legs=(1, -4, 0, 4), fhand=(5, 2), sword=20, scarf=(-10, 9)),
         _hero(lift=6, legs=(3, -2, 6, 7), fhand=(5, 0), sword=10, scarf=(-11, 8)),
         _hero(lift=7, legs=(4, -1, 8, 8), fhand=(6, -1), sword=0, scarf=(-11, 5))],
        [_hero(fhand=(6, 2), sword=None, glow=1),
         _hero(fhand=(7, 1), sword=None, glow=2),
         _hero(fhand=(8, 1), sword=None, glow=3)],
        [_hero(lean=-2, bob=1, fhand=(4, 10), sword=80),
         _hero(lean=-3, crouch=3, fhand=(3, 12), sword=95),
         _hero(kneel=6, legs=(4, -6, 4, 0), fhand=(4, 10), sword=100),
         _hero(kneel=10, legs=(5, -7, 6, 0), fhand=(5, 8), sword=110),
         _hero(lie=1), _hero(lie=2), _hero(lie=3)],
    ]
    _sheet(rows, 48, 56, path, svg)


# ===========================================================================
def main():
    os.makedirs(SPRITES, exist_ok=True)
    os.makedirs(SVG, exist_ok=True)
    print("Generating art into", SPRITES)
    jobs = [
        (power_health,   "power_health.png"),
        (power_speed,    "power_speed.png"),
        (power_invincible, "power_invincible.png"),
        (power_magnet,   "power_magnet.png"),
        (power_shield,   "power_shield.png"),
        (power_coins,    "power_coins.png"),
        (power_jump,     "power_jump.png"),
        (power_damage,   "power_damage.png"),
        (heart_icon,     "heart.png"),
        (projectile,     "proj.png"),
        (fireball,       "fireball.png"),
        (shockwave,      "shockwave.png"),
        (archer,         "archer.png"),
        (bomber,         "bomber.png"),
        (slime,          "enemy_slime.png"),
        (skeleton,       "enemy_skeleton.png"),
        (bat,            "enemy_bat.png"),
        (mage,           "enemy_mage.png"),
        (enemy_archer,   "enemy_archer.png"),
        (enemy_bomber,   "enemy_bomber.png"),
        (boss,           "enemy_boss.png"),
        (hero,           "player_sprites.png"),
    ]
    only = set(sys.argv[1:])     # e.g. `build_art.py enemy_slime.png`
    if only:
        jobs = [j for j in jobs if j[1] in only]
    for fn, name in jobs:
        png = os.path.join(SPRITES, name)
        svg = os.path.join(SVG, name.replace(".png", ".svg"))
        fn(png, svg)
        print("  %-22s + svg/%s" % (name, os.path.basename(svg)))


if __name__ == "__main__":
    main()
