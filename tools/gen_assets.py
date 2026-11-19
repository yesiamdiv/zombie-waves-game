#!/usr/bin/env python3
"""Generate placeholder pixel-art tile textures for the game.

Art is defined as 2D pixel grids (list of strings over a color palette) plus a
set of procedural helpers for repetitive terrain. Outputs scaled-up PNGs into
assets/textures/tiles/ so artists can replace them 1:1 (same filenames) later.

Usage:  python3 tools/gen_assets.py [--scale N] [--verify]
"""

import argparse
import math
import os
import random
import re
import sys

from PIL import Image

TILE_BASE = 16
DEFAULT_SCALE = 4
OUT_DIR = os.path.join(os.path.dirname(__file__), "..", "assets", "textures", "tiles")
ENT_DIR = os.path.join(os.path.dirname(__file__), "..", "assets", "textures", "entities")
UI_DIR = os.path.join(os.path.dirname(__file__), "..", "assets", "textures", "ui")
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
NET_ART_C = os.path.join(ROOT, "src", "net", "net_art.c")
GATE_BASELINE = os.path.join(ROOT, "docs", "knowledge", "gate-baseline.md")


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


def make_entity(name, grid, palette, scale=DEFAULT_SCALE):
    img = render(grid, palette, scale)
    path = os.path.join(ENT_DIR, name + ".png")
    img.save(path)
    print("wrote", os.path.abspath(path), img.size)
    return path


# ---- the 16x16 logical grid (docs/knowledge/art-overhaul-plan.md) ----------
#
# Entity PNGs are 32x32 because net_art.c says `base_px = 32`, but NOTHING
# draws them at 32px: the player is CSprite.scale 0.5 -> 16x16 on screen
# (src/players.c), a zombie is size/16 with size = 10 + rand()%6 -> 20..30 px
# (src/world/waves.c), and the asset manager sets SDL_SCALEMODE_NEAREST. A
# 32 -> 16 nearest downscale throws away every odd-aligned row and column, so
# half of a 32px design never reaches the screen and a 1px motion becomes a
# coin flip. That is most of B50.
#
# So entity art is AUTHORED on a 16x16 grid and emitted at 2x into the same
# 32x32 PNG. Nothing in C changes. The player's on-screen downscale now
# reproduces the authored pixels exactly, and because every logical pixel is a
# 2x2 block it survives ANY downscale >= 0.5 - which also stabilises the
# zombie's non-integer 0.625..0.94 range.
#
# One logical pixel is therefore worth two source pixels, which is why the
# motion amplitudes below (1-2 logical px) read when the old ones (1-2 source
# px) did not.
ENT_BASE = 16
ENT_EMIT = 2


def upscale(grid, factor=ENT_EMIT):
    """Nearest-neighbour blow-up of a character grid. Pixel art, so no blur."""
    out = []
    for row in grid:
        wide = [ch for ch in row for _ in range(factor)]
        for _ in range(factor):
            out.append(list(wide))
    return out


def outline(grid, ch="K", over="."):
    """Put a 1px dark edge around every drawn pixel.

    A readable silhouette at gameplay size is A1's actual requirement, and at
    16 logical pixels a dark contour is what delivers it: the figures are
    colour-modulated at draw time (the player by its slot colour, a zombie by
    one of three theme tints), so internal tones shift with the tint while a
    near-black edge stays near-black whatever it is multiplied by."""
    h, w = len(grid), len(grid[0])
    edge = []
    for y in range(h):
        for x in range(w):
            if grid[y][x] != over:
                continue
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                nx, ny = x + dx, y + dy
                if 0 <= nx < w and 0 <= ny < h and grid[ny][nx] not in (over, ch):
                    edge.append((x, y))
                    break
    for x, y in edge:
        grid[y][x] = ch
    return grid


def emit_entity(name, grid16, palette):
    """Save a 16x16 logical grid as the 32x32 PNG the art table expects."""
    assert len(grid16) == ENT_BASE and len(grid16[0]) == ENT_BASE, \
        "%s: entity art is authored on a %dx%d grid" % (name, ENT_BASE, ENT_BASE)
    return make_entity(name, upscale(grid16), palette, 1)


