# Feature Sheet

Every feature added across the commits of the **assets worktree** (branch
`feature/assets-maps`). Scoped to this worktree only; when the branch is merged
into `main` this sheet is merged with whatever `main` carries.

## Tracking table

| # | Feature | Feature commit | Status |
|---|---------|----------------|--------|
| F1 | Asset manager + texture sprite path (`SDL_image`) | `e7f91da` | SHIPPED |
| F2 | Theme system + textured tile rendering | `7165c18` | SHIPPED |
| F3 | ASCII map files, parser, multi-size map registry | `4dc1740` | SHIPPED |
| F4 | Map picker on the main menu | `b9b8987` | SHIPPED |
| F5 | Textured entities (player, zombies, weapons, pickups) | `40057bd` | SHIPPED |
| F6 | Map parser/registry tests + asset authoring docs | `0520d30` | SHIPPED |
| F7 | Assets shipped next to the game binary | `61a0b6b` | SHIPPED |
| F8 | Dedicated map-select screen with mini-map preview | `c005d95` | SHIPPED |
| F9 | Entity tints resolved through the theme | `f91b13f` | SHIPPED |
| F10 | HUD damage flash on player hurt | `81f3bd3` | SHIPPED |
| F11 | Ammo pickup refills owned consumables | `da515eb` | SHIPPED |
| F12 | Camera clamped to world bounds | `8967a99` | SHIPPED |

## Detailed entries

## F1. Asset manager + texture sprite path

- **Status**: SHIPPED (commit `e7f91da`)
- Adds `SDL_image` via FetchContent; `zombie_core` links `SDL3_image`.
- New `src/assets/asset_manager`: lazy-keyed texture cache resolved via
  `SDL_GetBasePath()` then CWD, safe no-op in headless builds.
- Sprite gains texture + source-rect support (`SPRITE_SHAPE_TEXTURE`) rendered
  by `SDL_RenderTextureRotated` with optional tint/alpha; new
  `sprite_texture()` / `sprite_texture_rect()` constructors.
- Asset manager init/shutdown wired into the game lifecycle.

## F2. Theme system + textured tile rendering

- **Status**: SHIPPED (commit `7165c18`)
- New `src/world/theme`: `ThemeID` enum (`GRASSLAND`/`DESERT`/`SNOW`/`CITY`)
  with per-tile texture paths and fallback colors for all four tile types
  (ground variants, wall, water, road).
- `world_draw` renders tile textures via the asset manager (culling like
  before), falling back to color fill when textures are missing or headless.
  Subtle per-theme grid overlay kept for navigation.
- `gen_assets.py` PIL tool generates 24 tile PNGs from explicit 16x16 pixel
  grids; artist-replaceable 1:1 via same filenames.
- `asset_manager_global()` accessor so systems resolve textures at draw time
  without threading pointers everywhere.
- `GameWorld` gains a `theme` field (default grassland).

## F3. ASCII map files, parser, multi-size map registry

- **Status**: SHIPPED (commit `4dc1740`)
- `GameWorld` tile grid is now heap-allocated (width x height); default stays
  50x50.
- `world_init_from_string()` parses ASCII maps (`. ground`, `# wall`,
  `~ water`, `+ road`, `S spawn`); `world_load_map()` reads `assets/maps/*.map`
  via `asset_path`. Spawn falls back to a center-walkable search when no `S`.
- `map_registry` defines 4 selectable maps with distinct themes/sizes:
  Grassland 46x34, Desert 60x52, Snow 34x34, City 50x60.

## F4. Map picker on the main menu

- **Status**: SHIPPED (commit `b9b8987`)
- Main menu gains a third line: `Map: <name> <-- / -->` cycled with left/right
  or A/D; selection stored on `MainMenu.selected_map`.
- `reset_game()` loads the selected map from the registry in windowed play
  (falling back to the classic world on load failure). Headless runs keep the
  fixed 50x50 classic world so scripts/tests stay layout-stable.

## F5. Textured entities

- **Status**: SHIPPED (commit `40057bd`)
- `gen_assets.py` emits entity pixel art (player, zombie, bullet, grenade,
  rocket, medkit, ammo, speed, sword_blade) tintable at runtime via color mod.
- `sprite_tex()` resolves textures through the global asset manager; assets use
  `NEAREST` scaling so pixel art stays crisp.
- All entity creation sites switch from colored shapes to textured sprites at
  matching world sizes: player 16, zombies `2*radius` tinted per variant,
  bullet 8 (yellow), grenade 12, rocket 10, pickups 8.
- `sprite_draw_blade` gains an optional texture so the sweeping sword is drawn
  with blade art pinned at the handle; still fills with color when
  art/headless.
- Effects (death particles, grenade fragments) intentionally stay circles.

## F6. Map parser/registry tests + asset authoring docs

- **Status**: SHIPPED (commit `0520d30`)
- Tests: ASCII parser (tiles, spawn coords, pixel dims, out-of-bounds wall) and
  ship-map registry checks (all maps load, sizes, theme, walkable spawn).
- ctest runs `zombie_tests` from the source dir so map assets resolve.
- `docs/EXTENDING.md` documents the texture/asset system, replacing art, and
  authoring new ASCII maps + themes.

## F7. Assets shipped next to the game binary

- **Status**: SHIPPED (commit `61a0b6b`)
- Copies `assets/` into the target dir so `SDL_GetBasePath()` resolves textures
  and maps no matter which directory the game is launched from (menu shortcut,
  double-click, etc.), not just the repo root.

## F8. Dedicated map-select screen with mini-map preview

- **Status**: SHIPPED (commit `c005d95`)
- Start Game now opens `GAME_STATE_MAP_SELECT`: lists each registered map (W/S
  or arrows to highlight, ENTER to play, ESC back), shows theme/size detail
  under the selection, and renders a tile-color mini-map preview of the
  highlighted map in a right-hand panel.
- `reset_game()` uses the map selected on the screen (headless runs keep the
  fixed classic world).
- Replaces the inline arrow-key map cycling on the main menu.

## F9. Entity tints resolved through the theme

- **Status**: SHIPPED (commit `f91b13f`)
- Completes the plan's Theme spec: `player_color` + `zombie_tints[3]` live in
  the registry instead of being hardcoded in spawn sites.
- Each environment defines its own palette.
- `waves_spawn_zombie` takes the `ThemeID` and picks zombie tint from the
  theme registry, keeping the same `rand()` cadence.
- Player sprite tint comes from the active map's theme.
- Adds the missing phase-6 theme-registry sanity test (60 checks).

## F10. HUD damage flash on player hurt

- **Status**: SHIPPED (commit `81f3bd3`)
- `hud_track_player_hp()` records HP each frame and arms the red flash on any
  HP drop; first call sets the baseline (no flash), healing never
  false-triggers. `hud_draw` calls it every frame.
- Covered by `test_hud_damage_flash`.

## F11. Ammo pickup refills owned consumables

- **Status**: SHIPPED (commit `da515eb`)
- `items_check_pickup()` takes a `PlayerInventory*`; `ITEM_AMMO` grants +1
  grenade when grenades are unlocked and +2 rockets when the launcher is
  unlocked; pistol-only fresh runs stay a safe no-op.
- Covered by `test_item_ammo_pickup`.

## F12. Camera clamped to world bounds

- **Status**: SHIPPED (commit `8967a99`)
- `camera_clamp_world()` clamps the camera center to
  `[half-viewport, world-size - half-viewport]`; when the world fits inside the
  viewport it is kept centered. Called after `camera_follow` in `main.c`.
- Covered by `test_camera_clamp_world`.