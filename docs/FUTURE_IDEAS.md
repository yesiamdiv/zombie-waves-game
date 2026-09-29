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
- Camera world-bounds clamp so the void beyond map edges isn't visible
  (esp. small maps like snow 34x34).