# Three tones plus a near-black edge, and no more. Colour mod is a MULTIPLY
# (the player by COLOR_BLUE {0.30,0.40,1.00}, a zombie by a theme tint whose
# components run 0.20..0.80), so the art has to stay light and desaturated or
# it multiplies to mud - and a five-tone ramp would not survive the multiply
# with its steps still distinguishable. See docs/knowledge/art-overhaul-plan.md.
PLAYER_PAL = {
    ".": (0, 0, 0),
    "K": (26, 27, 32),       # edge
    "D": (134, 138, 150),    # shadow tone
    "M": (196, 200, 212),    # base tone
    "L": (244, 246, 252),    # highlight tone
    "V": (58, 64, 82),       # visor
    "G": (74, 78, 88),       # gun metal
}

ZOMBIE_PAL = {
    ".": (0, 0, 0),
    "K": (24, 24, 28),       # edge
    # Pushed brighter than they look on a contact sheet on purpose: a theme
    # tint multiplies every component by 0.20..0.80, so what reads as "a bit
    # washed out" here is what reads at all on the ground in game.
    "D": (160, 154, 144),    # shadow tone
    "M": (218, 215, 203),    # base tone
    "L": (252, 251, 242),    # highlight tone
    "E": (36, 30, 32),       # eye socket / open jaw
    "W": (116, 92, 92),      # wound
}


# ---- the player ------------------------------------------------------------

def _player_pose(body_dy=0, arm_l=0, arm_r=0,
                 leg_l=(0, 3), leg_r=(0, 3)):
    """One player pose on the 16x16 grid.

    The figure is drawn FRONT-FACING, towards the camera, because facing
    cannot come from rotation: render.c passes draw_rot = 0.0f for every
    entity, so there is exactly one facing and it has to be in the pixels.
    What makes it read as a front view is the ordering of the parts top to
    bottom - helmet crown, visor band, shoulders wider than the hips, two legs
    - plus the rifle carried on one side, which also gives the silhouette a
    handedness so it is never mistaken for a symmetric blob.

    THE VERTICAL BUDGET IS EXACTLY 16 AND IT IS TIGHT. The body occupies rows
    2..13 (head 3, shoulders 2, torso 4, legs 3); the bob moves it +-1; and
    the outline needs one row above and below. 3 + 2 + 4 + 3 + 2 + 2 = 16.
    Grow any part and a boot or a helmet loses its outline against the frame
    edge, which at gameplay size reads as a clipped sprite.

    Light comes from the upper left, consistently: left columns take the
    highlight tone, right columns the shadow tone. Adjacent body parts are
    separated by TONE rather than by a gap, because outline() only draws into
    transparent pixels - two touching parts in the same tone merge into one
    slab, which is precisely what made the old sprite a featureless egg."""
    g = canvas(ENT_BASE, ENT_BASE)
    dy = body_dy

    # helmet. The crown is the single most useful pixel group in the sprite:
    # at 16px it is what says "this end is the head".
    rect(g, 7, 2 + dy, 10, 2 + dy, "L")
    rect(g, 6, 3 + dy, 11, 3 + dy, "M")          # helmet rim, wider than the crown
    rect(g, 6, 3 + dy, 7, 3 + dy, "L")
    rect(g, 10, 3 + dy, 11, 3 + dy, "D")
    rect(g, 7, 4 + dy, 10, 4 + dy, "V")          # visor band

    # shoulders, wider than the hips: the second silhouette cue for a front view
    rect(g, 5, 5 + dy, 12, 6 + dy, "M")
    rect(g, 5, 5 + dy, 5, 6 + dy, "L")
    rect(g, 12, 5 + dy, 12, 6 + dy, "D")

    # torso, with a sling strap so the chest is not one flat slab of one tone
    rect(g, 7, 7 + dy, 10, 10 + dy, "M")
    rect(g, 7, 7 + dy, 7, 10 + dy, "L")
    rect(g, 10, 7 + dy, 10, 10 + dy, "D")
    rect(g, 8, 7 + dy, 8, 9 + dy, "D")           # strap

    # arms, swinging in opposition. In a front view a swing reads vertically:
    # the forward arm rides up, the trailing arm hangs lower. Each arm is a
    # shadow column plus a mid column, so it reads against the torso beside it.
    al = 6 + dy + arm_l
    rect(g, 5, al, 6, al + 3, "M")
    rect(g, 5, al, 5, al + 3, "D")
    rect(g, 5, al + 3, 6, al + 3, "L")           # glove
    ar = 6 + dy + arm_r
    rect(g, 11, ar, 12, ar + 3, "M")
    rect(g, 12, ar, 12, ar + 3, "D")
    rect(g, 11, ar + 3, 12, ar + 3, "L")         # glove

    # the rifle, carried on the left and travelling with the left arm. It is
    # the darkest tone on the sprite, which is what separates it from the arm
    # beside it - and it touches the glove, so it reads as HELD rather than as
    # a second object floating next to the figure.
    rect(g, 3, 5 + dy + arm_l, 4, 10 + dy + arm_l, "G")
    rect(g, 3, 5 + dy + arm_l, 3, 6 + dy + arm_l, "D")    # stock
    rect(g, 4, 10 + dy + arm_l, 4, 10 + dy + arm_l, "L")  # muzzle
    rect(g, 4, 9 + dy + arm_l, 4, 9 + dy + arm_l, "L")    # hand on the grip

    # THE LEGS ARE THE WALK. The old player had none at all, which is why its
    # cycle read as a wobbling egg: only the arms moved, by one screen pixel.
    # The two legs touch, so the right leg's shadow column is what separates
    # them when they are together.
    for (dx, length), x0, shade in ((leg_l, 7, "L"), (leg_r, 9, "D")):
        x = x0 + dx
        top = 11 + dy
        rect(g, x, top, x + 1, top + length - 1, "M")
        rect(g, x, top, x, top + length - 1, shade)
        rect(g, x, top + length - 1, x + 1, top + length - 1, "D")   # boot

    return outline(g, "K")


