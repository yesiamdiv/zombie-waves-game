# Sprint 7 - lane A (art & animation overhaul) log

Agent `art`, branch `feature/art-overhaul`, worktree `worktrees/art`.
Lane A of `docs/TICKETS.md` (A1-A6). **The PM owns `docs/TICKETS.md`** - this
file is the lane's record and the PM merges it into the board.

Durable decisions live in `docs/knowledge/art-overhaul-plan.md`, not here.

## Status

| ID | What | Status | Commit |
|---|---|---|---|
| A1 | Player sprite redesign | **unproven** (done, not seen on a display) | `dd25514` |
| A2 | Zombie redesign | **unproven** | `dd25514` |
| A2 | Zombie variants (brute / runner) | **art unproven, WIRING NOT DONE - out of lane** | `4c6384e` |
| A3 | Walk cycle overhaul | **unproven** | `dd25514` |
| A4 | Dedicated idle / attack frames | **blocked - transport does not support them** | - |
| A5 | `--verify` clean; count re-baselined 50 -> 68 | done | `4c6384e` |
| A6 | B50 stays `unproven` until a human looks | held | - |

Contact sheets for review (regenerate with
`python tools/gen_contact_sheet.py`):

- `build/logs/art/player_walk.png`, `zombie_walk.png`, `idles.png`
- `build/logs/art/compare_player.png`, `compare_zombie.png` (vs `main`)
- `build/logs/art/crowd_city.png`, `crowd_grass.png` (true gameplay scale)
- `build/logs/art/before/` - the same sheets rendered from the pre-change art

## Entries

### 2026-11-20 - plan committed before implementation (`0ce56b6`)

Read `AGENTS.md`, lane A of `docs/TICKETS.md`, B50, the four animation/asset
knowledge topics, `docs/SYSTEM_DESIGN_RULES.md` and `docs/VERSIONING.md`, then
measured the real draw path instead of trusting the 32px authoring size. Plan
and reasoning: `docs/knowledge/art-overhaul-plan.md`.

### 2026-11-20 - contact sheet tool (`f0aebca`)

`tools/gen_contact_sheet.py`. The project has no way to review art: the gate is
headless and `--verify` only checks the pipeline. The crowd sheet reproduces
the real draw path (player 16x16, zombies 20-30px, real tints, the P1a hashed
start frame) because a sheet drawn at 32px on frame 0 would flatter the art and
hide the exact defect B50 reports.

### 2026-11-20 - A1 + A3 + base A2: entity art re-authored (`dd25514`)

Root cause of B50, measured, two parts:

1. **No internal contrast at all.** `entity_player()` and `entity_zombie()`
   drew head, shoulders, arms and rifle in the *same* tone `"w"` as the
   `body_any()` disc underneath them, so every feature was invisible. What
   reached the screen was a white egg with a dark notch. The "3-tone shading"
   was a comment, not pixels. `build/logs/art/before/` shows it.
2. **The motion was halved before it was seen.** The old stride moved arms by
   +-2 and the body by 1 *source* pixels on art that is downscaled 32 -> 16
   with NEAREST, so +-1 and 0-or-1 *screen* pixels - and neither figure had
   legs. Only arms moved, by one pixel. That is "doesn't feel like walking".

Fixed by authoring on a 16x16 logical grid emitted 2x into the same 32x32 PNGs
(no `.c`/`.h` change, `base_px` untouched), with real legs, an alternating
stride, a body bob at twice stride frequency and opposed arm swing.

### 2026-11-20 - A2 variants (`4c6384e`), texture re-baseline 50 -> 68

Two extra builds, `brute` and `runner`, each an idle plus a full 8-frame cycle.
**Unreferenced art**: the wiring is outside lane A and is specified in
`docs/knowledge/art-overhaul-plan.md`. Separate commit so it can be dropped
whole; dropping it restores 50 textures and a clean `--verify`.

## For the PM

1. **A4 is blocked, not skipped.** Dedicated idle and attack frames cannot be
   added from this lane. A frame is only addressable if it has an **art id**,
   an art id is resolved from a texture **path**, and the id blocks are fixed
   in `src/net/net_art.h` at `NET_ART_*_WALK_COUNT = 8`
   (`docs/knowledge/animation-transport.md`, design A). The game already
   *approximates* both states out of the walk cycle - `system_animation()`
   holds frame 0 when idle and `ANIM_ATTACK_FRAME = 6` while attacking - so
   this lane did the in-lane maximum instead: frame 0 is now a real standing
   pose, and frame 6 is the cycle's furthest reach, which
   `entity_zombie_walk()` now asserts so the two cannot drift. Adding true
   idle/attack frames means appended art ids, a `NET_ASSET_VERSION` bump, the
   `test_net_codec` update in the same commit, and an `anim.h` state ->
   id mapping that the host renderer and the snapshot builder both go through.

2. **A3 verdict: frame CONTENT, not frame count.** 8 frames at
   `ANIM_FRAME_PERIOD = 0.1 s` is a 0.8 s stride, a normal walk period, and
   15 fps is a hard ceiling anyway because the snapshot stream is 20 Hz. More
   frames would have changed nothing; the frames had no legs. The count is
   also not ours to change - it is a protocol number.

3. **MERGE HAZARD on `docs/knowledge/gate-baseline.md`.** The verifier reads
   the **last** `N textures` declaration in the file. This branch appends a
   `68 textures` re-baseline at the end. Ticket W5 will append a Windows gate
   section that almost certainly says `50 textures`. If W5's section lands
   *after* this one, `--verify` breaks on the merged tree even though both
   branches are individually correct. Order the sections, or make W5's Windows
   section cite 68.

4. **PNG byte churn on this box.** Running the generator here rewrites all
   texture files byte-for-byte with identical pixels (Pillow 12.0.0). Every
   commit in this lane includes only the pixel-changed files; the rest were
   reverted. Details and a detection snippet:
   `docs/knowledge/pillow-png-byte-churn.md`.

5. **This lane touched no `.c`, `.h` or `CMakeLists.txt`.** `git diff
   <merge-base> HEAD -- src tests CMakeLists.txt` is empty, so the
   single-player seed-42 byte-identity gate cannot have moved and the C tests
   (which assert art *paths* and *ids*, never pixels) are unaffected. The
   Windows build was not run: no C changed, and `worktrees/windows` owns it.

6. **Everything visual is `unproven`.** `--verify` reports a clean 68 textures,
   which proves the pipeline is intact and nothing about how the art looks or
   whether the walk reads as walking. A PM review of the contact sheets is the
   best check available while the owner is away; it is still not a sign-off on
   the game in motion. B50 stays OPEN/unproven (A6).
