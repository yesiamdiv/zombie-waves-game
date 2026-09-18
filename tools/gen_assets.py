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


def render(grid, palette, scale):
    h = len(grid)
    w = len(grid[0])
    img = Image.new("RGB", (w, h))
    for y, row in enumerate(grid):
        for x, ch in enumerate(row):
            img.putpixel((x, y), palette[ch])
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
    """Brick-ish wall block."""
    base, dark, bright = colors[0], colors[1], colors[2]
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


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--scale", type=int, default=DEFAULT_SCALE)
    ap.add_argument("--verify", action="store_true",
                    help="re-open generated files to confirm they parse")
    args = ap.parse_args()

    os.makedirs(OUT_DIR, exist_ok=True)

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

    if args.verify:
        for p in paths:
            with Image.open(p) as im:
                im.verify()
        print(f"verified {len(paths)} textures")


if __name__ == "__main__":
    main()