# One cycle is WALK_FRAMES frames and each frame is its OWN PNG, because a
# frame travels to a client as an art id and an art id is resolved from a
# texture PATH - every frame of a sprite sheet shares one path, so a sheet
# cannot name a frame at all (docs/knowledge/animation-transport.md).
#
# The frame COUNT is not ours to change: NET_ART_PLAYER_WALK_COUNT and
# NET_ART_ZOMBIE_WALK_COUNT are 8 in src/net/net_art.h and the per-frame ids
# are a contiguous appended block in the shared art table, so 8 is a protocol
# number, not a drawing choice. The frame RATE is not here either: the game
# plays the cycle at ANIM_FRAME_PERIOD = 0.1 s (10 fps, a 0.8 s stride), under
# the 15 fps ceiling the 20 Hz snapshot stream imposes.
WALK_FRAMES = 8

# The player stride, as a TABLE rather than a sine, for the two reasons the
# original gave and which still hold: a pure sine makes frame 0 and frame n/2
# the same pose, and a table is where a human can read the stride at a glance.
#
#    (body_dy, left arm, right arm, left leg (dx, len), right leg (dx, len))
#
# Phrasing of the cycle: frame 0 is the NEUTRAL STANDING pose (it has to be -
# the game holds frame 0 whenever the player is not moving, and player.png is
# the same picture, so a player who stops must not pop). Frames 1-3 are the
# left step, 4 is the opposite pass, 5-7 the right step. The body bobs twice
# per cycle - up on the lift, down on the plant - and the arms swing against
# the legs. Frame 4 is deliberately NOT a repeat of frame 0: the right leg is
# still lifted there, so all eight frames differ from one another.
PLAYER_STRIDE = [
    (0,   0,  0, (0, 3), (0, 3)),    # 0 neutral - the standing pose
    (-1,  1, -1, (-1, 2), (0, 3)),   # 1 left leg lifts, body rises
    (0,   2, -2, (-2, 3), (0, 3)),   # 2 left foot out, full stride
    (1,   1, -1, (-1, 3), (0, 2)),   # 3 weight plants, body sinks
    (0,   0,  0, (0, 3), (0, 2)),    # 4 pass - right leg still lifted
    (-1, -1,  1, (0, 3), (1, 2)),    # 5 right leg lifts, body rises
    (0,  -2,  2, (0, 3), (2, 3)),    # 6 right foot out, full stride
    (1,  -1,  1, (0, 2), (1, 3)),    # 7 weight plants, body sinks
]


