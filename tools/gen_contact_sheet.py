#!/usr/bin/env python3
"""Render contact sheets of the entity art so a HUMAN can actually review it.

Why this exists: `gen_assets.py --verify` proves the asset pipeline is intact
(RGBA, transparent corners, widths matching net_art's base_px, declared count).
It proves nothing about whether the art looks good or whether a walk cycle
reads as walking, and the game's own gate is headless, so it is structurally
blind to art (docs/SYSTEM_DESIGN_RULES.md section 4). Every visual claim in this
repo is `unproven` until a human looks at it - this tool is what a human looks
at when the game cannot be run.

Three kinds of sheet:

  filmstrip   One walk cycle left to right, hugely upscaled with NEAREST, so
              individual pixels and the pose change between frames are visible.
  compare     The same strip for the current tree and for another git revision,
              stacked, so a redesign can be judged against what it replaced.
  crowd       One player plus a horde at TRUE GAMEPLAY SCALE on a ground tile,
              with the real per-theme tints and the real random sizes, because
              silhouette readability is a question about the drawn size and
              nothing else. See docs/knowledge/art-overhaul-plan.md: the player
              is drawn at 16x16 and a zombie at 20x20..30x30, NOT at 32x32.

Usage:
    python tools/gen_contact_sheet.py [--out DIR] [--compare REV] [--scale N]

Output goes to build/logs/art/ by default - never /tmp (docs/knowledge/
tmp-scratch-rule.md).
"""

import argparse
import io
import os
import random
import subprocess
import sys

from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
ENT = os.path.join(ROOT, "assets", "textures", "entities")
TILES = os.path.join(ROOT, "assets", "textures", "tiles")
DEFAULT_OUT = os.path.join(ROOT, "build", "logs", "art")

BG = (26, 28, 32, 255)
BG_ALT = (36, 39, 45, 255)
INK = (236, 238, 244, 255)
DIM = (150, 156, 168, 255)

# The real per-theme zombie tints, copied from src/world/theme.c so a sheet
# shows what the game shows. If theme.c changes these, this list is stale - it
# is a review aid, not a contract.
ZOMBIE_TINTS = {
    "grass": [(0.30, 0.60, 0.20), (0.50, 0.40, 0.20), (0.40, 0.20, 0.30)],
    "desert": [(0.72, 0.55, 0.30), (0.62, 0.42, 0.24), (0.50, 0.32, 0.35)],
    "snow": [(0.60, 0.68, 0.72), (0.50, 0.60, 0.55), (0.68, 0.70, 0.80)],
    "city": [(0.45, 0.50, 0.40), (0.55, 0.40, 0.30), (0.40, 0.35, 0.50)],
}
# COLOR_BLUE from src/graphics/sprite.c - the single-player slot colour.
PLAYER_TINT = (0.30, 0.40, 1.00)


def font(size):
    try:
        return ImageFont.load_default(size=size)
    except TypeError:          # very old Pillow: fixed-size bitmap font
        return ImageFont.load_default()


def label(img, xy, text, size=16, fill=INK):
    ImageDraw.Draw(img).text(xy, text, font=font(size), fill=fill)


def tint(img, rgb):
    """Reproduce SDL's colour mod: a per-channel multiply, alpha untouched."""
    img = img.convert("RGBA")
    r, g, b, a = img.split()
    r = r.point(lambda v: int(v * rgb[0]))
    g = g.point(lambda v: int(v * rgb[1]))
    b = b.point(lambda v: int(v * rgb[2]))
    return Image.merge("RGBA", (r, g, b, a))


def load(name, rev=None):
    """An entity PNG from the working tree, or from a git revision."""
    rel = "assets/textures/entities/%s.png" % name
    if rev is None:
        p = os.path.join(ENT, name + ".png")
        if not os.path.exists(p):
            return None
        return Image.open(p).convert("RGBA")
    out = subprocess.run(["git", "-C", ROOT, "show", "%s:%s" % (rev, rel)],
                         capture_output=True)
    if out.returncode != 0 or not out.stdout:
        return None
    return Image.open(io.BytesIO(out.stdout)).convert("RGBA")


