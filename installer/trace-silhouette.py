#!/usr/bin/env python3
"""Trace the controller outline out of installer/icon-source.png.

    python3 trace-silhouette.py            writes controller-path.txt
    python3 trace-silhouette.py --check    writes nothing, exit 1 if it differs

⭐ WHY A TRACE AND NOT A HAND-DRAWN PATH. Three attempts at drawing a DualSense
outline from memory produced something rhoquinn8217 fairly described as a
tooth: the grips came out short and the top edge domed, where the real shape is
widest at the very top and has long grips sweeping down and out. Measuring the
art is the only way to get proportions right, so the art is measured.

THE PIPELINE, and both halves live in this folder:

    icon-source.png  --[ this script ]-->  controller-path.txt
    controller-path.txt  --[ build-icon.ps1 ]-->  ../app/ctm-usbip.ico

⛔ THE ICO IS NOT EDITED BY HAND, EVER. It is one file and the whole app wears
it: ctm-usbip.rc links it as `1 ICON` for the exe and again as `101 RCDATA` so
the same bytes are served at /favicon.ico, which is where the configs window's
title bar gets its icon; both .iss files point SetupIconFile at it; the tray
and the window load it from the exe's resources. One file, every surface.

Standard library only, on purpose: this runs on a machine with nothing
installed, which is the same reason gen_bridge_badge.py is written this way.
"""

from __future__ import annotations

import struct
import sys
import zlib
from collections import deque
from pathlib import Path

HERE = Path(__file__).resolve().parent
SOURCE = HERE / "icon-source.png"
TARGET = HERE / "controller-path.txt"

# ⭐ THE LOOK, and the only numbers here that are a matter of taste.
FRAME_W = 100.0        # the path is emitted in a frame this many units wide
SIMPLIFY = 2.5         # Ramer-Douglas-Peucker tolerance, in source pixels
MIN_PIECE = 2000       # ignore specks smaller than this many pixels
THRESHOLD = 128        # below this is ink


# ------------------------------------------------------------------ PNG, by hand

def _unfilter(raw: bytes, w: int, h: int, bpp: int) -> bytearray:
    stride = w * bpp
    out = bytearray()
    prev = bytearray(stride)
    pos = 0
    for _ in range(h):
        ft = raw[pos]
        pos += 1
        line = bytearray(raw[pos:pos + stride])
        pos += stride
        if ft == 1:
            for x in range(bpp, stride):
                line[x] = (line[x] + line[x - bpp]) & 0xFF
        elif ft == 2:
            for x in range(stride):
                line[x] = (line[x] + prev[x]) & 0xFF
        elif ft == 3:
            for x in range(stride):
                a = line[x - bpp] if x >= bpp else 0
                line[x] = (line[x] + ((a + prev[x]) >> 1)) & 0xFF
        elif ft == 4:
            for x in range(stride):
                a = line[x - bpp] if x >= bpp else 0
                c = prev[x - bpp] if x >= bpp else 0
                p = a + prev[x] - c
                pa, pb, pc = abs(p - a), abs(p - prev[x]), abs(p - c)
                pred = a if (pa <= pb and pa <= pc) else (prev[x] if pb <= pc else c)
                line[x] = (line[x] + pred) & 0xFF
        elif ft != 0:
            raise ValueError("bad PNG filter {}".format(ft))
        out += line
        prev = line
    return out


def load_png(path: Path):
    """8-bit grey, RGB or RGBA. Returns (w, h, luminance bytes)."""
    data = path.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("{}: not a PNG".format(path.name))
    idat = b""
    w = h = bd = ct = None
    i = 8
    while i < len(data):
        (ln,) = struct.unpack(">I", data[i:i + 4])
        typ = data[i + 4:i + 8]
        body = data[i + 8:i + 8 + ln]
        if typ == b"IHDR":
            w, h, bd, ct = struct.unpack(">IIBB", body[:10])
        elif typ == b"IDAT":
            idat += body
        i += 12 + ln
    if bd != 8 or ct not in (0, 2, 6):
        raise ValueError("{}: need 8-bit grey, RGB or RGBA".format(path.name))
    bpp = {0: 1, 2: 3, 6: 4}[ct]
    pix = _unfilter(zlib.decompress(idat), w, h, bpp)
    if bpp == 1:
        return w, h, pix
    lum = bytearray(w * h)
    for k in range(w * h):
        r, g, b = pix[k * bpp], pix[k * bpp + 1], pix[k * bpp + 2]
        lum[k] = (299 * r + 587 * g + 114 * b) // 1000
    return w, h, lum


# -------------------------------------------------------------------- the shape