def _player_walk_grid(i, n=WALK_FRAMES):
    """One frame of the player walk cycle. n frames make one full cycle."""
    body_dy, arm_l, arm_r, leg_l, leg_r = PLAYER_STRIDE[i % len(PLAYER_STRIDE)]
    return _player_pose(body_dy, arm_l, arm_r, leg_l, leg_r), PLAYER_PAL


def entity_player():
    """The standing player. Identical to walk frame 0, by construction."""
    return _player_walk_grid(0)


def entity_player_walk(n=WALK_FRAMES):
    """The player walk cycle: n frames, one PNG each."""
    paths = []
    for i in range(n):
        g, p = _player_walk_grid(i, n)
        paths.append(emit_entity("player_walk%d" % i, g, p))
    assert len(paths) == n, "player walk cycle must have exactly n frames"
    assert _player_walk_grid(0)[0] == entity_player()[0], \
        "player walk frame 0 must be the idle pose"
    poses = {tuple(tuple(r) for r in _player_walk_grid(i, n)[0]) for i in range(n)}
    assert len(poses) == n, "every player walk frame must be a distinct pose"
    return paths


# ---- the zombies -----------------------------------------------------------

# A horde used to read as one repeated blob for a reason that is visible in the
# old art: every zombie was the same symmetric disc, and the only variation was
# a tint and a random size. Three builds fix the silhouette half of that.
#
#   walker  the baseline shambler - head lolled to one side, one arm reaching
#   brute   squat and wide, heavy shoulders, short legs, both arms hanging
#   runner  lean and low, head thrust forward, long thin limbs, wide stride
#
# They are deliberately different WIDTHS and HEIGHTS inside the same 16x16
# frame, because at 20-30 drawn pixels the silhouette is the only thing that
# survives; colour does not distinguish them (the theme tint overrides it).
#
# NOTE FOR THE INTEGRATOR: only `walker` is wired. All zombies share
# zombie_walk0..7 because anim_walk_texture() maps frame -> art id with no
# per-entity variant input. The brute and runner art is a drop-in for the
# wiring described in docs/knowledge/art-overhaul-plan.md; until that lands it
# is unreferenced art, which is why it arrives in its own commit.
ZOMBIE_BUILDS = {
    # sh    hunched shoulder span (x0, x1) and its top row
    # head  (x0, width) and its top row - x0 is LEFT of centre: the loll
    # torso (x0, x1) and (y0, y1); the legs hang from y1 + 1
    # legs  (length, width); arms are placed relative to the torso
    # head_y is ABOVE sh_y on purpose: the skull crown has to break the
    # shoulder line or the head merges into the hunch and the creature reads
    # as a torso with a notch in it - which is exactly what the old one was.
    "walker": dict(sh=(5, 11), sh_y=3, head=(4, 4), head_y=2,
                   torso=(6, 10), torso_y=(5, 10), legs=(3, 2)),
    "brute":  dict(sh=(3, 13), sh_y=3, head=(5, 5), head_y=2,
                   torso=(5, 11), torso_y=(5, 10), legs=(3, 3)),
    "runner": dict(sh=(6, 11), sh_y=3, head=(4, 4), head_y=3,
                   torso=(7, 10), torso_y=(5, 9), legs=(4, 2)),
}


