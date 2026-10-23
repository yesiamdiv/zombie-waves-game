# Animation transport: design A (art-id frames) vs design B (phase field)

<!-- Project knowledge (Intercom-AI). Agents append entries; edit freely. -->

## 2026-10-09 12:35 UTC - decision by assets: PARKED pending a spike. Lean A. Nobody may build B speculatively.

STATUS: PARKED. This is not 'silence to be defaulted on' - it is pending a
spike verdict and must not be acted on either way. `knowledge/animation-plan` §3
makes this the spike's first verdict.

DESIGN A — the frame travels as an art id. No wire change.
DESIGN B — a phase field is added. Bumps `NET_WIRE_VERSION` 8 -> 9 and requires
a `test_net_codec` update in the same commit.

TWO CHECKS THAT DECIDE IT, both measured not argued:
1. **Sheet constraint.** If a frame lives *inside a sprite sheet*,
   `asset_manager_name_of()` cannot name it, so design A is simply unavailable
   for sheets. Spike A settles this in about an hour.
2. **20 Hz aliasing.** If frames are separate textures, A is available - but
   sampling an 8-frame walk cycle at 20 Hz can stutter or run backwards on the
   client. Whether that is acceptable must be measured.

IF B WINS, its shape: append at the end of `NetEntitySnap` - `anim_phase u8`
(host-authoritative; the client may interpolate between snapshots
deterministically), plus `anim_state u8` (idle/walk/attack) and `facing u8`
**only if** the spike shows they are needed. Per **entity**, not per slot -
zombies are not slots and they are most of the animation.

ID-REUSE RISK: under B, a recycled ECS index can briefly wear a dead entity's
phase unless the design resets phase on a changed `kind`/`art`. Design A is
stateless per snapshot and does not care. A mild point for A, a real duty for B.
Logged as a risk in `knowledge/animation-plan` §7.

LOAD-BEARING CONSTRAINT EITHER WAY: animation stays strictly downstream of the
snapshot - no RNG, no feedback into the sim, no reordering around
`system_cleanup`, and the client computes only interpolation between two host
frames. `SYSTEM_DESIGN_RULES.md` §1 holds regardless of which design wins.

PARTICLES (B29): stay accepted as their own item. Animation does not fold them
in; effects would be a separate proposal with the bandwidth math first.

**Why:** Design A gives maximum host authority - the client draws literally what the host drew, which is the property the I5 fix exists to protect - and needs no wire change. But two measurable constraints can force B, and the spike decides, not the argument.

## 2026-10-22 18:30 UTC - verdict by animation: DESIGN A, individual frame files, animation rate <= 15 fps

The spike ran on `feature/animation-assets` (worktrees/animation). Every spike
change was thrown away afterwards: the tree is clean at `68280a3`, `git status
--short -- src/net` is empty, and the gate is green. This entry is the only
thing that lands from the spike.

### Spike A - frame storage: individual PNGs vs one sheet

One 8-frame zombie walk cycle, both ways, thrown away afterwards.

| | individual PNGs | one 256x32 sheet |
|---|---|---|
| files under `assets/` | 8 | 1 |
| bytes on disk | 2234 | 629 |
| generator LOC (as spiked) | +24 | +30 |
| `gen_assets.py --verify` | 0.11 s, clean tree | 0.11 s, clean tree |
| one-frame change | **1 file**: `zombie_walk3.png` (281 bytes before and after - only pixels move) | **1 file**: `zombie_walk.png` 629 -> 709 bytes; 77 pixels differ, all inside frame cell 3, the rest of the sheet is re-encoded |

Generator LOC and file count are NOT the differentiator (2 KB is not a
constraint here, and both routes are one function over a shared frame builder).
The difference that decides it is this:

**A sheet cannot be named.** `asset_manager_name_of()` resolves a texture to a
PATH, and all 8 frames of a sheet share one path, so the path-only reverse
lookup hands back the SAME art id for every frame:

```
frame 0 src=(0,0,32,32)    -> name_of = textures/entities/zombie_walk.png -> net_art_from_path = 10
frame 7 src=(224,0,32,32)  -> name_of = textures/entities/zombie_walk.png -> net_art_from_path = 10
```

