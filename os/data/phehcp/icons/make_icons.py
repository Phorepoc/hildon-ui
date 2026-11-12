# Copyright The Phorepoc Authors
# SPDX-License-Identifier: BSD-2-Clause
"""make_icons.py -- draws the control panel's icons (ours, BSD-2-Clause): general_clock, general_globe, general_display, filemanager_unknown_file (the grid's fallback), 48x48 RGBA PNG, no dependencies.
    python3 make_icons.py   -> hicolor/48x48/hildon/<name>.png
Each icon is a rounded dark tile with a white line drawing and one blue accent (the Hildon touch look); shapes are distance functions, 4x4 supersampled."""
import math
import os
import struct
import zlib

N, SS = 48, 4
TILE, LINE, ACCENT = (0x2B, 0x2D, 0x31), (0xF2, 0xF3, 0xF4), (0x35, 0xA7, 0xF5)


def seg(px, py, ax, ay, bx, by):
    dx, dy = bx - ax, by - ay
    t = max(0.0, min(1.0, ((px - ax) * dx + (py - ay) * dy) / (dx * dx + dy * dy)))
    return math.hypot(px - ax - t * dx, py - ay - t * dy)


def rrect(px, py, x0, y0, x1, y1, r):                       # signed distance to a rounded rectangle
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
    qx, qy = abs(px - cx) - ((x1 - x0) / 2 - r), abs(py - cy) - ((y1 - y0) / 2 - r)
    return math.hypot(max(qx, 0), max(qy, 0)) + min(max(qx, qy), 0) - r


def clock(x, y):
    d = abs(math.hypot(x - 24, y - 24) - 14)
    if d < 1.5: return LINE
    if seg(x, y, 24, 24, 24, 14) < 1.5: return LINE
    if seg(x, y, 24, 24, 31, 28) < 1.5: return ACCENT
    return None


def globe(x, y):
    r = math.hypot(x - 24, y - 24)
    if abs(r - 15) < 1.4: return LINE
    if abs(math.hypot((x - 24) / 0.45, y - 24) - 15) < 1.8: return LINE
    if r < 15 and (abs(y - 24) < 0.9 or abs(y - 17) < 0.7 and abs(x - 24) < 13.6 or abs(y - 31) < 0.7 and abs(x - 24) < 13.6): return ACCENT if abs(y - 24) < 0.9 else LINE
    return None


def sheet(x, y):
    if abs(rrect(x, y, 15, 9, 33, 39, 2.5)) < 1.4: return LINE
    if 22 < y < 24 and 19 < x < 29: return ACCENT
    if 27 < y < 29 and 19 < x < 29: return LINE
    return None


def display(x, y):                                          # a phone screen with a light (a sun: disc and rays) on it
    if abs(rrect(x, y, 13, 7, 35, 41, 3)) < 1.4: return LINE
    if 34 < y < 36 and 21 < x < 27: return LINE
    r = math.hypot(x - 24, y - 21)
    if r < 3.6: return ACCENT
    for k in range(8):
        a = k * math.pi / 4
        if seg(x, y, 24 + 6 * math.cos(a), 21 + 6 * math.sin(a), 24 + 8.5 * math.cos(a), 21 + 8.5 * math.sin(a)) < 0.9: return ACCENT
    return None


def render(shape):
    rows = []
    for j in range(N):
        row = bytearray([0])
        for i in range(N):
            acc, cov = [0, 0, 0], 0
            for sj in range(SS):
                for si in range(SS):
                    x, y = i + (si + .5) / SS, j + (sj + .5) / SS
                    if rrect(x, y, 2, 2, 46, 46, 9) > 0: continue
                    c = shape(x, y) or TILE
                    for k in range(3): acc[k] += c[k]
                    cov += 1
            n = SS * SS
            row += bytes([acc[0] // cov, acc[1] // cov, acc[2] // cov, 255 * cov // n]) if cov else bytes(4)
        rows.append(bytes(row))
    raw = b"".join(rows)

    def chunk(t, d):
        return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", N, N, 8, 6, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")


here = os.path.dirname(os.path.abspath(__file__))
out = os.path.join(here, "hicolor", "48x48", "hildon")
os.makedirs(out, exist_ok=True)
for name, shape in (("general_clock", clock), ("general_globe", globe), ("filemanager_unknown_file", sheet), ("general_display", display)):
    open(os.path.join(out, name + ".png"), "wb").write(render(shape))
