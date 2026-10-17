#!/usr/bin/env python3
"""
A tiny drawing layer with two backends: Pillow (PNG) and SVG.

Why this exists
---------------
The game loads PNGs; the user asked for SVG art they can edit.  Generating
both from two separate code paths is how they drift apart -- change the PNG
and the SVG quietly keeps the old shape.  So a sprite is described ONCE as a
list of primitives and rendered by whichever backend is needed.

There is also a hard practical reason not to reuse an off-the-shelf SVG
rasteriser.  Tried and rejected on this machine:

  * ImageMagick's internal MSVG renderer -- silently wrong.  The existing
    heart.svg (a radial gradient) rasterised as a solid black blob.
  * cairosvg -- needs libcairo, not installed.
  * svglib + reportlab -- parses fine, but renderPM also requires libcairo.

So the PNG path goes through Pillow, which is already a dependency of every
other tool here.  The SVG path emits plain <rect>/<polygon>/<ellipse>/
<polyline>, which is also what makes the art editable by hand afterwards.

Deliberately NOT supported: gradients, filters, blur, text, clipping paths.
The house style is flat shading with hard edges, so nothing needs them, and
leaving them out is what keeps both backends exactly equivalent.
"""

from PIL import Image, ImageDraw

from palette import PAL


def _rgb(c):
    """Accept a 3-tuple, a 4-tuple, or None."""
    if c is None:
        return None
    return tuple(c[:3])


def _svg_color(c):
    if c is None:
        return "none"
    return PAL.hex(_rgb(c))


def _fmt(v):
    """Trim float noise: 12.0 -> '12', 12.5 -> '12.5'."""
    f = float(v)
    return str(int(f)) if f == int(f) else ("%.2f" % f).rstrip("0").rstrip(".")


class Canvas:
    """A list of drawing ops plus the width/height they live in."""

    def __init__(self, w, h, title=""):
        self.w = w
        self.h = h
        self.title = title
        self.ops = []

    # --- primitives -------------------------------------------------------
    # Every one takes an optional `outline`/`lw` so shapes can carry the
    # heavy #1c121c rim that defines the platformer.png look.

    def rect(self, x, y, w, h, fill, outline=None, lw=0, r=0):
        self.ops.append(("rect", (x, y, w, h), fill, outline, lw, r))
        return self

    def poly(self, pts, fill, outline=None, lw=0):
        """pts: sequence of (x, y).  Auto-closed."""
        self.ops.append(("poly", [tuple(p) for p in pts], fill, outline, lw, 0))
        return self

    def line(self, pts, stroke, lw=2, cap="round", join="round"):
        """Open polyline.  Use for limbs, antennae, whip-like shapes."""
        self.ops.append(("line", [tuple(p) for p in pts], stroke, None, lw,
                         0 if cap == "butt" else 1, cap, join))
        return self

    def circle(self, cx, cy, r, fill, outline=None, lw=0):
        self.ops.append(("circle", (cx, cy, r), fill, outline, lw, 0))
        return self

    def ellipse(self, cx, cy, rx, ry, fill, outline=None, lw=0):
        self.ops.append(("ellipse", (cx, cy, rx, ry), fill, outline, lw, 0))
        return self

    # --- backends ---------------------------------------------------------

    def render(self, d, ox=0, oy=0, scale=1):
        """Draw this canvas's ops into an existing ImageDraw with an offset."""
        s = max(1, int(scale))
        for op in self.ops:
            kind, geo, fill, outline, lw, extra = op[:6]
            f = _rgb(fill)
            o = _rgb(outline)
            L = int(round(lw * s))
            if kind == "rect":
                x, y, w, h = [v * s for v in geo]
                r = extra * s
                box = [ox + x, oy + y, ox + x + w, oy + y + h]
                if r > 0:
                    d.rounded_rectangle(box, radius=r, fill=f, outline=o, width=L)
                else:
                    d.rectangle(box, fill=f, outline=o, width=L)
            elif kind == "poly":
                d.polygon([(ox + x * s, oy + y * s) for x, y in geo],
                          fill=f, outline=o)
                if o and L:
                    closed = list(geo) + [geo[0]]
                    d.line([(ox + x * s, oy + y * s) for x, y in closed],
                           fill=o, width=L, joint="curve")
            elif kind == "circle":
                cx, cy, r = [v * s for v in geo]
                d.ellipse([ox + cx - r, oy + cy - r, ox + cx + r, oy + cy + r],
                          fill=f, outline=o, width=L)
            elif kind == "ellipse":
                cx, cy, rx, ry = [v * s for v in geo]
                d.ellipse([ox + cx - rx, oy + cy - ry,
                           ox + cx + rx, oy + cy + ry],
                          fill=f, outline=o, width=L)
            elif kind == "line":
                pts = [(ox + x * s, oy + y * s) for x, y in geo]
                d.line(pts, fill=_rgb(op[2]), width=L, joint="curve")

    def to_png(self, path, scale=1):
        """Rasterise with Pillow.  `scale` supersamples for smoother edges."""
        s = max(1, int(scale))
        img = Image.new("RGBA", (self.w * s, self.h * s), (0, 0, 0, 0))
        self.render(ImageDraw.Draw(img), 0, 0, s)
        if s > 1:
            img = img.resize((self.w, self.h), Image.LANCZOS)
        img.save(path)
        return img

    def to_svg(self):
        out = [
            '<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" '
            'viewBox="0 0 %d %d">' % (self.w, self.h, self.w, self.h),
        ]
        if self.title:
            out.append("  <title>%s</title>" % self.title)
        for op in self.ops:
            kind, geo = op[0], op[1]
            fill, outline, lw = op[2], op[3], op[4]
            if kind == "rect":
                x, y, w, h = geo
                r = op[5]
                extra = ' rx="%s"' % _fmt(r) if r else ""
                out.append(
                    '  <rect x="%s" y="%s" width="%s" height="%s"%s '
                    'fill="%s"%s/>'
                    % (_fmt(x), _fmt(y), _fmt(w), _fmt(h), extra,
                       _svg_color(fill),
                       (' stroke="%s" stroke-width="%s"' % (_svg_color(outline),
                                                            _fmt(lw)))
                       if outline else ""))
            elif kind == "poly":
                pts = " ".join("%s,%s" % (_fmt(x), _fmt(y)) for x, y in geo)
                out.append(
                    '  <polygon points="%s" fill="%s"%s/>'
                    % (pts, _svg_color(fill),
                       (' stroke="%s" stroke-width="%s"' % (_svg_color(outline),
                                                            _fmt(lw)))
                       if outline else ""))
            elif kind == "circle":
                cx, cy, r = geo
                out.append(
                    '  <circle cx="%s" cy="%s" r="%s" fill="%s"%s/>'
                    % (_fmt(cx), _fmt(cy), _fmt(r), _svg_color(fill),
                       (' stroke="%s" stroke-width="%s"' % (_svg_color(outline),
                                                            _fmt(lw)))
                       if outline else ""))
            elif kind == "ellipse":
                cx, cy, rx, ry = geo
                out.append(
                    '  <ellipse cx="%s" cy="%s" rx="%s" ry="%s" fill="%s"%s/>'
                    % (_fmt(cx), _fmt(cy), _fmt(rx), _fmt(ry), _svg_color(fill),
                       (' stroke="%s" stroke-width="%s"' % (_svg_color(outline),
                                                            _fmt(lw)))
                       if outline else ""))
            elif kind == "line":
                pts = " ".join("%s,%s" % (_fmt(x), _fmt(y)) for x, y in geo)
                cap = op[6] if len(op) > 6 else "round"
                out.append(
                    '  <polyline points="%s" fill="none" stroke="%s" '
                    'stroke-width="%s" stroke-linecap="%s" stroke-linejoin="%s"/>'
                    % (pts, _svg_color(fill), _fmt(lw), cap,
                       op[7] if len(op) > 7 else "round"))
        out.append("</svg>")
        return "\n".join(out) + "\n"

    def save(self, png_path, svg_path=None, scale=1):
        self.to_png(png_path, scale=scale)
        if svg_path:
            with open(svg_path, "w") as f:
                f.write(self.to_svg())
        return png_path