The frame identity is gone before it reaches the wire. Making it work was
measured by writing it (then thrown away):

- `src/net/net_art.h` +8/-1 - two struct fields, three declarations
- `src/net/net_art.c` +39/-10 - the 10 existing rows each grow two fields,
  three new functions
- `src/net/net_snapshot.c` **1 line, and it is NET'S FILE**:
  `net_art_from_path(name)` -> `net_art_from_sprite(name, cs->sprite.src.x, cs->sprite.src.y)`
- `src/systems/render.c` +5/-1 - the mirror has to apply the frame's src rect
- `tests/tests.c` +5/-2 - the art-table invariant becomes `(path,src) -> id`,
  and the CURRENT test **fails 7 times** until it is changed

With individual files all 8 frames resolve to 8 distinct paths -> 8 distinct
art ids -> **no code change outside the art table**, and the client needs **no
render change at all**: `system_render_mirror()` already resolves
art id -> path -> texture through `sprite_tex(art_path)` (render.c:140-141).

### Spike B - animation is invisible to the gate

`COMP_ANIMATION` attached to zombies in `waves_spawn_zombie()`:

- `total_frames = 1` (a no-op): ctest 3/3, `zombie_tests` 1186/0, two
  same-seed SP runs diff-empty (125938 bytes / 1603 lines) and identical to the
  pre-spike baseline.
- `total_frames = 8`, `frame_duration = 0.125` (a real 8 fps cycle): the same
  green gate, the same byte-identical SP runs.

`system_animation()` takes no RNG, emits no events, and is called after the sim.
Only `render.c` and `net_snapshot.c` ever READ `CSprite` - every other
reference in the tree is a write at spawn time. So the frame index can live in
the ECS without the gate noticing, and the animation cannot feed back into the
sim.

### Spike C - 20 Hz aliasing, measured on the real `system_animation`

The host ticks at 60 Hz, the snapshot is built at `NET_SNAP_HZ` = 20, and the
client holds the last received art id for one transport interval (50 ms).
Measured over 8 s, counted per 8-frame cycle:

| frame period | rate | distinct frames per cycle | skipped steps | backwards steps | phase lag |
|---|---|---|---|---|---|
| 33.3 ms | 30 fps | 5..6 of 8 | 68 | 0 | 0..3 frames |
| 41.7 ms | 24 fps | 6..7 of 8 | 30 | 0 | 0..2 frames |
| 50.0 ms | 20 fps | 7..8 of 8 | 0 | 0 | 0..2 frames |
| 66.7 ms | 15 fps | 8 of 8 | 0 | 0 | 0..2 frames |
| 83.3 ms | 12 fps | 8 of 8 | 0 | 0 | 0..1 frame |
| 100 ms | 10 fps | 8 of 8 | 0 | 0 | 0..1 frame |
| 125 ms | 8 fps | 8 of 8 | 0 | 0 | 0..1 frame |

Two findings, one of them a surprise:

1. **The cycle must not run faster than ~15 fps.** Below that the 20 Hz
   snapshot cannot carry every frame and the cycle SKIPS: at 24 fps a quarter of
   the steps are skipped and the walk pops. At 15 fps and slower every single
   cycle shows all 8 frames, with zero skipped steps.
2. **The client never sees the animation run BACKWARDS** - 0 backwards steps at
   every rate from 3 fps to 30 fps. The risk written down in
   `knowledge/animation-plan` §7 is half wrong: the client can stutter (it
   skips) but it cannot run backwards, because it only ever holds the last id
   it was given. Phase lag is 0..1 frame, i.e. within one snapshot interval.

### THE VERDICT

**DESIGN A, individual PNG files, animation rate <= 15 fps (target 10 fps).**

- **Wire design: A.** No new snapshot field, no `NET_WIRE_VERSION` bump, no
  `NET_SNAP_ENTRY_BYTES` change - the frame travels as the art id that is
  already in the snapshot. Design B is NOT built: it would need
  `NET_WIRE_VERSION` 8 -> 9, a `test_net_codec` update and a per-entity phase
  byte, to buy a smoother cycle that the measurement shows is unnecessary at
  <= 15 fps.
