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
import os

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
    ]
    for fn, name in jobs:
        png = os.path.join(SPRITES, name)
        svg = os.path.join(SVG, name.replace(".png", ".svg"))
        fn(png, svg)
        print("  %-22s + svg/%s" % (name, os.path.basename(svg)))


if __name__ == "__main__":
    main()
