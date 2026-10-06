# Future Ideas & Features Backlog

Owner: PM | Triage date: 2026-09-29 | Board: `docs/SPRINT_PLAN.md`

Ideas that are not yet ticket-sized or are parked for later sprints. Small
commits per item; each gets a `docs/PROGRESS.md` entry when started. Ideas
listed here are *candidates* — nothing is committed until it's picked into a
sprint.

## UI / feel polish

- **Muzzle flash / impact sparks** — short-lived starburst entity on gunfire +
  spark particles on hits. New 15th sprite in `gen_assets.py`.
- **Screen shake** — small camera trauma on grenade blasts and player damage
  (Camera has no trauma field yet; add one in `world/camera.h`).
- **Floating damage numbers** — small popup text at hit locations so hits read
  clearly (note: `draw_text` re-creates an SDL surface/tex per frame in
  `src/ui/hud.c` — worth caching if used heavily).
- **Item bob** — pickups already carry `bob_timer` in `CItemTag` but nothing
  animates it; add gentle bob/glow to draw attention.
- **Muzzle/aim indicator** — subtle line or dot showing the exact shot origin so
  aiming near walls is legible.
- **Zombie separation** — keep overlapping zombies from stacking into one blob
  (cheap steering while still chasing the player).

## Assets

- More themed wall/ground/water texture variations per theme (currently 3 ground
  variants, 1 wall/water/road each).
- Spawn-ring visual (brief circle at spawn points when a wave begins).
- Low-health vignette overlay.
- Death/explosion screen flash on game-over.

## Gameplay / systems

- Score/other stats on the map-select screen (no persistent storage yet).
- Longer-term maps or procedural map generation (maps are static files today).
- Balancing: `ITEM_AMMO` currently does nothing (pistol has unlimited ammo);
  could grant grenades/rockets when those weapons are owned.
- Pickup magnet (items drift toward player within a radius).

## Engineering (deferred)

- Cache text surfaces in the HUD/menus (every `draw_text*` makes a new
  surface+texture per frame).
- Cap `item_spawn_timer`-based free item spawns per wave.
- **Health-bar render guards** (from the 2026-10-05 UI audit — checked, found
  unreachable, deliberately *not* fixed then).
  `src/ui/hud.c` clamps `ratio` below but not above, and the local branch reads
  `hp->max` with no `> 0` guard while the `use_auth` branch has one. Nothing
  reaches either today: `hp->max` is never assigned after init, and every write
  to `hp->current` clamps (`src/items/items.c:110`, `src/world/waves.c:252`) or
  derives from a percentage the CLI already clamps to 0-100. Worth doing anyway —
  the asymmetry between the two branches means a future `use_auth`-only change
  can't protect the other one. Cheap: `hp_max > 0 ? : 1.0f` and `if (ratio > 1)
  ratio = 1;`, plus a test that `hp_max == 0` doesn't produce NaN.
- **Read slot 0's beacon from the snapshot on a client** (2026-10-05 audit).
  `sync_client_beacon_slots()` (`src/main.c:302`) skips slot 0 because it holds
  a "real local entity", leaving `beacon_pos` at the client's own spawn guess
  instead of reading it from the snapshot like every other slot. It agrees today
  only because `beacon_pos` is written exactly once, at respawn
  (`src/players.c:75`), and never moved — so the skip is load-bearing on a
  property nothing enforces, and slot 0 silently diverges the day beacons become
  movable. Drop the `continue` and let it read from the mirror like the rest.
- **Bounds guard in the `ecs.h` component accessors** (same audit, same
  reasoning). All of them — `ecs_get_health`, `ecs_get_position`,
  `ecs_get_sprite`, and the rest — do
  `&w-><array>[ecs_get_entity_index(w, e)]` with no range check of their own.
  `ecs_get_entity_index` *does* check (`src/ecs/ecs.c:73`) and returns
  `ECS_MAX_ENTITIES` for an out-of-range entity — but the arrays are
  `ECS_MAX_ENTITIES` long, so index 2048 is itself one past the end. The guard
  hands back exactly the invalid value it should have prevented, and neither the
  accessors nor any caller compare against it. So the one place that could make
  this safe currently guarantees the unsafe access. Unreachable from `hud_draw`
  only because an earlier `if (!alive) return;` happens to catch it; that
  protects the accessor from nowhere, and every other caller gets no such
  protection.

  **Scope it before picking it up — it is not one line:**
  - The accessors are `static inline` in the header with ~10 callers that
    dereference the result immediately (`*ecs_get_health(ecs, e) = ...`,
    `hp->current += ...`). Making them return NULL would mean a null check at
    every call site; that is the real work, and it changes the module's contract.
  - The fix belongs in `ecs_get_entity_index` and applies to *all* accessors at
    once, so it cannot be done for health alone without leaving the identical
    hole open in position/sprite/collider/velocity.

  The cheap half that is still worth taking: change that single return from
  `ECS_MAX_ENTITIES` to `ECS_MAX_ENTITIES - 1` so the access is in bounds by
  construction, and assert in debug builds when it clamps. Fixes the
  memory-unsafety without touching a caller. Tradeoff: a null entity then
  silently reads a live component instead of crashing, so the assert is what
  makes it debuggable.

## Zombie speed variants (lowest priority — after N1–N4)

Requested 2026-10-03: **make some zombies fast and some slow, sized accordingly**
— e.g. small zombies quick, large zombies slow. Logged last in the queue on
purpose.

Two things to settle before anyone starts this, both recorded now so they are not
discovered halfway through:

1. **This is a single-player gameplay change.** `feature/net-protocol` exists to
   add multiplayer *without* changing single-player, and this alters how every
   wave plays for everyone. It probably belongs on `main` as a gameplay feature,
   or behind an explicit opt-in, rather than riding along on a net branch.
2. **Size is currently random (`10 + rand()%6`) and already replicated.** Sprint
   N1 puts size on the wire as `size_q`, so once speed keys off size, the client
   needs no new replication to animate or reason about it — but `speed` itself
   becomes gameplay state that must *not* be re-derived per client if it ever
   affects anything the client displays.

Acceptance when it is picked up: still byte-identical for a fixed seed, wave 1
still `8 zombies, interval: 1.90s, difficulty: 1.00`, and the size→speed mapping
written down as a table rather than a formula buried in a spawn function.
