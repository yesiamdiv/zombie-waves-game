# Sprint 7 - lane A (art & animation overhaul) log

Agent `art`, branch `feature/art-overhaul`, worktree `worktrees/art`.
Lane A of `docs/TICKETS.md` (A1-A6). **The PM owns `docs/TICKETS.md`** - this
file is the lane's record and the PM merges it into the board.

Durable decisions live in `docs/knowledge/art-overhaul-plan.md`, not here.

## Status

| ID | What | Status | Commit |
|---|---|---|---|
| A1 | Player sprite redesign | planned | |
| A2 | Zombie redesign + variants | planned | |
| A3 | Walk cycle overhaul | planned | |
| A4 | Idle/attack frames (only if transport supports) | investigating | |
| A5 | `--verify` stays clean | continuous | |
| A6 | B50 stays `unproven` until a human looks | continuous | |

## Entries

### 2026-11-20 - plan committed before implementation

Read `AGENTS.md`, `docs/TICKETS.md` lane A, B50 in `docs/BUGS.md`, the four
animation/asset knowledge topics, `docs/SYSTEM_DESIGN_RULES.md` and
`docs/VERSIONING.md`, then measured the actual draw path rather than trusting
the 32px authoring size.

The plan, with the root-cause measurement that drove it, is
`docs/knowledge/art-overhaul-plan.md`. Headline: **entity art is authored at
32px but drawn at 16px (player) / 20-30px (zombie) with NEAREST sampling**, so
half the authored detail and half the walk amplitude never reached the screen.
The lane therefore re-authors on a 16x16 logical grid emitted 2x into the same
32x32 PNGs - no `.c`/`.h` change, no `base_px` change, no size change.

Also noted for the PM: running `tools/gen_assets.py` on this Windows box
rewrites all 50 PNG files byte-for-byte while leaving every pixel identical
(Pillow 12.0.0 encodes PNG chunks differently from whatever produced the
committed files). This lane commits only the PNGs whose **pixels** changed and
reverts the rest, so the diffs stay meaningful. See
`docs/knowledge/pillow-png-byte-churn.md`.