# The zombie cycle. Same phrasing as the player's, plus two things a human
# walk does not have:
#
#   the LIMP - `head_dy` and `drag` make the second half of the cycle unequal
#   to the first, so a zombie does not march; and
#   the REACH - `reach` extends the leading arm, PEAKING ON FRAME 6.
#
# Frame 6 is load-bearing: ANIM_ATTACK_FRAME in src/graphics/anim.h is 6, and
# the animation system HOLDS frame 6 while a zombie is attacking, so frame 6
# must be the furthest reach of the cycle or an attacking zombie looks like it
# is walking in place while hitting you (AN5). Do not reorder this table
# without changing that constant in the same commit; entity_zombie_walk()
# asserts the two agree.
#
#    (body_dy, head_dy, reach, drag, left leg (dx, dlen), right leg (dx, dlen))
ZOMBIE_STRIDE = [
    (0,  0, 0, 0, (0, 0), (0, 0)),    # 0 neutral - the spawn/idle pose
    (-1, 0, 1, 0, (-1, -1), (0, 0)),  # 1 left foot lifts
    (0,  1, 1, 1, (-2, 0), (0, 0)),   # 2 left foot out, head lolls
    (1,  1, 2, 1, (-1, 0), (0, -1)),  # 3 plant, the bad leg drags
    (0,  0, 2, 0, (0, 0), (0, -1)),   # 4 pass
    (-1, 0, 3, 0, (0, 0), (1, -1)),   # 5 right foot lifts
    (0,  1, 4, 1, (0, 0), (2, 0)),    # 6 FURTHEST REACH - ANIM_ATTACK_FRAME
    (1,  1, 2, 1, (0, -1), (1, 0)),   # 7 plant, head still lolled
]


def _zombie_pose(build, body_dy=0, head_dy=0, reach=0, drag=0,
                 leg_l=(0, 0), leg_r=(0, 0)):
    """One zombie pose on the 16x16 grid, for one of the three builds.

    ASYMMETRY IS THE WHOLE DESIGN. The old zombie was a symmetric disc with a
    dark notch in it, so a wave of them was one picture repeated: that is the
    "crowd reads as a blob" half of B50, and a tint and a random size do not
    fix it. Here the head sits off centre and sunk forward over the chest (the
    hunch and the loll), one arm reaches while the other dangles, and the two
    legs are different lengths, so the creature has a direction and a gait
    before it has moved at all.

    Same vertical budget as the player: the body lives in rows 2..13, the bob
    moves it +-1, the outline takes the row above and below."""
    b = ZOMBIE_BUILDS[build]
    g = canvas(ENT_BASE, ENT_BASE)
    dy = body_dy
    tx0, tx1 = b["torso"]
    ty0, ty1 = b["torso_y"]

    # hunched shoulders, drawn high and first
    sx0, sx1 = b["sh"]
    rect(g, sx0, b["sh_y"] + dy, sx1, b["sh_y"] + 1 + dy, "M")
    rect(g, sx0, b["sh_y"] + dy, sx0, b["sh_y"] + 1 + dy, "L")
    rect(g, sx1, b["sh_y"] + dy, sx1, b["sh_y"] + 1 + dy, "D")

    # torso, with a torn gash down one side - the one piece of internal
    # detail that still reads after the theme tint multiplies everything
    rect(g, tx0, ty0 + dy, tx1, ty1 + dy, "M")
    # the highlight is the TOP ROW, not the left column: a lit column beside
    # the reaching arm turns the creature into three vertical bars at
    # gameplay size, where a lit shelf under the shoulders just reads as a
    # chest catching the light.
    rect(g, tx0, ty0 + dy, tx1 - 1, ty0 + dy, "L")
    rect(g, tx1, ty0 + dy, tx1, ty1 + dy, "D")
    rect(g, tx1 - 1, ty0 + 2 + dy, tx1 - 1, ty1 - 1 + dy, "W")

    # the head: drawn LAST of the body parts so it sits over the chest rather
    # than on top of a neck. No neck is the point - it is sunk into the
    # shoulders and lolled to one side, and the loll changes with the cycle.
    hx0, hw = b["head"]
    hx = hx0 + (1 if head_dy else 0)
    hy = b["head_y"] + dy + head_dy
    rect(g, hx, hy, hx + hw - 1, hy, "L")                     # skull crown
    rect(g, hx, hy + 1, hx + hw - 1, hy + 2, "M")
    rect(g, hx, hy + 2, hx, hy + 2, "D")                      # cheek shadow
    rect(g, hx + 1, hy + 1, hx + 1, hy + 1, "E")              # eye socket
    rect(g, hx + hw - 2, hy + 2, hx + hw - 1, hy + 2, "E")    # slack jaw

    # the reaching arm, on the viewer's left, extending with `reach`
    ay = ty0 + dy
    alen = 3 + min(reach, 4)
    rect(g, tx0 - 4, ay, tx0 - 3, ay + alen, "M")
    rect(g, tx0 - 4, ay, tx0 - 4, ay + alen, "D")
    rect(g, tx0 - 4, ay + alen, tx0 - 3, ay + alen, "L")      # clawed hand

    # the dangling arm, on the viewer's right, which only sways
    by = ty0 + 1 + dy + drag
    rect(g, tx1 + 1, by, tx1 + 2, by + 4, "M")
    rect(g, tx1 + 2, by, tx1 + 2, by + 4, "D")
    rect(g, tx1 + 1, by + 4, tx1 + 2, by + 4, "D")

    # the legs. The right one is a pixel longer than the left whatever the
    # stride table says, so the creature limps even standing still; the two
    # touch when together, so the right leg's shadow column separates them.
    base, lw = b["legs"]
    for (dx, dlen), x0, longer, shade in ((leg_l, tx0, 0, "L"),
                                          (leg_r, tx1 - lw + 1, 1, "D")):
        length = max(1, base + dlen + longer)
        x = x0 + dx
        top = ty1 + 1 + dy
        rect(g, x, top, x + lw - 1, top + length - 1, "M")
        rect(g, x, top, x, top + length - 1, shade)
        rect(g, x, top + length - 1, x + lw - 1, top + length - 1, "D")

    return outline(g, "K")


