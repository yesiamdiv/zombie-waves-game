# Bug Sheet

Status tracking for gameplay/UI bugs reported during playtesting. Each bug
gets a fix commit; this file records the symptom, root cause, and resolution.

## B1. Main menu UI overlaps (padding)

- **Status**: FIXED (commit `19ce2a1`)
- **Symptom**: Text/buttons on the main screen overlap each other; elements
  need padding on all sides.
- **Investigation**: `menu_draw` placed title at `0.20h`, subtitle at `0.30h`,
  options at `0.50h` with fixed 50px spacing, hints at `0.85h`/`0.90h` —
  fixed positions that collide at some window sizes. Map-select rows (42px)
  crowded the detail line against the next row.
- **Fix**: All menu draws now measure `TTF_GetFontHeight` and lay out rows
  relative to it: title/subtitle blocks, vertically-centered option blocks,
  bottom-anchored hint stacks. Main menu, map select, pause, shop, and
  game-over all use the same spacing rhythm so nothing overlaps at any window
  size.

## B2. Entity sprites show black where transparent

- **Status**: FIXED (commit `f8c95ac`)
- **Symptom**: In-game sprites render black in the alpha-transparent regions
  (bad alpha channel).
- **Root cause (confirmed)**: `tools/gen_assets.py` wrote `Image.new("RGB",
  ...)` for all art, and background pixels were palette `(0,0,0)` *opaque
  black* — the transparent background was baked in as black. Also found a
  latent generator bug: `stone_wall()` unpacked the base color's channels
  into separate palette entries, producing flat red walls (`(126,0,0)`).
  `asset_manager_get()` also never set a texture blend mode, so alpha would
  not blend even once the PNGs had it.
- **Fix**: `render()` writes RGBA with `(0,0,0,0)` for the `.` background;
  `stone_wall()` derives dark/bright from the full RGB base; every loaded
  texture gets `SDL_BLENDMODE_BLEND`.
- **Regression check**: all 33 PNGs are now RGBA; entity corners are fully
  transparent (`(0,0,0,0)`), walls carry true material colors.

## B3. Dropped item sprites too small

- **Status**: FIXED (commit `fdb5bc8`)
- **Symptom**: Health/ammo/speed pickups are barely visible; too small to
  notice (only visible due to the black alpha box from B2).
- **Root cause (confirmed)**: `items_spawn()` used 32px art with
  `.scale = 0.25f` (renders as 8 world units) — tiny next to the 16-unit
  player.
- **Fix**: Pickups render at `.scale = 0.5f` (16 world units), matching the
  player's footprint. Collider left at 10 units (generous pickup radius).

## B4. HUD damage flash never triggers

- **Status**: FIXED (commit `878988c`) — *Sprint 3*
- **Symptom**: The red hurt-flash overlay is dead — the screen gives no visual
  feedback when the player takes damage.
- **Investigation**: `hud->damage_flash` is only initialized to 0, decremented
  in `hud_update`, and *drawn* in `hud_draw` (hud.c:8,15-17,189-192). Nothing
  ever sets it to 1.0 when HP drops, so the overlay code path is unreachable.
- **Fix**: Added `hud_track_player_hp()` — the HUD records HP each frame and
  arms the flash on any HP *drop* (first call establishes the baseline without
  flashing; medkit healing raises HP so it never false-triggers). `hud_draw`
  now calls it with the player's current HP each frame.

## B5. Ammo pickup does nothing

- **Status**: OPEN (Sprint 3 — see `docs/SPRINT_PLAN.md`)
- **Symptom**: Running over the yellow ammo pickup has zero effect (pistol has
  unlimited ammo); pickup is a dead action.
- **Investigation**: `ITEM_AMMO` case in `items_check_pickup` (items.c:117-119)
  is an empty branch.
- **Fix**: TBD (plan: grant grenades/rockets when the corresponding weapon is
  unlocked).

## B6. Camera shows empty void beyond map edges

- **Status**: OPEN (Sprint 3 — see `docs/SPRINT_PLAN.md`)
- **Symptom**: Near the border walls the camera pans past the map into black
  nothing (most visible on the small snow 34x34 map).
- **Investigation**: `camera_follow` (camera.c:14-17) lerps toward the target
  with no clamp against `GameWorld` bounds.
- **Fix**: TBD (plan: clamp camera center to world bounds minus half-viewport;
  skip clamp when the viewport exceeds the world).