def checker(size, a=(52, 54, 60, 255), b=(44, 46, 52, 255), cell=8):
    """A checkerboard, so transparent pixels are visibly transparent."""
    img = Image.new("RGBA", (size, size), a)
    d = ImageDraw.Draw(img)
    for y in range(0, size, cell):
        for x in range(0, size, cell):
            if ((x // cell) + (y // cell)) % 2:
                d.rectangle([x, y, x + cell - 1, y + cell - 1], fill=b)
    return img


def cell_of(frame, px, rgb=None, on_checker=True):
    """One frame blown up to px*px with NEAREST, over a checkerboard."""
    if rgb is not None:
        frame = tint(frame, rgb)
    big = frame.resize((px, px), Image.NEAREST)
    if not on_checker:
        base = Image.new("RGBA", (px, px), BG_ALT)
    else:
        base = checker(px)
    base.alpha_composite(big)
    return base


def strip(rows, px, title, notes=()):
    """`rows` is [(row label, [frames], tint-or-None), ...]."""
    pad, gap, head, lab_w = 18, 8, 54, 190
    ncols = max(len(f) for _, f, _ in rows)
    w = pad * 2 + lab_w + ncols * px + (ncols - 1) * gap
    note_h = 22 * len(notes) + (10 if notes else 0)
    h = pad * 2 + head + len(rows) * (px + gap) + note_h
    sheet = Image.new("RGBA", (w, h), BG)

    label(sheet, (pad, pad), title, 22)
    y = pad + head - 24
    for i in range(ncols):
        x = pad + lab_w + i * (px + gap)
        label(sheet, (x + 2, y), "frame %d" % i, 14, DIM)

    y = pad + head
    for name, frames, rgb in rows:
        label(sheet, (pad, y + px // 2 - 8), name, 15)
        for i, fr in enumerate(frames):
            x = pad + lab_w + i * (px + gap)
            sheet.alpha_composite(cell_of(fr, px, rgb), (x, y))
        y += px + gap

    y += 10
    for n in notes:
        label(sheet, (pad, y), n, 14, DIM)
        y += 22
    return sheet


def save(img, out_dir, name):
    os.makedirs(out_dir, exist_ok=True)
    p = os.path.join(out_dir, name)
    img.convert("RGB").save(p)
    print("wrote", os.path.abspath(p), img.size)
    return p


def cycle(prefix, rev=None, n=8):
    frames = [load("%s%d" % (prefix, i), rev) for i in range(n)]
    return [f for f in frames if f is not None]


def variants_present():
    """Which zombie variants this tree actually emits."""
    out = []
    for v in ("zombie", "zombie_brute", "zombie_runner"):
        if cycle(v + "_walk"):
            out.append(v)
    return out


def ground(size):
    """A real ground tile, tiled, as the crowd-shot backdrop."""
    p = os.path.join(TILES, "city_ground0.png")
    if not os.path.exists(p):
        return Image.new("RGBA", (size, size), (60, 62, 58, 255))
    t = Image.open(p).convert("RGBA")
    bg = Image.new("RGBA", (size, size))
    for y in range(0, size, t.height):
        for x in range(0, size, t.width):
            bg.alpha_composite(t, (x, y))
    return bg


def crowd(out_dir, theme="city", zoom=1, seed=42):
    """Player + horde at TRUE gameplay scale, then the whole shot upscaled.

    Sizes and placements come from the real draw path: the player sprite is
    CSprite.scale 0.5 on 32px art -> 16x16, and a zombie is size/16 with
    size = 10 + rand()%6 -> 20..30 px. Zombies also start on a hashed walk
    frame (P1a stagger in waves.c), which is reproduced here because a horde
    all on frame 0 is exactly the "one repeated blob" the owner complained
    about and would make this sheet lie.
    """
    rng = random.Random(seed)
    W = H = 320
    scene = ground(W)

    zvars = variants_present()
    strips = {v: cycle(v + "_walk") or [load(v)] for v in zvars}
    tints = ZOMBIE_TINTS.get(theme, ZOMBIE_TINTS["city"])

    spots = [(44, 60), (108, 44), (178, 58), (246, 48), (26, 136),
             (96, 128), (170, 140), (250, 132), (60, 216), (150, 224),
             (238, 212)]
    for idx, (x, y) in enumerate(spots):
        v = zvars[idx % len(zvars)] if zvars else None
        if v is None:
            continue
        fr = strips[v]
        if not fr:
            continue
        # P1a: the same hash waves.c uses, so the poses scatter as in game.
        hp = (idx + 3) * 2654435761 & 0xFFFFFFFF
        hp ^= hp >> 15
        hp = (hp * 0x85EBCA6B) & 0xFFFFFFFF
        hp ^= hp >> 13
        f = fr[hp % len(fr)]
        size = 10 + rng.randrange(6)             # waves.c: 10 + rand()%6
        drawn = max(1, int(round(32 * (size / 16.0))))
        sp = tint(f, tints[idx % len(tints)]).resize((drawn, drawn),
                                                     Image.NEAREST)
        scene.alpha_composite(sp, (x, y))

    pf = cycle("player_walk") or [load("player")]
    if pf and pf[0] is not None:
        p = tint(pf[2 % len(pf)], PLAYER_TINT).resize((16, 16), Image.NEAREST)
        scene.alpha_composite(p, (W // 2 - 8, H // 2 - 8))

    up = 3
    big = scene.resize((W * up, H * up), Image.NEAREST)
    pad, head = 18, 100
    sheet = Image.new("RGBA", (big.width + pad * 2, big.height + pad + head), BG)
    label(sheet, (pad, pad), "Crowd shot - TRUE gameplay scale, %s theme" % theme, 22)
    label(sheet, (pad, pad + 32),
          "Player drawn 16x16 (scale 0.5), zombies 20..30px (size/16); whole "
          "shot then upscaled %dx with NEAREST." % up, 14, DIM)
    label(sheet, (pad, pad + 54),
          "Real per-theme tints, real random sizes, and the real P1a hashed "
          "start frame, so the horde is NOT in unison.", 14, DIM)
    label(sheet, (pad, pad + 76),
          "Variants in this tree: " + (", ".join(zvars) or "none"), 14, DIM)
    sheet.alpha_composite(big, (pad, pad + head))
    return save(sheet, out_dir, "crowd_%s.png" % theme)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=DEFAULT_OUT)
    ap.add_argument("--compare", default="main",
                    help="git revision to compare against, or '' for none")
    ap.add_argument("--scale", type=int, default=144,
                    help="pixels per filmstrip cell (>=128 so pixels are legible)")
    args = ap.parse_args()
    px = max(64, args.scale)
    out = os.path.abspath(args.out)
    made = []

    # ---- player ----
    pw = cycle("player_walk")
    if not pw:
        print("no player walk frames found in", ENT, file=sys.stderr)
        return 1
    made.append(save(strip(
        [("player, raw art", pw, None),
         ("player, slot tint", pw, PLAYER_TINT)],
        px, "Player walk cycle - 8 frames, 10 fps (0.8 s stride)",
        ["Raw art on top; below it the same frames under the slot colour mod "
         "COLOR_BLUE {0.30,0.40,1.00} the game applies.",
         "Frame 0 is the idle pose exactly, so starting and stopping does not "
         "pop. Judge: do the legs alternate, does the body bob, do the arms "
         "oppose?"]), out, "player_walk.png"))

    # ---- zombies, one strip per variant ----
    zvars = variants_present()
    rows = []
    for i, v in enumerate(zvars):
        fr = cycle(v + "_walk")
        rows.append((v.replace("zombie", "zombie ").strip() + ", raw", fr, None))
        rows.append((v.replace("zombie", "zombie ").strip() + ", tinted",
                     fr, ZOMBIE_TINTS["city"][i % 3]))
    if rows:
        made.append(save(strip(
            rows, px, "Zombie walk cycles - every variant, 8 frames each",
            ["Each variant raw, then under one of the real per-theme tints "
             "(city). Tints are a MULTIPLY, which is why the art stays light.",
             "Judge: is each variant a different silhouette at a glance, and "
             "does each cycle read as a shamble rather than a slide?"]),
            out, "zombie_walk.png"))

    # ---- idle poses side by side ----
    idles = [(v.replace("zombie", "zombie ").strip(), [load(v)], None)
             for v in zvars if load(v) is not None]
    if load("player") is not None:
        idles.insert(0, ("player", [load("player")], None))
    if idles:
        flat = [i[1][0] for i in idles]
        made.append(save(strip(
            [("idle / fallback art", flat, None),
             ("same, tinted", flat, (0.55, 0.50, 0.42))],
            px, "Idle (non-animated fallback) sprites: " +
            ", ".join(i[0] for i in idles),
            ["These are the plain <name>.png files used when the walk cycle is "
             "missing, and for the player they are byte-identical to walk "
             "frame 0."]), out, "idles.png"))

    # ---- before / after against another revision ----
    if args.compare:
        rev = args.compare
        old_p, old_z = cycle("player_walk", rev), cycle("zombie_walk", rev)
        if old_p and old_z:
            made.append(save(strip(
                [("BEFORE (%s)" % rev, old_p, None),
                 ("AFTER (this branch)", pw, None),
                 ("BEFORE, tinted", old_p, PLAYER_TINT),
                 ("AFTER, tinted", pw, PLAYER_TINT)],
                px, "Player walk: before vs after",
                ["Same 8 frames, same 32x32 PNG, same net_art base_px. Only "
                 "the pixels changed."]), out, "compare_player.png"))
            made.append(save(strip(
                [("BEFORE (%s)" % rev, old_z, None),
                 ("AFTER (this branch)", cycle("zombie_walk"), None),
                 ("BEFORE, tinted", old_z, ZOMBIE_TINTS["grass"][0]),
                 ("AFTER, tinted", cycle("zombie_walk"),
                  ZOMBIE_TINTS["grass"][0])],
                px, "Zombie walk: before vs after",
                ["Tinted rows use the grassland variant-0 tint "
                 "{0.30,0.60,0.20}."]), out, "compare_zombie.png"))
        else:
            print("note: could not read art from revision %r; "
                  "skipping the comparison sheets" % rev, file=sys.stderr)

    # ---- crowd shots ----
    made.append(crowd(out, "city"))
    made.append(crowd(out, "grass"))

    print("\n%d contact sheets in %s" % (len(made), out))
    for p in made:
        print(" ", os.path.abspath(p))
    return 0


if __name__ == "__main__":
    sys.exit(main())