def _zombie_walk_grid(i, build="walker", n=WALK_FRAMES):
    """One frame of a zombie walk cycle. Frame 0 is the idle/spawn pose."""
    body_dy, head_dy, reach, drag, leg_l, leg_r = \
        ZOMBIE_STRIDE[i % len(ZOMBIE_STRIDE)]
    return (_zombie_pose(build, body_dy, head_dy, reach, drag, leg_l, leg_r),
            ZOMBIE_PAL)


def entity_zombie(build="walker"):
    """The standing zombie. Identical to walk frame 0, by construction.

    waves.c spawns a zombie on walk frame 0 and falls back to zombie.png only
    when the cycle is missing, so the two must be the same picture or the
    fallback would be a different creature."""
    return _zombie_walk_grid(0, build)


def entity_zombie_walk(build="walker", n=WALK_FRAMES):
    """A zombie walk cycle: n frames, one PNG each."""
    prefix = "zombie" if build == "walker" else "zombie_%s" % build
    paths = []
    for i in range(n):
        g, p = _zombie_walk_grid(i, build, n)
        paths.append(emit_entity("%s_walk%d" % (prefix, i), g, p))
    assert len(paths) == n, "walk cycle must have exactly n frames"
    assert _zombie_walk_grid(0, build)[0] == entity_zombie(build)[0], \
        "zombie walk frame 0 must be the idle pose"
    reach = [ZOMBIE_STRIDE[i][2] for i in range(n)]
    assert reach.index(max(reach)) == 6, \
        "frame 6 must be the furthest reach: ANIM_ATTACK_FRAME in " \
        "src/graphics/anim.h holds frame 6 for the attack pose (AN5)"
    return paths


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


def blit(grid, cells, fill, outline="K"):
    """Fill `cells` with `fill`, ringed by a 1px `outline`; leave the rest
    untouched (transparent)."""
    s = set(cells)
    for (x, y) in s:
        grid[y][x] = fill
    for (x, y) in s:
        for dx in (-1, 0, 1):
            for dy in (-1, 0, 1):
                nx, ny = x + dx, y + dy
                if 0 <= ny < len(grid) and 0 <= nx < len(grid[0]) \
                        and (nx, ny) not in s:
                    grid[ny][nx] = outline