class Sheet:
    """A grid of cells, each a Canvas offset into one image / one SVG.

    Sprite sheets are the same problem one level up: the PNG and the SVG must
    contain the same cells in the same order, so both are emitted from the
    same list of (canvas, col, row) placements.
    """

    def __init__(self, cols, rows, cw, ch):
        self.cols, self.rows, self.cw, self.ch = cols, rows, cw, ch
        self.cells = []          # list of (col, row, Canvas)

    def sub(self, col, row, title=""):
        c = Canvas(self.cw, self.ch, title)
        self.cells.append((col, row, c))
        return c

    def to_png(self, path, scale=1):
        s = max(1, int(scale))
        img = Image.new("RGBA",
                        (self.cols * self.cw * s, self.rows * self.ch * s),
                        (0, 0, 0, 0))
        d = ImageDraw.Draw(img)
        for col, row, cv in self.cells:
            cv.render(d, col * self.cw * s, row * self.ch * s, s)
        if s > 1:
            img = img.resize((self.cols * self.cw, self.rows * self.ch),
                             Image.LANCZOS)
        img.save(path)
        return img

    def to_svg(self):
        W = self.cols * self.cw
        H = self.rows * self.ch
        out = [
            '<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" '
            'viewBox="0 0 %d %d">' % (W, H, W, H),
        ]
        for col, row, cv in self.cells:
            body = cv.to_svg()
            # Re-emit the cell's shapes inside a translated group so the sheet
            # is one editable document rather than a pile of nested <svg>s.
            inner = []
            for ln in body.splitlines()[1:-1]:
                inner.append("  " + ln.strip())
            out.append('  <!-- cell %d,%d -->' % (col, row))
            out.append('  <g transform="translate(%d,%d)">' %
                       (col * self.cw, row * self.ch))
            out.extend("  " + l for l in inner)
            out.append("  </g>")
        out.append("</svg>")
        return "\n".join(out) + "\n"

    def save(self, png_path, svg_path=None, scale=1):
        self.to_png(png_path, scale=scale)
        if svg_path:
            with open(svg_path, "w") as f:
                f.write(self.to_svg())
        return png_path
