#!/usr/bin/env python3
"""Generate placeholder pixel-art tile textures for the game.

Art is defined as 2D pixel grids (list of strings over a color palette) plus a
set of procedural helpers for repetitive terrain. Outputs scaled-up PNGs into
assets/textures/tiles/ so artists can replace them 1:1 (same filenames) later.

Usage:  python3 tools/gen_assets.py [--scale N] [--verify]
"""

import argparse
import os
import random
import sys

from PIL import Image

TILE_BASE = 16
DEFAULT_SCALE = 4
OUT_DIR = os.path.join(os.path.dirname(__file__), "..", "assets", "textures", "tiles")
ENT_DIR = os.path.join(os.path.dirname(__file__), "..", "assets", "textures", "entities")


def render(grid, palette, scale):
    h = len(grid)
    w = len(grid[0])
    img = Image.new("RGBA", (w, h))
    for y, row in enumerate(grid):
        for x, ch in enumerate(row):
            if ch == ".":
                img.putpixel((x, y), (0, 0, 0, 0))
            else:
                img.putpixel((x, y), (*palette[ch], 255))
    return img.resize((w * scale, h * scale), Image.NEAREST)


def ground(base, speck=(45, 90, 40), light=(230, 235, 190), seed=0, dark_prob=0.18,
           light_prob=0.10, base2=None):
    """Terrain fill: base color + scattered dark/light speckles (deterministic)."""
    rng = random.Random(seed)
    base2 = base2 or base
    grid = []
    for y in range(TILE_BASE):
        row = []
        for x in range(TILE_BASE):
            roll = rng.random()
            if roll < dark_prob:
                row.append("d")
            elif roll < dark_prob + light_prob:
                row.append("l")
            else:
                row.append("b")
        grid.append(row)
    return grid, {"b": base if base == base2 else base2, "d": speck, "l": light}


def stone_wall(colors=(120, 118, 112)):
    """Brick-ish wall block. `colors` is the base RGB; dark/bright variants are
    derived so the wall reads as one material instead of flat channels."""
    base = colors
    dark = tuple(int(c * 0.78) for c in base)
    bright = tuple(min(255, int(c * 1.12)) for c in base)
    grid = []
    for y in range(TILE_BASE):
        row = []
        for x in range(TILE_BASE):
            ch = "b"
            if (y % 5) == 4:
                ch = "D"
            elif (y % 5) == 0 and (x % 8) == 7:
                ch = "D"
            elif ((x + y) % 7) == 0:
                ch = "H"
            row.append(ch)
        grid.append(row)
    return grid, {"b": base, "D": dark, "H": bright}


def water(base=(58, 110, 170), light=(110, 160, 210), dark=(40, 80, 130)):
    grid = []
    for y in range(TILE_BASE):
        row = []
        for x in range(TILE_BASE):
            ch = "b"
            if y in (3, 11) and x % 4 < 3:
                ch = "l"
            elif y in (7, 8) and (x % 6) == 0:
                ch = "d"
            elif (x * 7 + y * 3) % 11 == 0:
                ch = "d"
            row.append(ch)
        grid.append(row)
    return grid, {"b": base, "l": light, "d": dark}


def asphalt(base=(64, 62, 58), dark=(42, 41, 38), light=(86, 84, 78)):
    grid = []
    for y in range(TILE_BASE):
        row = []
        for x in range(TILE_BASE):
            ch = "b"
            r = (x * 13 + y * 7) % 17
            if r == 0:
                ch = "l"
            elif r == 5:
                ch = "d"
            row.append(ch)
        grid.append(row)
    return grid, {"b": base, "l": light, "d": dark}


def city_road(base=(52, 50, 47), dark=(36, 35, 33), light=(78, 75, 70), lane=(212, 190, 60)):
    grid, pal = asphalt(base, dark, light)
    pal["y"] = lane
    for y in (7, 8):
        for x in range(2, TILE_BASE, 6):
            grid[y][x] = "y"
    return grid, pal


def snow_ground(base=(232, 238, 246), speck=(198, 210, 226), light=(250, 252, 254),
                seed=0, dark_prob=0.16, light_prob=0.12):
    return ground(base, speck=(speck), light=light, seed=seed,
                  dark_prob=dark_prob, light_prob=light_prob)


def sand_ground(base=(196, 172, 118), speck=(158, 132, 86), light=(222, 202, 150),
                seed=0, dark_prob=0.18, light_prob=0.12):
    return ground(base, speck=(speck), light=light, seed=seed,
                  dark_prob=dark_prob, light_prob=light_prob)


