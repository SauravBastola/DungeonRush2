#!/usr/bin/env python3
"""
The one palette for the whole game.

Every colour here was sampled out of assets/platformer.png (a 9x5 grid of
64px cells, 115049 opaque pixels, 34 distinct colours).  Those 34 colours are
the entire visual language of the terrain, and they are why the sheet reads as
one coherent world: nothing on it uses an outside colour.

Before this module the art was split across three unrelated palettes -- the
terrain sheet, the hand-drawn sprites, and the procedurally generated enemies
(which had their own INK/BONE/HOOD/CLOTH set).  The result
was a game where the enemies looked like they wandered in from another title.
Everything game-facing now draws from this table.

Percentages are the share of opaque pixels in platformer.png, so they double
as a guide to how loudly each colour should be used: #1e6f50 and #0c2e44 are
structural, #ffeb57 is a single-pixel highlight.

    >>> from palette import PAL
    >>> PAL.TEAL
    (30, 111, 80)
"""

# --- structural -----------------------------------------------------------
INK       = (0x1c, 0x12, 0x1c)   # 23.46%  near-black; the outline on everything
TEAL      = (0x1e, 0x6f, 0x50)   # 25.24%  primary terrain / foliage body
NAVY      = (0x0c, 0x2e, 0x44)   # 17.79%  deep shadow, dark stone, night water
BROWN_XD  = (0x39, 0x1f, 0x21)   #  8.51%  darkest brown; trunks, cave mouths
WATER     = (0x00, 0x98, 0xdc)   #  5.21%  bright water
BROWN     = (0x8a, 0x48, 0x36)   #  4.61%  mid brown; wood, soil, autumn canopy
BROWN_DK  = (0x5d, 0x2c, 0x28)   #  3.81%  dark red-brown
GRASS     = (0x5a, 0xc5, 0x4f)   #  3.00%  lit grass highlight
TAN       = (0xbf, 0x6f, 0x4a)   #  2.49%  warm tan; sand, stone faces
TEAL_DK   = (0x13, 0x4c, 0x4c)   #  1.68%  deep teal shade
WATER_DK  = (0x00, 0x69, 0xaa)   #  1.33%  mid water
TAN_LT    = (0xe6, 0x9c, 0x69)   #  1.11%  lit tan

# --- secondary ------------------------------------------------------------
SLATE_XD  = (0x2a, 0x2f, 0x4e)   #  0.33%  slate shadow
SLATE     = (0x42, 0x4c, 0x6e)   #  0.31%  slate body
GRASS_MID = (0x33, 0x98, 0x4b)   #  0.23%  mid grass
NAVY_DK   = (0x00, 0x39, 0x6d)   #  0.22%  deepest water
SLATE_LT  = (0x65, 0x73, 0x92)   #  0.17%  lit slate
RED       = (0xc4, 0x24, 0x30)   #  0.15%  blood, danger, enemy accent
RED_DK    = (0x89, 0x1e, 0x2b)   #  0.09%  dark red
PLATE     = (0x92, 0xa1, 0xb9)   #  0.07%  metal highlight
NAVY_XD   = (0x1a, 0x19, 0x32)   #  0.05%  near-black blue

# --- accents (use sparingly; these are a handful of pixels on the sheet) ---
PINK      = (0xf6, 0x81, 0x87)   #  0.03%
CHROMA    = (0xdb, 0x3f, 0xfd)   #  0.02%  *** CHROMA KEY -- NEVER DRAW WITH IT ***
VIOLET    = (0x7a, 0x09, 0xfa)   #  0.02%
ORANGE_DK = (0xed, 0x76, 0x14)   #  0.01%
ORANGE    = (0xff, 0xa2, 0x14)   #  0.01%
ORCHID    = (0xf3, 0x89, 0xf5)   #  0.01%
YELLOW    = (0xff, 0xeb, 0x57)   #  0.01%
CREAM     = (0xf9, 0xe6, 0xcf)   #  0.01%
BLUSH     = (0xfd, 0xd2, 0xed)   #  0.01%
GOLD      = (0xff, 0xc8, 0x25)   #  0.01%
EMBER     = (0xff, 0x50, 0x00)   #  0.01%
RUST      = (0xc6, 0x45, 0x24)   #  0.00%
RUST_DK   = (0x8e, 0x25, 0x1d)   #  0.00%

TRANSPARENT = (0, 0, 0, 0)


class _Pal:
    """Attribute access to every colour above, plus the derived helpers.

    Exists so call sites read `PAL.TEAL` instead of importing twenty names,
    and so the derived ramp functions have somewhere to live.
    """

    def __init__(self):
        import sys
        m = sys.modules[__name__]
        for name in dir(m):
            if name.isupper() and isinstance(getattr(m, name), tuple):
                setattr(self, name, getattr(m, name))

    # --- derived ramps ----------------------------------------------------
    @staticmethod
    def mix(a, b, t, snap=False):
        """Linear blend, t=0 -> a, t=1 -> b.

        `snap=True` rounds the result to the nearest real sheet colour.
        """
        t = max(0.0, min(1.0, t))
        c = tuple(int(round(a[i] + (b[i] - a[i]) * t)) for i in range(3))
        return _Pal.snap(c) if snap else c

    @staticmethod
    def shade(c, t, snap=True):
        """Darken toward INK: t=0 unchanged, t=1 full ink."""
        return _Pal.mix(c, INK, t, snap)

    @staticmethod
    def light(c, t, snap=True):
        """Lighten toward CREAM: t=0 unchanged, t=1 full cream."""
        return _Pal.mix(c, CREAM, t, snap)

    @staticmethod
    def snap(c):
        """Nearest colour that actually exists on platformer.png.

        This defaults ON for shade()/light(), and that is the whole point of
        the module.  Blending toward CREAM looks harmless on paper, but the
        sheet never lightens that way -- 22% toward cream turns TEAL #1e6f50
        into #4e896c, a washed-out sage that is 57 units from any colour the
        terrain uses.  The sheet highlights toward SATURATED neighbours
        (#33984b, #5ac54f), so uncontrolled blending quietly invents colours
        from outside the world.  Snapping keeps every derived shade on a real
        ramp step.
        """
        best, bd = None, None
        for p in SAFE:
            d = sum((c[i] - p[i]) ** 2 for i in range(3))
            if bd is None or d < bd:
                best, bd = p, d
        return best

    @staticmethod
    def hex(c):
        return "#%02x%02x%02x" % tuple(c)


PAL = _Pal()

# The set of colours that are safe to draw with -- everything except the
# chroma key.  build_objects.py keys transparency off CHROMA, so painting it
# into a sprite would punch holes in the art.
SAFE = tuple(
    v for k, v in sorted(globals().items())
    if k.isupper() and isinstance(v, tuple) and len(v) == 3 and v != CHROMA
)

if __name__ == "__main__":
    for k in sorted(n for n in dir(PAL) if n.isupper()):
        v = getattr(PAL, k)
        print("  %-10s %s" % (k, PAL.hex(v)))
