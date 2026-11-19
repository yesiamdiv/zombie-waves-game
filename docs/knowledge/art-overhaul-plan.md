# Art overhaul (B50 / Sprint 7 lane A) - the plan, and the measurement that drove it

<!-- Project knowledge. Agents append entries; edit freely. -->

## 2026-11-20 - decision by art: author entity art on a 16x16 logical grid, emitted 2x

Owner finding B50 (2026-11-19): *"we are lacking in player and zombie
animations... doesn't feel like walking or anything"*, root cause attributed to
the art rather than the frame transport. Owner art direction (recorded in
`docs/TICKETS.md`): **richer pixel art, same style** - keep the 16px grid, keep
everything procedural in `tools/gen_assets.py`, do not raise the sprite base
resolution, do not hand-author PNGs.

### The measurement nobody had taken: the art is downscaled before you see it

Entity art is authored as a 32x32 grid and saved at scale 1, so the PNG is
32x32 and `net_art.c` `base_px` is 32. But **nothing draws it at 32px**:

| Entity | `CSprite.scale` | Source | Drawn size at `zoom == 1` |
|---|---|---|---|
| player | `0.5` (`src/players.c`) | 32px | **16x16** |
| zombie | `size / 16`, `size = 10 + rand()%6` (`src/world/waves.c`) | 32px | **20x20 .. 30x30** |

and `asset_manager.c` sets `SDL_SCALEMODE_NEAREST`. `render.c` passes
`draw_rot = 0.0f` for every entity, so sprites never rotate either.

Three consequences, and together they are most of B50:

1. **Half the authored player pixels never reach the screen.** A 32 -> 16
   nearest downscale samples every other row and column, so every odd-aligned
   pixel is simply discarded. The old player's 1px-wide details (visor edge,
   arm outlines) were a coin flip.
2. **The walk motion was halved on the way to the screen.** The old player
   stride table moved the arms by +-2 and the body by 1 *source* pixels; after
   the 0.5 downscale that is +-1 and 0-or-1 *screen* pixels. A one-pixel
   wobble is exactly "doesn't feel like walking".
3. **The zombie's non-integer downscale (0.625 .. 0.94) is unstable.** Which
   source pixels survive changes with the random size, so 1px features shimmer
   between zombies instead of reading as one creature.

### The decision

**Author every entity frame on a 16x16 logical grid and emit it 2x-nearest into
the existing 32x32 PNG.**

- The PNG stays 32x32, so `net_art.c` `base_px = 32` is untouched and
  `verify_entity_sizes_vs_net_art()` still passes. No `.c`/`.h` change at all.
- The player's on-screen 32 -> 16 nearest downscale now reproduces the authored
  16x16 pixel art **exactly**. What is drawn is what was designed.
- Every logical pixel occupies a 2x2 source block, so it survives *any*
  downscale factor >= 0.5. The zombie's 0.625 .. 0.94 range can no longer drop
  a feature; it can only widen one by a pixel.
- It is the owner's direction taken literally: this makes the entities true
  16px sprites instead of 32px sprites pretending to be 16px ones.
- Motion amplitudes are now specified in logical pixels, and 1 logical pixel is
  2 source pixels, so a 2-logical-pixel arm swing is visible rather than
  half-lost.

### What this lane can and cannot touch

The frame count is **locked by the C headers**, not by the generator:
`NET_ART_ZOMBIE_WALK_COUNT` / `NET_ART_PLAYER_WALK_COUNT` are 8 in
`src/net/net_art.h`, `ANIM_WALK_FRAMES` is derived from them in
`src/graphics/anim.h`, and the per-frame art ids are a contiguous appended
block in the shared art table. Changing 8 to anything else, or adding idle or
attack frames of their own, is a `net_art.h` + `net_art.c` + `NET_ASSET_VERSION`
change - i.e. **out of this lane** (lane A is generator + PNG pixels only).