def entity_pickup(cross=False, stripes=False, bolt=False):
    """Pickup icons on a TRANSPARENT background (B43).

    The old art filled most of the sprite with an opaque near-white card,
    which read as a white box lying on the ground. The icon is now drawn
    directly, larger, on nothing but transparent pixels, with its own dark
    outline so it stays legible over any terrain."""
    g = canvas(32, 32)
    if cross:  # medkit: a bold red plus
        cells = {(x, y) for y in range(3, 29) for x in range(11, 21)}
        cells |= {(x, y) for y in range(11, 21) for x in range(3, 29)}
        blit(g, cells, "R")
    if stripes:  # ammo: three cartridges
        cells = set()
        for x0 in (6, 14, 22):
            cells |= {(x, y) for y in range(8, 28) for x in range(x0, x0 + 4)}
            cells |= {(x, y) for y in range(6, 8) for x in range(x0 + 1, x0 + 3)}
        blit(g, cells, "Y")
    if bolt:  # speed: a lightning bolt
        spans = [
            (4, 18, 24), (5, 17, 24), (6, 16, 24), (7, 15, 24), (8, 14, 24),
            (9, 13, 24), (10, 12, 24), (11, 11, 16), (12, 10, 16),
            (13, 9, 16), (14, 9, 16), (15, 10, 22), (16, 11, 23),
            (17, 12, 24), (18, 13, 24), (19, 14, 24), (20, 15, 24),
            (21, 16, 24), (22, 17, 24), (23, 18, 24), (24, 19, 24),
            (25, 20, 24), (26, 21, 24), (27, 22, 24),
        ]
        cells = {(x, y) for y, x0, x1 in spans for x in range(x0, x1 + 1)}
        blit(g, cells, "B")
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


def ui_vignette(size=256, max_alpha=170, color=(220, 30, 30)):
    """Damage vignette (B45): a square red radial-alpha texture, alpha 0 in the
    centre rising from ~40% of the radius outward to a maximum at the edges and
    corners (smoothstep falloff). The HUD draws it stretched to the screen and
    fades it on the damage timer; this replaces the banded real-time fill from
    B41/e637a5d. It is UI-only art (not in the net art table), so
    NET_ASSET_VERSION is untouched, but it is a new texture, so the gate-baseline
    count moves 33 -> 34 in the same commit (`knowledge/ui-damage-vignette`)."""
    img = Image.new("RGBA", (size, size))
    px = img.load()
    c = size / 2.0
    r0 = 0.40 * c
    r1 = c * (2.0 ** 0.5)   # distance from centre to a corner
    for y in range(size):
        for x in range(size):
            r = ((x - c) ** 2 + (y - c) ** 2) ** 0.5
            t = (r - r0) / (r1 - r0)
            t = 0.0 if t < 0.0 else (1.0 if t > 1.0 else t)
            a = int(round(max_alpha * (t * t * (3.0 - 2.0 * t))))  # smoothstep
            px[x, y] = (color[0], color[1], color[2], a)
    return img


def _asset_fail(msg):
    print("ASSET VERIFY FAILED:", msg, file=sys.stderr)
    sys.exit(1)


def verify_entity_art(paths):
    """Every entity PNG must be RGBA with fully transparent corners.

    An opaque corner is exactly the B2/B43 bug: the transparent background was
    baked in as an opaque card, so the sprite drew as a box on the ground. The
    render path already blends correctly, so this is the only place the class
    can hide."""
    ent_dir = os.path.abspath(ENT_DIR)
    entities = [p for p in paths
                if os.path.dirname(os.path.abspath(p)) == ent_dir]
    if not entities:
        _asset_fail(f"no entity textures were generated into {ENT_DIR}")
    for p in entities:
        name = os.path.basename(p)
        with Image.open(p) as im:
            im.load()
            if im.mode != "RGBA":
                _asset_fail(f"{name} is {im.mode}, expected RGBA")
            w, h = im.size
            for (cx, cy) in ((0, 0), (w - 1, 0), (0, h - 1), (w - 1, h - 1)):
                if im.getpixel((cx, cy))[3] != 0:
                    _asset_fail(
                        f"{name} has a non-transparent corner "
                        f"({cx},{cy})={im.getpixel((cx, cy))}; entity art must "
                        f"have a transparent background (B2/B43)")


