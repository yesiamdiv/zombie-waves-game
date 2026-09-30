# Bug Sheet

Complete tracking sheet for every bug found and fixed across the commits of
the **assets worktree** (branch `feature/assets-maps`). This sheet is scoped to
this worktree only; when the branch is merged into `main`, it will be merged
with the bug sheet that `main` carries.

Each bug gets a fix commit; statuses below are for the current HEAD of
`feature/assets-maps`.

## Tracking table

| # | Bug | Fix commit | Status |
|---|-----|------------|--------|
| B1 | Main menu UI overlaps (hardcoded padding) | `19ce2a1` | FIXED |
| B2 | Entity sprites show black where transparent (baked-black alpha) | `f8c95ac` | FIXED |
| B3 | Dropped item sprites too small to notice | `fdb5bc8` | FIXED |
| B4 | HUD damage flash never triggers | `81f3bd3` | FIXED |
| B5 | Ammo pickup does nothing | `da515eb` | FIXED |
| B6 | Camera pans past map edges into empty void | `8967a99` | FIXED |
| B7 | Textures/maps not found when game is launched outside the repo root | `61a0b6b` | FIXED |
| B8 | Tests can't resolve map assets when run via ctest (wrong CWD) | `0520d30` | FIXED |
| B9 | Uninitialized stack `GameWorld` in tests (crash after heap tiles) | `4dc1740` | FIXED |
| B10 | Stone wall generator produced flat red walls | `f8c95ac` | FIXED |
| B11 | World tiles leaked on shutdown (missing `world_free`) | `b9b8987` | FIXED |

## Detailed entries

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
  into separate palette entries, producing flat red walls `(126,0,0)`
  (tracked separately as B10). `asset_manager_get()` also never set a texture
  blend mode, so alpha would not blend even once the PNGs had it.
- **Fix**: `render()` writes RGBA with `(0,0,0,0)` for the `.` background;
  `stone_wall()` derives dark/bright from the full RGB base; every loaded
  texture gets `SDL_BLENDMODE_BLEND`.
- **Regression check**: all 33 PNGs are now RGBA; entity corners are fully
  transparent `(0,0,0,0)`, walls carry true material colors.

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

- **Status**: FIXED (commit `81f3bd3`) — *Sprint 3*
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

- **Status**: FIXED (commit `da515eb`) — *Sprint 3*
- **Symptom**: Running over the yellow ammo pickup has zero effect (pistol has
  unlimited ammo); pickup is a dead action.
- **Investigation**: `ITEM_AMMO` case in `items_check_pickup` (items.c:117-119)
  is an empty branch.
- **Fix**: `items_check_pickup` now takes a `PlayerInventory*`; `ITEM_AMMO`
  refills consumable stocks of owned weapons — +1 grenade when the grenade
  weapon is unlocked, +2 rockets when the launcher is unlocked. Fresh runs
  (pistol only) are a safe no-op.

## B6. Camera shows empty void beyond map edges

- **Status**: FIXED (commit `8967a99`) — *Sprint 3*
- **Symptom**: Near the border walls the camera pans past the map into black
  nothing (most visible on the small snow 34x34 map).
- **Investigation**: `camera_follow` (camera.c:14-17) lerps toward the target
  with no clamp against `GameWorld` bounds.
- **Fix**: Added `camera_clamp_world(cam, world_w, world_h)` — clamps the camera
  center to `[half-viewport, world-size - half-viewport]`; when the world fits
  inside the viewport it is kept centered instead. Wired into `main.c` right
  after `camera_follow` each playing frame.

## B7. Textures/maps not found when launched outside the repo root

- **Status**: FIXED (commit `61a0b6b`)
- **Symptom**: Running the game from a menu shortcut, double-click, or any CWD
  other than the repo root renders no textures and loads no maps.
- **Root cause (confirmed)**: `asset_manager` resolved via `SDL_GetBasePath()`
  then CWD — both pointed away from `assets/` unless launched from the repo
  root.
- **Fix**: CMake copies `assets/` next to the game binary in the build target
  dir, so `SDL_GetBasePath()` resolves textures and maps regardless of the
  launch directory.

## B8. Tests can't resolve map assets when run via ctest

- **Status**: FIXED (commit `0520d30`)
- **Symptom**: `ctest` ran `zombie_tests` from the build dir with the working
  directory set to the build dir, so `world_load_map()` failed to find
  `assets/maps/*.map` and map tests failed/fell back unexpectedly.
- **Fix**: ctest now runs `zombie_tests` from the source dir so map assets
  resolve.

## B9. Uninitialized stack GameWorld in tests

- **Status**: FIXED (commit `4dc1740`)
- **Symptom**: Tests that declared `GameWorld world;` on the stack crashed or
  misread fields after the tile grid became heap-allocated.
- **Root cause (confirmed)**: The tile grid is now `world.width x world.height`
  heap memory; uninitialized stack `GameWorld`s carried garbage `width/height`,
  so `world_init` wrote/read out of bounds.
- **Fix**: All stack `GameWorld` declarations in tests are zero-initialized
  (`GameWorld world = {0};`).

## B10. Stone wall generator produced flat red walls

- **Status**: FIXED (commit `f8c95ac`)
- **Symptom**: Stone wall art rendered as a single flat red `(126,0,0)` with no
  dark/bright shading.
- **Root cause (confirmed)**: `stone_wall()` in `tools/gen_assets.py` unpacked
  the base color's channels into separate palette entries instead of deriving
  variants from the full RGB base.
- **Fix**: `stone_wall()` derives dark/bright variants from the full base RGB;
  walls carry true material colors.

## B11. World tiles leaked on shutdown

- **Status**: FIXED (commit `b9b8987`)
- **Symptom**: With heap-allocated tile grids, repeated runs leaked the world's
  tile buffers on shutdown.
- **Root cause (confirmed)**: No cleanup path existed for the heap tile grid.
- **Fix**: `world_free()` frees tile memory; wired into game shutdown.