So the verdict on A3 is: **the problem is frame CONTENT, not frame count.**
8 frames at `ANIM_FRAME_PERIOD = 0.1 s` (10 fps) is a 0.8 s stride, which is a
normal walk period, and the rate ceiling is 15 fps anyway because the snapshot
stream is 20 Hz (`docs/knowledge/animation-transport.md`). What was missing was
amplitude and limb articulation: the old cycles moved only arms (the figures had
no legs at all) by 1 screen pixel. The fix is real legs with an alternating
stride, a 1-logical-pixel body bob at twice stride frequency, opposed arm swing,
and - for the zombie - a limp that makes the two halves of the cycle unequal.

### Tint discipline (unchanged, and now load-bearing for shading)

Both sprites are colour-modulated at draw time: the player by its slot colour
(`COLOR_BLUE = {0.3, 0.4, 1.0}`), the zombie by one of three per-theme
`zombie_tints` with components 0.3 .. 0.8 (`src/world/theme.c`). Colour mod is a
multiply, so **the art must stay light and desaturated**: a saturated or dark
base multiplies to mud. Three tones plus a near-black outline survive a multiply
with their relative contrast intact, which is why 3-tone shading is the right
amount of shading here and a 5-tone ramp would not survive.

### Facing

`draw_rot` is always 0, so facing cannot come from rotation - it must be baked
in, and there is exactly one facing. Both figures are therefore drawn
**front-facing (towards the camera)**: a visible head top, shoulders wider than
the hips, and the weapon held on one side. "Clear facing" in A1 means the
silhouette is unambiguously a front view, not that the sprite turns.

### Zombie variants (A2) - the art is in lane, the wiring is not

A crowd reads as one blob for two reasons. One is in lane: the old zombie was a
symmetric `body_any()` disc, so every zombie was the same symmetric lump and the
only variation was a tint and a size. The other is not: all zombies share the
same eight textures, because `system_animation()` picks the frame and
`anim_walk_texture()` maps frame -> art id with no per-entity variant input.

This lane therefore delivers the variant **art** (walker / brute / runner, each
a full 8-frame cycle plus an idle) as additional PNGs on the existing 16x16
grid and the existing 32px size, and leaves the wiring to the PM. Wiring needed,
all of it outside lane A:

1. `net_art.h`: append two contiguous 9-id blocks (idle + 8 frames per variant).
   Append only - never renumber.
2. `net_art.c`: the matching rows, `base_px = 32` for all of them.
3. A per-entity variant selector. The variant is **already drawn** in
   `waves.c` (`int variant = rand() % 3`) but is thrown away after picking a
   tint - it never reaches the ECS (`src/net/net.h` says so explicitly). Store
   it on `CZombieTag` (or `CAnimation`), then have one function - next to
   `anim_walk_art_id()` in `src/graphics/anim.h`, never a second copy - map
   (variant, frame) -> art id. Host screen and snapshot builder must both go
   through it, per `animation-transport.md`.
4. `NET_ASSET_VERSION` bump in the same commit, plus the `test_net_codec`
   update, per `docs/VERSIONING.md`.
5. No new `rand()` calls, and the existing `rand() % 3` must stay in the same
   place in the sequence, or the seed-42 SP events log moves.

Until that lands the variant PNGs are unreferenced art. That is a deliberate,
revertible hand-off: it is one commit, and the texture-count re-baseline it
needs is in that same commit.

### Verification honesty (A6)

`gen_assets.py --verify` proves the pipeline is intact - RGBA, transparent
corners, widths matching `base_px`, declared count. It proves **nothing** about
whether the art looks good or whether the walk reads as walking. Per
`docs/SYSTEM_DESIGN_RULES.md` section 4 this lane's output is `unproven` until a
human looks at it on a real display. The contact sheets in `build/logs/art/`
exist so that the PM can do a real visual review before the owner is back; a PM
sign-off on a contact sheet is still not a sign-off on the game in motion.
