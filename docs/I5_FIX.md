# R13-I5 — how "the client renders dots, not sprites" was fixed

**Status:** fixed in code, commit `f7c44f3`. **Still unproven on screen** —
see "What nobody has verified yet" at the bottom. This document explains the fix
so the next person does not re-derive it or accidentally undo it.

Written after the fix, for future development. If you are reading this to decide
*what* the code should do, read "Why the obvious fix was rejected" first — it is
the part that is not visible in the diff.

---

## 1. The symptom

Two real windows, host and client, playing the same match. The player and
zombie *shapes* differ between them. The client shows flat coloured circles.
Reported on-device 2026-09-29 by the user watching two windows at once.

This survived nine commits and two earlier beacon fixes (I2a/I2b) because it is
a **visual** defect and every automated gate in this repo is headless.
`render()` returns immediately when `game.headless` is set, so no test, no
`ctest`, and no 25-second bot run can see a sprite. Only a human with two
windows can.

## 2. Root cause

The host and the client do not share a renderer. They never did.

| | host | client |
|---|---|---|
| entry point | `system_render` (`src/systems/render.c`) | `system_render_mirror` (`src/systems/render.c`) |
| player | the entity's own `CSprite` from `spawn_player_entity` | `sprite_circle(10.0f, net_slot_color(slot))` |
| zombie | real `CSprite` | `sprite_circle(11.0f, COLOR_GREEN)` |
| bullet / item | real `CSprite` | `sprite_circle(3.0f)` / `sprite_circle(7.0f)` |

`system_render_mirror` never reconstructed sprite geometry. It switched on
`NetEntitySnap.kind` and **invented a circle per kind**. `NetEntitySnap` said
nothing about how anything looked, so the client had no choice but to guess — and
it guessed something that happened to be a reasonable circle.

Colour was already correct (`net_slot_color(0)` is the same constant the host's
local player uses), which is why the bug reads as "shape only" and is genuinely
only shape and size.

### Why I2a/I2b missed it

Those fixes made the beacon render *beneath* the player instead of over it, and
both were checked only as "is the beacon in front". On a client there was never a
real sprite underneath, so a correctly-ordered dot looks like a successful fix
while the character is still missing. Ordering bugs and missing-geometry bugs
look identical when the thing underneath is a circle.

## 3. Why the obvious fix was rejected

The first decision recorded for I5 was: *"mirror the host's shape constructors
client-side; do not put sprite geometry on the wire"*, explicitly **rejecting**
transmitting the sprite because it widens every snapshot entry and pushes render
concerns into the netcode.

**That decision was wrong, and it was reversed.** Why:

That plan makes both processes *agree by duplicating a formula*. The client would
call the same `sprite_rect(16, 16, color)` the host calls, keyed off entity
kind. But "agree by duplication" means any future change to the host's shape
constructor — a new zombie size, a rescaled player sprite, an art swap — has to
be made in two places, and until it is made in both, the two windows silently
disagree again. The bug is not that the client drew the wrong circle. The bug is
that the client was *guessing* at all. Duplicating the guess rule keeps the
guessing and just adds a second copy of it to keep in sync.

A shared table makes this worse, not better. If the host reports "zombie, size 11"
and the client looks that up, then changing the number means changing the table;
the table has to be versioned, and a peer with an older table draws the wrong size
with no complaint. The size has to travel.

The rule that follows, and that `docs/SYSTEM_DESIGN_RULES.md` now states: **the
host is the only authority for anything a player can see.** Guessing on the
client is not a rendering shortcut, it is an authority violation — it is the same
bug class as the client HUD reading its own unsimulated ECS (B22, B23) and the
wave panel reading a `waves` struct that is frozen at init on a client (B26).

## 4. The actual fix

`NET_WIRE_VERSION` 3 → 4. `NetEntitySnap` gained an appearance block, 26 → 31
bytes per entry:

```c
uint8_t art;        /* NET_ART_*: which sprite the host drew */
uint8_t size_q;     /* world-space diameter in HALF units (size = size_q / 2) */
uint8_t tint[3];    /* the host's literal CSprite RGB, 0-255 each */
```

Three deliberate choices in that layout:

- **`art` is a numbered table id, not a path or a texture handle.** Both peers
  resolve the id identically (`src/net/net_art.c`, shared source, not two copies
  of a table), new artwork is a one-line append with no wire change, and it costs
  one byte. A path would cost ~30 bytes per entity per snapshot, 20 times a
  second, for a string both sides already agree on.
- **`size_q` is a world-space diameter, not a texture-relative scale.** A scale is
  meaningless without knowing the texture's pixel size, so the client would have
  to know the art dimensions to use it — re-introducing exactly the coupling this
  is meant to remove. Half units keep it an integer (no float on the wire) while
  staying independent of the art.
- **`tint` is the host's literal RGB, not a theme or palette index.** This deletes
  the client-side theme-derivation code, which was a second independent guess at
  something the host already knew.

Host side (`src/net/net_snapshot.c`) reads appearance off the entity's **live**
`CSprite` via a new `asset_manager_name_of()` reverse lookup — a path-to-id
inverse of the existing id-to-path map. So the host reports the sprite it is
actually drawing rather than inferring one from the entity kind. An untextured
sprite honestly reports `NET_ART_NONE`, which is how the host says "I really am
drawing a circle here" instead of inventing art to cover it.

Client side (`src/systems/render.c`): the per-kind size table, the hardcoded
colours and the slot lookup are **deleted**. `system_render_mirror` now only
decodes `art` / `size_q` / `tint`. The player's colour comes from the host too —
the client no longer has any appearance opinion at all.

Deleting that code is the actual proof the fix is real. There is no fallback left
to silently disagree with.

## 5. What is tested, and what is not

Tested (`zombie_tests`, 781 assertions at the time, from 719):

- `test_net_art_table` — id → path → id round trip for every id, base pixel size
  sane, and an unknown id degrading to `NET_ART_NONE` rather than to some other
  sprite. This is the part of appearance work CI can genuinely verify: that both
  peers resolve a given id to the same file, and that a missing art entry fails
  visibly instead of quietly drawing the wrong thing.
- `test_net_snapshot_build` — appearance survives the codec, encoded length is
  exactly `NET_HDR_SIZE + 18 + n * 31`, and a headless host correctly reports
  `NET_ART_NONE` instead of claiming art it did not draw.
- `NetEntitySnap` is 36 bytes in memory against 31 on the wire (alignment
  padding). Asserted `>=`, with a comment, so nobody "fixes" the size constant to
  `sizeof()` and silently breaks the stride.

Not tested — the part that actually matters:

- **That the client draws the right sprite.** Every gate above can pass while
  `system_render_mirror` draws garbage. There is no headless test that can catch
  it, by construction.

One thing the gates *did* catch: a `-Werror` sweep (which the normal build does
not run) found dead clamping in the first draft — comparing a `uint8_t` against
255 is a tautology. The clamping now happens in float space.

## 6. What nobody has verified yet

Everything above is code correctness, not visual correctness. Before treating I5
as closed, a human needs two real windows and needs to check:

1. All six entity kinds draw as sprites on the client, not circles.
2. Sizes match the host's, especially zombies across a difficulty change.
3. Player colours match, and each remote player is distinguishable.
4. Pickups (medkit, ammo) draw as their own art, not generic circles.
5. Replacing an art file on one side shows up rather than being papered over.

That list is also in `docs/PENDING_VERIFICATION.md`. If I5 is on a checklist
anywhere as "done", it means **fixed in code**, and those five boxes are what
"done" actually requires.

## 7. If you are about to change any of this

- Do not add a client-side fallback for a missing appearance field. A fallback is
  the original bug. If the host did not send it, draw nothing and log it.
- Adding an art id is a one-line append in `net_art.h` / `net_art.c`. It does not
  need a wire bump, and it should not be given one.
- Any *new* field in the appearance block is a wire change: bump
  `NET_WIRE_VERSION`, update `NET_SNAP_ENTRY_BYTES`, update the history comment
  on the `#define`, and update `test_net_snapshot_build`'s length assertion.
- Keep `docs/VERSIONING.md`'s current-value column honest. It silently advertised
  wire 4 while the code was at 7 for several commits, and that is precisely the
  kind of drift that makes a future bump miss a peer.