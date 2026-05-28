#!/usr/bin/env python3
"""Generate three 32x32 monochrome icons (settings/wifi/bluetooth)
and emit page-major C byte arrays compatible with render_icon."""

import math

W = H = 32
CX = CY = 16

def empty():
    return [[0] * W for _ in range(H)]

def setp(grid, x, y, on=1):
    if 0 <= x < W and 0 <= y < H:
        grid[y][x] = on

def line(grid, x0, y0, x1, y1):
    # Bresenham
    dx = abs(x1 - x0); dy = -abs(y1 - y0)
    sx = 1 if x0 < x1 else -1
    sy = 1 if y0 < y1 else -1
    err = dx + dy
    while True:
        setp(grid, x0, y0)
        if x0 == x1 and y0 == y1: break
        e2 = 2 * err
        if e2 >= dy: err += dy; x0 += sx
        if e2 <= dx: err += dx; y0 += sy

def line_thick(grid, x0, y0, x1, y1):
    line(grid, x0, y0, x1, y1)
    line(grid, x0+1, y0, x1+1, y1)

def filled_circle(grid, cx, cy, r):
    for y in range(cy - r - 1, cy + r + 2):
        for x in range(cx - r - 1, cx + r + 2):
            dx, dy = x - cx, y - cy
            if dx*dx + dy*dy <= r*r:
                setp(grid, x, y)

def ring(grid, cx, cy, r_outer, r_inner):
    for y in range(cy - r_outer - 1, cy + r_outer + 2):
        for x in range(cx - r_outer - 1, cx + r_outer + 2):
            d2 = (x - cx)**2 + (y - cy)**2
            if r_inner*r_inner < d2 <= r_outer*r_outer:
                setp(grid, x, y)

def arc(grid, cx, cy, r_outer, r_inner, ang_start_deg, ang_end_deg):
    for y in range(cy - r_outer - 1, cy + r_outer + 2):
        for x in range(cx - r_outer - 1, cx + r_outer + 2):
            dx, dy = x - cx, y - cy
            d2 = dx*dx + dy*dy
            if not (r_inner*r_inner < d2 <= r_outer*r_outer):
                continue
            ang = math.degrees(math.atan2(-dy, dx))  # screen-y is inverted
            if ang < 0: ang += 360
            if ang_start_deg <= ang <= ang_end_deg:
                setp(grid, x, y)

# ------------------- Settings: gear -------------------
def gear():
    g = empty()
    # Outer body
    filled_circle(g, CX, CY, 13)
    # 8 teeth: small rectangles sticking out at 0, 45, 90, ...
    for i in range(8):
        ang = math.radians(i * 45)
        for r in range(13, 16):
            x = round(CX + r * math.cos(ang))
            y = round(CY + r * math.sin(ang))
            # 3-pixel wide tooth (perpendicular)
            px = -math.sin(ang); py = math.cos(ang)
            for w in range(-1, 2):
                setp(g, round(x + w*px), round(y + w*py))
    # Hollow inner ring (carve out)
    for y in range(H):
        for x in range(W):
            d2 = (x - CX)**2 + (y - CY)**2
            if d2 <= 5*5:
                g[y][x] = 0
    return g

# ------------------- WiFi: 3 arcs + dot -------------------
def wifi():
    g = empty()
    cx, cy = 16, 26
    # Three arcs, opening upward (angles 30..150 in math coords; we use atan2)
    arc(g, cx, cy, r_outer=20, r_inner=17, ang_start_deg=30, ang_end_deg=150)
    arc(g, cx, cy, r_outer=14, r_inner=11, ang_start_deg=30, ang_end_deg=150)
    arc(g, cx, cy, r_outer=8,  r_inner=5,  ang_start_deg=30, ang_end_deg=150)
    # Dot
    filled_circle(g, cx, cy, 2)
    return g

# ------------------- Bluetooth: stylized B (rune) -------------------
def bluetooth():
    g = empty()
    # Stem vertical line at x=16, y=4..28
    for y in range(4, 29):
        setp(g, 16, y)
        setp(g, 17, y)
    # Upper triangle: (16,4) -> (22,12) -> (16,16)
    line(g, 16, 4, 22, 12)
    line(g, 17, 4, 23, 12)
    line(g, 22, 12, 16, 16)
    line(g, 23, 12, 17, 16)
    # Lower triangle: (16,16) -> (22,20) -> (16,28)
    line(g, 16, 16, 22, 20)
    line(g, 17, 16, 23, 20)
    line(g, 22, 20, 16, 28)
    line(g, 23, 20, 17, 28)
    # Left side: (16,4)-(10,12)-(16,16)-(10,20)-(16,28)
    line(g, 16, 4, 10, 12)
    line(g, 17, 4, 11, 12)
    line(g, 10, 12, 16, 16)
    line(g, 11, 12, 17, 16)
    line(g, 16, 16, 10, 20)
    line(g, 17, 16, 11, 20)
    line(g, 10, 20, 16, 28)
    line(g, 11, 20, 17, 28)
    return g

def to_bytes(grid):
    # Page-major, 4 pages of 32 bytes each.
    # Each byte = one column within one page (8 rows tall). LSB = top row.
    out = []
    for page in range(4):
        for col in range(W):
            b = 0
            for row in range(8):
                y = page * 8 + row
                if grid[y][col]:
                    b |= (1 << row)
            out.append(b)
    return out

def emit(name, grid):
    bs = to_bytes(grid)
    print(f"const uint8_t {name}[128] = {{")
    for page in range(4):
        line = ", ".join(f"0x{bs[page*32 + i]:02X}" for i in range(32))
        comma = "," if page < 3 else ""
        print(f"    // page {page}")
        print(f"    {line}{comma}")
    print("};")

# Optional: print ASCII preview to stderr
import sys
def preview(name, grid):
    print(f"-- {name} --", file=sys.stderr)
    for row in grid:
        print("".join("#" if c else "." for c in row), file=sys.stderr)
    print("", file=sys.stderr)

g_settings = gear()
g_wifi = wifi()
g_bt = bluetooth()
preview("settings", g_settings)
preview("wifi", g_wifi)
preview("bluetooth", g_bt)

print('#include "icons.h"')
print()
emit("icon_settings", g_settings)
print()
emit("icon_wifi", g_wifi)
print()
emit("icon_bluetooth", g_bt)