def verify_entity_sizes_vs_net_art(paths):
    """net_art's `base_px` must equal the real PNG width.

    base_px converts a replicated diameter into a draw scale, so a mismatch
    makes the client draw the wrong size (B43). Parse the C table instead of
    duplicating its numbers here, so the two cannot drift apart."""
    with open(NET_ART_C, encoding="utf-8") as f:
        text = f.read()
    rows = re.findall(r'"([^"]+\.png)"\s*,\s*(\d+)\s*\}', text)
    if not rows:
        _asset_fail(f"could not parse any art rows from {NET_ART_C}")
    by_name = {os.path.basename(p): p for p in paths}
    for rel, base_px in rows:
        name = os.path.basename(rel)
        if name not in by_name:
            _asset_fail(f"net_art references {rel}, which this generator "
                        f"does not emit")
        with Image.open(by_name[name]) as im:
            if im.width != int(base_px):
                _asset_fail(
                    f"{rel}: PNG width {im.width} != net_art base_px {base_px} "
                    f"in src/net/net_art.c (they must match)")


def declared_texture_count():
    """Read the CURRENT texture count from gate-baseline.md.

    The file is a chronological log, so the last `N textures` declaration in
    the tree is the current one. This stays lane-local: animation's lane
    declares its own number and the merged tree declares the combined one, so
    never hard-code a count here."""
    last = None
    with open(GATE_BASELINE, encoding="utf-8") as f:
        for line_no, line in enumerate(f, 1):
            for m in re.finditer(r"(\d+)\s+textures\b", line):
                last = (int(m.group(1)), line_no)
    if last is None:
        _asset_fail(f"no 'N textures' declaration found in {GATE_BASELINE}")
    return last


def verify_texture_count(paths):
    declared, line_no = declared_texture_count()
    if len(paths) != declared:
        _asset_fail(
            f"generator emits {len(paths)} textures but {GATE_BASELINE} "
            f"(line {line_no}) declares {declared}. Textures change only via a "
            f"deliberate re-baseline: update gate-baseline.md in the same commit.")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--scale", type=int, default=DEFAULT_SCALE)
    ap.add_argument("--verify", action="store_true",
                    help="re-open generated files to confirm they parse")
    args = ap.parse_args()

    os.makedirs(OUT_DIR, exist_ok=True)
    os.makedirs(ENT_DIR, exist_ok=True)
    os.makedirs(UI_DIR, exist_ok=True)

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
    # The player and the zombie are authored on the 16x16 logical grid and go
    # out through emit_entity, which blows them up 2x to the 32x32 PNG the art
    # table expects. The props (bullet, pickups, blade) are still authored at
    # their final pixel size, so they go straight to make_entity. --verify
    # catches a mix-up either way: it compares every PNG's real width against
    # net_art's base_px.
    for name, fn in entity_specs:
        g, p = fn()
        if len(g) == ENT_BASE and len(g[0]) == ENT_BASE:
            paths.append(emit_entity(name, g, p))
        else:
            paths.append(make_entity(name, g, p, 1))

    # The walk cycle is part of the entity set: its frames are textures like
    # any other, and --verify has to open them like any other.
    paths.extend(entity_zombie_walk())
    paths.extend(entity_player_walk())

    # ---- ui (screen overlays) ----
    vignette_path = os.path.join(UI_DIR, "vignette.png")
    ui_vignette().save(vignette_path)
    print("wrote", os.path.abspath(vignette_path), (256, 256))
    paths.append(vignette_path)

    assert len(paths) == len(set(paths)), "duplicate texture path"

    if args.verify:
        for p in paths:
            with Image.open(p) as im:
                im.verify()
        verify_entity_art(paths)
        verify_entity_sizes_vs_net_art(paths)
        verify_texture_count(paths)
        print(f"verified {len(paths)} textures")


if __name__ == "__main__":
    main()