# Bug Sheet

Status tracking for gameplay/UI bugs reported during playtesting. Each bug
gets a fix commit; this file records the symptom, root cause, and resolution.

## B1. Main menu UI overlaps (padding)

- **Status**: OPEN
- **Symptom**: Text/buttons on the main screen overlap each other; elements
  need padding on all sides.
- **Investigation**: `menu_draw` places title at `0.20h`, subtitle at `0.30h`,
  options at `0.50h` spacing 50px, hints at `0.85h`/`0.90h` — tight/fixed
  positions that collide at some window sizes. Map-select rows (42px) also
  crowd the detail line every row.
- **Fix plan**: Give each screen a layout box with real padding: title block,
  centered option column with generous row height, bottom hint block; use
  per-screen macros so numbers can't drift apart.

## B2. Entity sprites show black where transparent

- **Status**: OPEN
- **Symptom**: In-game sprites render black in the alpha-transparent regions
  (bad alpha channel).
- **Root cause (confirmed)**: `tools/gen_assets.py` writes `Image.new("RGB",
  ...)` for all art, and background pixels are palette `(0,0,0)` *opaque
  black* — so the transparent background is baked in as black. Additionally,
  `asset_manager_get()` never sets a texture blend mode, so RGBA art (once
  fixed) still won't blend.
- **Fix plan**: Generate entity art as RGBA with `(0,0,0,0)` for the `.`
  background key; set `SDL_SetTextureBlendMode(..., SDL_BLENDMODE_BLEND)` on
  every loaded texture; regenerate + commit the PNGs.

## B3. Dropped item sprites too small

- **Status**: OPEN
- **Symptom**: Health/ammo/speed pickups are barely visible; too small to
  notice (only visible due to the black alpha box from B2).
- **Root cause (confirmed)**: `items_spawn()` uses 32px art with
  `.scale = 0.25f` (renders as 8 world units) — tiny next to the 16-unit
  player. Collider stays 10 units.
- **Fix plan**: Render pickups at ~16 world units (scale ~0.5) matching the
  player, keep the generous pickup collider.