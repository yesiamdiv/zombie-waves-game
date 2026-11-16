# Effects Plan — E1–E6

Owner: agent `effects` (Effects Developer). Worktree `worktrees/effects`,
branch `feature/effects`.

No document in the repo defined E1–E6; the corp role blurb named five effects
for six IDs. The breakdown below was proposed to PM on 2026-11-04 and confirmed
the same day (`msg_01a1a6dad7417799a37639da`). It is recorded here so the
mapping survives the chat.

## Scope

| ID | What | Status |
|---|---|---|
| E1 | Shared text popup system — world-space, camera-aware, TTF, private RNG | planned |
| E2 | Damage numbers on zombie hits | planned |
| E3 | Points / pickup popups (kills, item pickups) | planned |
| E4 | Muzzle flash on weapon fire | planned |
| E5 | Hit flash (zombie tint on hit) | planned |
| E6 | Wave transition banner | planned |

E1 is the foundation: E2 and E3 are consumers of it, so it lands first.

## Binding constraints

Every item here is **cosmetic and client-safe**. Restating what
`docs/SYSTEM_DESIGN_RULES.md` and `docs/VERSIONING.md` already require, in the
specific form it takes for effects work:

1. **No wire change.** Nothing in E1–E6 adds a field to a packet, so
   `NET_WIRE_VERSION` and `NET_WORLD_GEN_VERSION` are not bumped by this work.
   An effect that would need new host data is out of scope until that is
   designed separately.
2. **No sim change.** Effects read simulation state and never write it.
3. **No draw from the global RNG stream.** See the decision below — this is the
   one that already had a live bug in it.
4. **Unproven until seen.** The gate is headless and `render()` early-returns
   when `game.headless` is set, so a green gate says nothing about whether any
   of E1–E6 is visible or correct on screen. Rows above move to `fixed` only
   after the owner has looked; until then they read `unproven`
   (`docs/SYSTEM_DESIGN_RULES.md` §4).

## Decision: cosmetic randomness uses a private RNG, never `rand()`

The `text_popup.c` seeded into this lane called global `rand()` to pick the
spread angle for `TEXT_POPUP_RANDOM`. The simulation draws from that same
global stream:

- `src/world/waves.c` — spawn angle, spawn ring radius, zombie variant, zombie
  size, spawn-point index, spawn jitter
- `src/items/items.c` — item type roll
- `src/ai/bot.c` — strafe direction and timer

So one cosmetic `rand()` call shifts every later simulation draw. Two runs at
the same seed diverge as soon as a popup spawns on a code path that also runs
headless, which is exactly the single-player byte-identity gate failing — and
failing for a reason that looks like it could not possibly matter.

**Decision:** the popup system owns a private 32-bit LCG. It never calls
`rand()`, never calls `srand()`, and its state is reset by
`text_popup_init()`. Cosmetic randomness is therefore invisible to the sim by
construction rather than by discipline.

Note for the record: `src/systems/render.c` (~line 276 and 292–298) already
calls `rand()` on the render path. The headless gate cannot see it because
`render()` returns early, so GUI and headless runs diverge there by design.
That is pre-existing and outside this lane's scope; it is noted so nobody reads
it as precedent for new cosmetic `rand()` calls.

## Coordination

- **assets** owns the HUD/damage-feedback track. To avoid parallel `hud.c`
  edits, E1 lives entirely in `src/ui/text_popup.*` and is called from
  `src/main.c`'s render path, not from inside `hud_draw`.
- **audio** owns effect triggers; the same gameplay events that spawn a popup
  are the ones audio hooks, so trigger sites are shared and must be agreed
  before either side adds one.