def make(name, grid, palette, scale):
    img = render(grid, palette, scale)
    path = os.path.join(OUT_DIR, name + ".png")
    img.save(path)
    print("wrote", os.path.abspath(path), img.size)
    return path


# ---- entity pixel art ----
# All entity art is IRGB filled with light base tones + dark outlines so the
# game can tint it at runtime (color mod) without washing out the silhouette.

def canvas(w, h, ch="."):
    return [[ch for _ in range(w)] for _ in range(h)]


def disc(grid, cx, cy, rx, ry, ch):
    for y in range(len(grid)):
        for x in range(len(grid[0])):
            if ((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2 <= 1.0:
                grid[y][x] = ch


def frame(grid, ch):
    h, w = len(grid), len(grid[0])
    for x in range(w):
        grid[0][x] = grid[h - 1][x] = ch
    for y in range(h):
        grid[y][0] = grid[y][w - 1] = ch


def rect(grid, x0, y0, x1, y1, ch):
    for y in range(max(0, y0), min(len(grid), y1 + 1)):
        for x in range(max(0, x0), min(len(grid[0]), x1 + 1)):
            grid[y][x] = ch


def body_any(w, h, seed, outline="K", base="w"):
    g = canvas(w, h)
    rng = random.Random(seed)
    disc(g, w // 2, h // 2, w * 0.34, h * 0.42, base)
    for _ in range(seed):
        x = w // 2 + rng.randint(-int(w * 0.3), int(w * 0.3))
        y = h // 2 + rng.randint(-int(h * 0.3), int(h * 0.3))
        if g[y][x] == base:
            g[y][x] = outline if rng.random() < 0.5 else "s"
    return g


def make_entity(name, grid, palette, scale=DEFAULT_SCALE):
    img = render(grid, palette, scale)
    path = os.path.join(ENT_DIR, name + ".png")
    img.save(path)
    print("wrote", os.path.abspath(path), img.size)
    return path


def entity_player():
    g = body_any(32, 32, 7)
    # head + shoulders read at small scale
    rect(g, 11, 5, 20, 11, "w")     # head
    rect(g, 13, 7, 18, 9, "K")      # visor
    rect(g, 9, 12, 22, 15, "w")     # shoulders
    rect(g, 10, 18, 14, 27, "w")    # left arm
    rect(g, 17, 18, 21, 27, "w")    # right arm
    rect(g, 3, 12, 8, 24, "w")      # held rifle
    rect(g, 4, 13, 7, 23, "K")      # rifle barrel
    return g, {".": (0, 0, 0), "K": (34, 34, 38), "w": (232, 232, 238),
               "s": (150, 152, 160)}


def entity_zombie():
    g = body_any(32, 32, 11)
    disc(g, 16, 12, 5, 6, "w")     # head, slumped
    disc(g, 14, 12, 2, 2, "K")     # eye hole
    rect(g, 9, 6, 20, 8, "w")      # arms reaching up
    rect(g, 12, 9, 17, 10, "K")    # open mouth
    rect(g, 23, 14, 25, 27, "w")   # right arm
    return g, {".": (0, 0, 0), "K": (30, 30, 34), "w": (215, 210, 205),
               "s": (140, 140, 150)}


def entity_bullet():
    g = canvas(8, 8)
    disc(g, 4, 4, 3, 3, "w")
    disc(g, 4, 4, 2, 2, "w")
    return g, {".": (0, 0, 0), "w": (245, 242, 220)}


def entity_grenade():
    g = canvas(32, 32)
    disc(g, 16, 18, 10, 10, "g")
    disc(g, 13, 15, 4, 4, "h")
    rect(g, 15, 6, 16, 9, "K")     # pin
    return g, {".": (0, 0, 0), "K": (60, 54, 48), "g": (66, 96, 56),
               "h": (96, 134, 82)}


def entity_rocket():
    g = canvas(32, 32)
    disc(g, 16, 16, 6, 9, "o")
    disc(g, 14, 10, 3, 3, "h")
    rect(g, 8, 10, 10, 22, "r")    # left fin
    rect(g, 22, 10, 24, 22, "r")   # right fin
    disc(g, 16, 22, 5, 3, "r")
    return g, {".": (0, 0, 0), "o": (228, 124, 44), "h": (246, 184, 96),
               "r": (150, 74, 32)}


def entity_pickup(cross=False, stripes=False, bolt=False):
    g = canvas(32, 32)
    rect(g, 4, 8, 27, 27, "w")
    rect(g, 7, 10, 24, 25, "w")
    frame(g, "K")
    if cross:
        rect(g, 13, 12, 18, 23, "R")
        rect(g, 10, 15, 21, 20, "R")
    if stripes:  # diagonal ammo bands
        for i in range(8):
            rect(g, 6 + i, 27 - i - 3, 8 + i, 29 - i - 3, "Y")
    if bolt:     # lightning
        pts = [(16, 10), (11, 19), (14, 19), (12, 26), (19, 16), (16, 16)]
        for ch, (x, y) in zip("B" * 6, pts):
            g[y][x] = "B"
        rect(g, 12, 20, 20, 21, "B")
    return g, {".": (0, 0, 0), "K": (40, 40, 44), "w": (238, 238, 242),
               "R": (214, 68, 62), "Y": (214, 178, 60), "B": (88, 140, 224)}


def entity_blade():
    g = canvas(40, 16)
    rect(g, 0, 6, 6, 9, "b")       # handle
    rect(g, 7, 4, 8, 11, "K")      # crossguard
    rect(g, 9, 7, 39, 8, "S")      # blade steel
    rect(g, 12, 5, 37, 6, "H")     # shine
    return g, {".": (0, 0, 0), "b": (120, 78, 44), "K": (60, 58, 54),
               "S": (176, 194, 206), "H": (226, 236, 244)}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--scale", type=int, default=DEFAULT_SCALE)
    ap.add_argument("--verify", action="store_true",
                    help="re-open generated files to confirm they parse")
    args = ap.parse_args()

    os.makedirs(OUT_DIR, exist_ok=True)
    os.makedirs(ENT_DIR, exist_ok=True)

    specs = []
    # ---- grassland ----
    for i, seed in enumerate((1, 2, 3)):
        g, p = ground((74, 118, 54), speck=(56, 92, 42), light=(98, 146, 66), seed=seed)
        specs.append((f"grass_ground{i}", g, p))
    g, p = stone_wall((126, 124, 118))
    specs.append(("grass_wall", g, p))
    g, p = water((58, 108, 168))
    specs.append(("grass_water", g, p))
    g, p = city_road()
    specs.append(("grass_road", g, p))

    # ---- desert ----
    for i, seed in enumerate((11, 12, 13)):
        g, p = sand_ground(seed=seed)
        specs.append((f"desert_ground{i}", g, p))
    g, p = stone_wall((172, 132, 88))
    specs.append(("desert_wall", g, p))
    g, p = water((64, 118, 128))
    specs.append(("desert_water", g, p))
    g, p = asphalt((172, 148, 104))
    specs.append(("desert_road", g, p))

    # ---- snow ----
    for i, seed in enumerate((21, 22, 23)):
        g, p = snow_ground(seed=seed)
        specs.append((f"snow_ground{i}", g, p))
    g, p = stone_wall((168, 184, 202))
    specs.append(("snow_wall", g, p))
    g, p = water((96, 128, 170), light=(150, 178, 214), dark=(74, 98, 136))
    specs.append(("snow_water", g, p))
    g, p = asphalt((150, 158, 168), dark=(128, 136, 146), light=(178, 186, 196))
    specs.append(("snow_road", g, p))

    # ---- city ----
    for i, seed in enumerate((31, 32, 33)):
        g, p = ground((104, 102, 98), speck=(82, 80, 76), light=(128, 125, 120), seed=seed)
        specs.append((f"city_ground{i}", g, p))
    g, p = stone_wall((142, 78, 58))
    specs.append(("city_wall", g, p))
    g, p = water((60, 66, 74), light=(92, 100, 110), dark=(44, 48, 54))
    specs.append(("city_water", g, p))
    g, p = city_road()
    specs.append(("city_road", g, p))

    paths = [make(name, g, p, args.scale) for name, g, p in specs]

    entity_specs = [
        ("player", entity_player),
        ("zombie", entity_zombie),
        ("bullet", entity_bullet),
        ("grenade", entity_grenade),
        ("rocket", entity_rocket),
        ("medkit", lambda: entity_pickup(cross=True)),
        ("ammo", lambda: entity_pickup(stripes=True)),
        ("speed", lambda: entity_pickup(bolt=True)),
        ("sword_blade", entity_blade),
    ]
    for name, fn in entity_specs:
        g, p = fn()
        paths.append(make_entity(name, g, p, 1))

    if args.verify:
        for p in paths:
            with Image.open(p) as im:
                im.verify()
        print(f"verified {len(paths)} textures")


if __name__ == "__main__":
    main()