def solid_mask(w: int, h: int, pix) -> bytearray:
    """True where the art is, with interior holes filled.

    Flood the light background in from the border; whatever the flood cannot
    reach is either ink or a hole enclosed by ink, and both belong to the
    silhouette. ⭐ This is what removes the D-pad, the face buttons and the
    sticks: they are holes, so they fill, and what is left is the outline."""
    outside = bytearray(w * h)
    q = deque()

    def seed(x, y):
        k = y * w + x
        if pix[k] >= THRESHOLD and not outside[k]:
            outside[k] = 1
            q.append((x, y))

    for x in range(w):
        seed(x, 0)
        seed(x, h - 1)
    for y in range(h):
        seed(0, y)
        seed(w - 1, y)
    while q:
        x, y = q.popleft()
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            nx, ny = x + dx, y + dy
            if 0 <= nx < w and 0 <= ny < h:
                k = ny * w + nx
                if not outside[k] and pix[k] >= THRESHOLD:
                    outside[k] = 1
                    q.append((nx, ny))
    return bytearray(0 if outside[k] else 1 for k in range(w * h))


def components(w: int, h: int, mask):
    """Connected pieces, largest first.

    ⚠️ There is more than one. The source draws the shoulder buttons and the
    touchpad detached from the body, and the touchpad pokes ABOVE the body's
    top edge so the gap around it reaches the outside and never fills. Only the
    largest piece -- the body -- is wanted."""
    seen = bytearray(w * h)
    out = []
    for s in range(w * h):
        if mask[s] and not seen[s]:
            piece = bytearray(w * h)
            q = deque([s])
            seen[s] = piece[s] = 1
            n = 1
            while q:
                k = q.popleft()
                x, y = k % w, k // w
                for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    nx, ny = x + dx, y + dy
                    if 0 <= nx < w and 0 <= ny < h:
                        j = ny * w + nx
                        if mask[j] and not seen[j]:
                            seen[j] = piece[j] = 1
                            n += 1
                            q.append(j)
            out.append((n, piece))
    out.sort(key=lambda t: -t[0])
    return out


def mirror_mask(w: int, h: int, mask, axis: float) -> bytearray:
    """Keep the left half and mirror it onto the right.

    ⛔ THIS CANNOT BE DONE ON THE CONTOUR. The outline crosses the centre line
    four times -- along the top edge and twice around the apex of the notch
    between the grips -- so the points with x <= axis are three separate runs,
    and stitching them head to tail draws a shape that was never traced. On the
    mask there is no ordering to get wrong.

    The art is not quite symmetric and an icon should be."""
    a = int(round(axis))
    out = bytearray(w * h)
    for y in range(h):
        row = y * w
        for x in range(w):
            src = x if x <= a else 2 * a - x
            if 0 <= src < w and mask[row + src]:
                out[row + x] = 1
    return out


def trace_boundary(w: int, h: int, mask):
    """Moore-neighbour boundary following, clockwise from the topmost pixel."""
    start = None
    for y in range(h):
        for x in range(w):
            if mask[y * w + x]:
                start = (x, y)
                break
        if start:
            break
    if not start:
        raise ValueError("nothing to trace")

    nbr = ((1, 0), (1, 1), (0, 1), (-1, 1), (-1, 0), (-1, -1), (0, -1), (1, -1))

    def on(px, py):
        return 0 <= px < w and 0 <= py < h and mask[py * w + px]

    contour = [start]
    cur, d = start, 6
    limit = 8 * (w + h)
    while True:
        found = False
        for k in range(8):
            i = (d + 6 + k) % 8
            nx, ny = cur[0] + nbr[i][0], cur[1] + nbr[i][1]
            if on(nx, ny):
                cur, d, found = (nx, ny), i, True
                break
        if not found or (cur == start and len(contour) > 2) or len(contour) > limit:
            break
        contour.append(cur)
    return contour


def rdp(pts, eps):
    """Ramer-Douglas-Peucker, iterative so a long contour cannot blow the stack."""
    keep = [False] * len(pts)
    keep[0] = keep[-1] = True
    stack = [(0, len(pts) - 1)]
    while stack:
        a, b = stack.pop()
        if b <= a + 1:
            continue
        ax, ay = pts[a]
        bx, by = pts[b]
        dx, dy = bx - ax, by - ay
        norm = (dx * dx + dy * dy) ** 0.5 or 1.0
        worst, wi = 0.0, a
        for i in range(a + 1, b):
            px, py = pts[i]
            dist = abs(dy * px - dx * py + bx * ay - by * ax) / norm
            if dist > worst:
                worst, wi = dist, i
        if worst > eps:
            keep[wi] = True
            stack.append((a, wi))
            stack.append((wi, b))
    return [p for p, k in zip(pts, keep) if k]