- **Frame storage: individual PNGs.** The sheet route needs a change inside
  `src/net/net_snapshot.c` (net's file), a src-rect draw path, and a changed
  art-table invariant, to save 1.6 KB.
- **Frame index location: the ECS** (`COMP_ANIMATION.current_frame`), owned by
  `system_animation()`. Frame -> art id -> texture is a render-side table (the
  shared art table), not ECS state. Spike B proves the ECS side is invisible to
  the gate.
- **HARD RULE, binding on the content phase: the animation frame period must be
  >= 67 ms (<= 15 fps).** At 10 fps an 8-frame cycle is 800 ms.

### NT4 (the sword) - answered here because net asked for it in this verdict

**NT4 = (b) procedural blade**, NOT frame-based sword art.

- The blade's angle is continuous and unbounded: `SWORD_SPIN_SPEED` is 5.0 rad/s
  = 286 deg/s, one full turn every 1.26 s. Frame-based sword art would need
  >= 16 frames per revolution just to keep the angular error under 22.5 deg,
  i.e. >= 12.7 frames/s - sitting exactly on the aliasing limit measured above.
- The host draws the sword procedurally (`sprite_draw_blade()`, render.c:22-32,
  from `CSwordTag.angle` and `outer_radius - radius`). Frame-based sword art
  would change the single-player picture, which the invariance rule forbids.
- So the snapshot carries the angle (quantised, int16 centidegrees, wrapping at
  180 deg) and the client draws the blade through the SAME
  `sprite_draw_blade()` path, interpolating the angle between the two host
  frames with wrap-safe shortest-path maths - the same interpolation the mirror
  already does for positions (`net_mirror_sample()`).
- Required `NET_ENT_SWORD` fields: kind + owner + art (blade art) + size_q
  (blade length) + tint + `angle` (int16 centidegrees), plus the extensible
  attribute block owner D1 asked for (attack speed and later additions). The
  angle rides the SAME wire bump as B38 - one bump, not two - and
  `net_snapshot.c` is net's to write under the freeze protocol.

### What this does NOT prove

A green gate cannot see a picture. The walk cycle's on-screen look, the
perceptual effect of the <= 15 fps rule, and the client's frame cadence are
**UNPROVEN** until a human watches two windows (Phase D is blocked on that
check anyway).

**Why:** The two measurable constraints in the 2026-10-09 entry both came out
in design A's favour, but not for the reason the argument expected. The sheet
constraint was real and it is what rules sheets out - not because a sheet
cannot be named, but because naming one costs a change in a file animation does
not own. The aliasing constraint is real too, and it does not force B; it
forces a RATE LIMIT (<= 15 fps), which is a content rule rather than a wire
redesign. Design A therefore stands, with one hard rule attached to it.

## 2026-10-22 18:55 UTC - note by animation: how the frame actually gets on screen (AN3)

The transport question is settled (design A: the frame travels as the art id
the snapshot already carries). This entry records the *wiring*, because it is
the part that decides whether the host and the client can ever disagree.

**The frame lives on the sprite.** `system_animation()` advances
`CAnimation.current_frame` and then puts that frame's texture into the
entity's `CSprite`. Everything downstream reads the sprite and needs to know
nothing about animation:

- the host's renderer draws `CSprite` (`system_render`), so the host shows the
  walk frame;
- the snapshot builder reads the sprite's texture NAME and turns it into an art
  id (`asset_manager_name_of` -> `net_art_from_path`,
  `src/net/net_snapshot.c`), so the wire carries the frame's id;
- the client resolves that id back to a path and a texture
  (`system_render_mirror`), so the client shows the frame the host chose.

**Why the sprite and not a separate "animation" field on the wire:** a second
place that answers "which frame is this" is a second place that can answer it
differently. The spike's whole lesson was that a peer inventing a value the
host owns is the bug class this project keeps hitting (B19-B23). Putting the
frame on the sprite means the host has ONE answer and the client copies it.

**What this changed: three files, none of them net's.**

- `src/graphics/anim.{h,c}` (new) - the frame-selection function:
  `anim_walk_art_id(frame)`, `anim_walk_path(frame)`, `anim_walk_texture(frame)`.
  The frame number maps to an art id by adding (`NET_ART_ZOMBIE_WALK0 + frame`),
  and the art table's contiguity is asserted at compile time.
- `src/systems/animation.c` (new) - `system_animation()` moved out of
  `render.c` (it is not rendering) and given the sprite swap.
- `src/world/waves.c` - a spawned zombie gets `COMP_ANIMATION` and starts on
  frame 0, which IS the idle pose (pixel-identical to `zombie.png`), so it does
  not pop when it starts walking.

`src/net/net_snapshot.c`, `src/net/net_codec.c` and the client's render path
are UNTOUCHED, and the wire layout did not move: `NET_WIRE_VERSION` 8,
`NET_SNAP_ENTRY_BYTES` 35. The client needed no change at all - it already
resolves an art id to a texture - it only needed the host to send the right id.

**The rate is now code:** `ANIM_FRAME_PERIOD` = 0.1 s (10 fps), with the
<= 15 fps ceiling written down next to it. AN4 pins the rule with a test.

**What this does NOT prove:** the picture. A headless gate cannot see a walk
cycle, and the client path still needs a human watching two windows. The
on-screen items stay open in `docs/PENDING_VERIFICATION.md`.

## 2026-10-22 18:58 UTC - note by animation: the rate rule is code, and the test has teeth (AN4)

The spike left one HARD RULE: the animation frame period must be >= 67 ms
(<= 15 fps). A rule that only exists in a document is a rule that gets broken
by the next person who does not read documents, so it is now a constant plus a
test that fails when the rule is broken.

- `src/graphics/anim.h`: `ANIM_MAX_FPS 15` and
  `ANIM_FRAME_PERIOD_MIN (1.0f / ANIM_MAX_FPS)`, with `ANIM_FRAME_PERIOD`
  (0.1 s = 10 fps) checked against it.
- `tests/tests.c`: `test_animation_rate_rule`. It simulates the CLIENT the way
  the network does it - the host ticks at 60 Hz, a snapshot goes out every
  `NET_SNAP_HZ` (20), and the client HOLDS the last art id it was given - and
  counts, per cycle, how many of the 8 frames the client actually saw and how
  many steps it missed.

**The test has teeth, and that was checked by breaking it on purpose:** with
`ANIM_FRAME_PERIOD` set to 1/30 the suite fails on both counts - the constant
check AND the behaviour (the client sees 5..6 of the 8 frames per cycle and
misses steps). Restored, the suite is green. So the rule is not "the constant
is big enough"; it is "the client still sees every frame".

**What the simulation reproduces** (the spike's table, in float this time):
10 fps 8/8, 0 missed; 15 fps 8/8, 0 missed; 12 fps 8/8; 24 fps 6/8, 2 missed
per cycle. The 15 fps boundary is safe, but only just - it is 4 host ticks per
frame, so a one-tick jitter can cost a frame. That is why the project runs at
10 fps (6 ticks per frame) and why the target is 10 rather than 15.

The simulation also confirms the spike's other finding: the client never runs
BACKWARDS (0 backwards steps at every rate) - it can stutter, it cannot
reverse, because it only ever holds the last id it was given.

**Still unproven:** the rate is arithmetic, not perception. Whether 10 fps
reads as a walk or as a slideshow is a question for human eyes
(`docs/PENDING_VERIFICATION.md`).

## 2026-10-22 19:05 UTC - note by animation: the sword angle contract (AN7)

AN7 was "sword art ids + angle contract for net". Net's v9 landed first
(`99f8db4`) and consumed what already existed, so AN7 turned out to need **no
new animation code** - but it did need the contract written down, because
"unsigned vs signed" is the kind of detail that is silently wrong.

**What the animation lane guarantees (unchanged by AN7):**

- The blade art id is `NET_ART_SWORD = 9` in the shared table
  (`src/net/net_art.h`), path `textures/entities/sword_blade.png`,
  `base_px 40`.
- The file is 40x16 RGBA and `entity_blade()` in `tools/gen_assets.py` draws
  the handle at x=0..6 and the tip at x=39 - so the texture's **left edge is
  the handle**. `sprite_draw_blade()` maps tex x=0 to the handle and x=1 to the
  tip, which matches the art; no mirroring is needed.
- The host spawns the sword's sprite with that texture
  (`src/systems/sword.c`), so the snapshot's `art` resolves to 9 through
  `net_art_from_path`.
- The sword carries no `COMP_ANIMATION`, so the frame selectors cannot clobber
  the blade.

**What the wire carries (net v9) - the one detail that had to be pinned:**

- The angle is a **signed int16 in centidegrees, in (-18000, +18000]**;
  18000 = 180 degrees. It is *signed* because the blade is drawn from a pivot:
  an unsigned half-turn (0..18000) would draw one half of the spin backwards.
  -172.5 degrees must survive encode/decode.
- Interpolation is **wrap-safe**: +170 -> -170 goes through 180, not through 0
  (`net_mirror_sample_attr`).
- `size_q` for a sword is its **blade length** in half units; width is a
  separate attribute byte (the host's `SWORD_WIDTH`), also half units.
- None of this moved `NET_SNAP_ENTRY_BYTES` (still 35): the attributes ride a
  trailing self-describing table, so any future attribute can be added without
  moving an existing offset.

**Not done, deliberately:** no `NET_ART_*` row was added or changed, no
`NET_ASSET_VERSION` bump, no `net.h` edit. The frame still travels as the
existing `art` id and the walk cycle adds nothing to the snapshot.

**Unproven:** the client's mirrored sword - angle, width, tint, and the
no-backwards-flip at 180 - needs a two-window run (`docs/PENDING_VERIFICATION.md`
M9). v9 draws it; nothing headless can see an angle.

## 2026-10-22 19:30 UTC - note by animation: the pose follows the state, and the bob is display-only (AN5)

**The pose follows the zombie's state.** Before AN5 every animated zombie
advanced the walk cycle no matter what it was doing, so a zombie hitting you
looked like it was walking in place. `system_animation()` now reads
`CZombieTag.state`:

| State | Pose |
|---|---|
| `ZOMBIE_CHASE` | the cycle advances (the only walking state) |
| `ZOMBIE_ATTACK` | holds `ANIM_ATTACK_FRAME` (6): the walk cycle's furthest reach, +3 px on the reaching arm |
| `ZOMBIE_HURT` | freezes on the current frame (the white flash says "hurt") |
| `ZOMBIE_IDLE` | holds frame 0, the stationary pose |
| `ZOMBIE_DEAD` | freezes (the entity is normally destroyed first) |

Entering ATTACK and IDLE resets `frame_timer`, so leaving the state resumes on
the next frame instead of holding a stale timer.

**No death pose, deliberately.** A zombie is destroyed the instant it dies
(`system_cleanup`) and the death reads as its particle burst. A corpse/death
pose would have to delay that destruction - a gameplay change that moves the
death timestamp in the single-player events log, which is exactly what the
single-player invariance gate exists to prevent. If a corpse state is wanted,
it is a gameplay item, not an animation one.

**The pickup bob is display-only.** `items_tick_bob()` moves the entity's
`CPosition` - what the renderer draws and the snapshot replicates, so host and
client bob together - while `CItemTag.base_pos` keeps the true position for the
pickup range and the `GE_ITEM_PICKUP` payload. That split is what makes the bob
safe: the bob can be 2.5 px and the pickup timing still does not move by a
single tick (single-player events log unchanged, 125938 bytes / 1603 lines).
The bob is vertical only, 1.5 Hz, accumulated from `dt` - no `rand()`, so it is
deterministic and does not touch the seed-RNG order.

**The missing-texture fallback** is now tested rather than assumed: a headless
build has no textures, and the zombie keeps the flat shape it spawned with
while the frame index still advances, because the frame index is what the
snapshot names. A missing PNG becomes the flat shape, never an invisible
entity and never a crash.

**Unproven:** whether the lunge frame reads as an attack and whether the 2.5 px
bob reads as hovering (docs/PENDING_VERIFICATION.md).