def to_beziers(pts, scale, ox, oy, alpha=0.5):
    """Centripetal Catmull-Rom through the points, as closed cubic segments.

    ⚠️ alpha MATTERS. The uniform form assumes evenly spaced points and these
    are not: RDP leaves three-pixel steps round a tight corner and three
    hundred along a straight run. Uniform weighting on that spacing throws
    control points outside the hull -- a span between y=3.4 and y=4.3 came back
    with a handle at y=1.7 -- which renders as a bulge or a cusp. Centripetal
    weights each span by its own length, and cannot self-intersect within a
    segment."""
    n = len(pts)

    def P(i):
        x, y = pts[i % n]
        return ((x - ox) * scale, (y - oy) * scale)

    def span(a, b):
        d = ((b[0] - a[0]) ** 2 + (b[1] - a[1]) ** 2) ** 0.5
        return max(d, 1e-6) ** alpha

    segs = []
    for i in range(n):
        p0, p1, p2, p3 = P(i - 1), P(i), P(i + 1), P(i + 2)
        d1, d2, d3 = span(p0, p1), span(p1, p2), span(p2, p3)
        c1, c2 = [], []
        for k in (0, 1):
            a = (d1 * d1 * p2[k] - d2 * d2 * p0[k]
                 + (2 * d1 * d1 + 3 * d1 * d2 + d2 * d2) * p1[k])
            c1.append(a / (3 * d1 * (d1 + d2)))
            b = (d3 * d3 * p1[k] - d2 * d2 * p3[k]
                 + (2 * d3 * d3 + 3 * d3 * d2 + d2 * d2) * p2[k])
            c2.append(b / (3 * d3 * (d3 + d2)))
        segs.append((c1[0], c1[1], c2[0], c2[1], p2[0], p2[1]))
    return P(0), segs


def render(start, segs, height) -> str:
    out = ["# The DS5-USBIP controller outline.",
           "# GENERATED by trace-silhouette.py from icon-source.png. Do not edit.",
           "# A frame {:g} units wide; the shape is {:.4f} tall in it.".format(
               FRAME_W, height),
           "height {:.4f}".format(height),
           "start {:.4f} {:.4f}".format(start[0], start[1])]
    for s in segs:
        out.append("bez " + " ".join("{:.4f}".format(v) for v in s))
    return "\n".join(out) + "\n"


def main() -> int:
    check = "--check" in sys.argv[1:]
    if not SOURCE.is_file():
        print("missing source: {}".format(SOURCE), file=sys.stderr)
        return 2

    w, h, pix = load_png(SOURCE)
    mask = solid_mask(w, h, pix)

    xs = [k % w for k in range(w * h) if mask[k]]
    ys = [k // w for k in range(w * h) if mask[k]]
    x0, x1, y0, y1 = min(xs), max(xs), min(ys), max(ys)
    bw, bh = x1 - x0 + 1, y1 - y0 + 1
    scale = FRAME_W / bw
    axis = (x0 + x1) / 2.0

    pieces = components(w, h, mirror_mask(w, h, mask, axis))
    pieces = [p for p in pieces if p[0] >= MIN_PIECE]
    if not pieces:
        print("nothing large enough to trace", file=sys.stderr)
        return 2

    # ⭐ The BODY only. The shoulders and the touchpad are separate pieces and
    # the mark leaves them out: the shoulders because they are not wanted, the
    # touchpad because its distinctive trapezoid is the part of a DualSense an
    # examiner would point at, and the outline reads perfectly well without it.
    contour = trace_boundary(w, h, pieces[0][1])
    simple = rdp(contour, SIMPLIFY)
    start, segs = to_beziers(simple, scale, x0, y0)
    text = render(start, segs, bh * scale)

    print("source {}x{}   body {} px   bbox {}x{} (aspect {:.3f})".format(
        w, h, pieces[0][0], bw, bh, bw / float(bh)))
    print("contour {} points -> {} after simplify -> {} cubic segments".format(
        len(contour), len(simple), len(segs)))

    # ⛔ LINE ENDINGS, BOTH WAYS. Python's text mode writes CRLF on Windows and
    # git stores the file with LF, so a fresh clone has LF on disk while this
    # script makes CRLF: --check would then shout DRIFT at a file that had not
    # changed, on a machine where nothing was wrong. Written with explicit LF
    # so disk and git agree, and compared with the endings normalised so the
    # answer does not depend on how the file arrived.
    if check:
        if TARGET.is_file():
            have = TARGET.read_text(encoding="utf-8").replace("\r\n", "\n")
            if have == text.replace("\r\n", "\n"):
                print("OK: {} is what this script makes".format(TARGET.name))
                return 0
        print("DRIFT: {} is NOT what this script makes".format(TARGET.name))
        return 1

    TARGET.write_text(text, encoding="utf-8", newline="\n")
    print("wrote {} ({} bytes)".format(TARGET.name, len(text)))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
