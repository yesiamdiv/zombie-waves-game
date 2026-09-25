# Asset & Map System Plan

Status: approved, branch `feature/assets-maps`

## Goal
Replace all primitive-shape rendering with real textures and support multiple
maps that differ in size, terrain theme, and entity palette. All art is
runtime-loadable pixel art so any asset can be swapped without code changes.

## Decisions (confirmed)
- Art: pixel-art PNGs generated with a Python/PIL tool (`tools/gen_assets.py`)
  from 2D pixel grids, committed under `assets/textures/`. Replaceable later.
- Maps: ASCII `.map` files under `assets/maps/` (arbitrary sizes).
- Textures: load via `SDL_image` added through `FetchContent`.
- Map selection: a dedicated map-select screen off the main menu (the "Map:"
  inline-cycling design was replaced in implementation with a full screen that
  lists maps and previews the highlighted one).

## Asset pipeline
- Add `SDL_image` to CMakeLists (`IMG_LoadTexture`).
- `src/assets/asset_manager.{h,c}`: lazy keyed texture cache.
  - `asset_init(renderer)`, `asset_shutdown()`, `asset_get(name)`.
  - Path resolution: `SDL_GetBasePath()/assets/...` then CWD `assets/...`.
  - No-op (returns NULL shapes) in headless builds.
- `tools/gen_assets.py`: defines each texture as a list of hex-color rows,
  scales to final size, writes `assets/textures/<name>.png`. One source of
  truth for placeholder art; regenerated + committed.

## Themes (`src/world/theme.{h,c}`)
- `ThemeID` (GRASSLAND, DESERT, SNOW, CITY) + `Theme` struct holding per-tile
  texture keys (ground variants, wall, water, road), grid/accent colors, and
  entity tints (player color, zombie variant tints).
- Static registry: `theme_count()`, `theme_get(id)`, `theme_name(id)`.

## Sprites (`src/graphics/sprite.{h,c}`)
- Extend `Sprite` with a texture + source rect; implement the existing
  `SPRITE_SHAPE_TEXTURE` path via `SDL_RenderTextureRotated` (pivot, rotation,
  flip).
- Small pickups/particles may keep colored circles until themes demand more.

## Worlds / maps (`src/world/world.{h,c}`)
- `GameWorld` gains a `ThemeID` and a precomputed spawn tile.
- `world_load_map(world, const MapDef*)`; `MapDef { name, file, theme }`.
  ASCII chars: `.` ground, `#` wall, `~` water, `+` road, `S` spawn marker
  (fallback: center walkable search).
- `world_draw` draws theme tile textures (grid-culled like today), alternating
  the three ground variants to break tiling repetition, with a subtle grid
  overlay drawn in the theme grid color.
- `world_init()` kept as the default-map wrapper so existing tests pass.

## Map registry (`src/world/map_registry.{h,c}`)
- Static table: grass 46x34, desert 60x52, snow 34x34, city 50x60. Layouts
  live in `.map` files; the registry maps display name + file + theme.
- Exposes `map_registry_count()`, `map_registry_get(index)`.

## UI (`src/ui/menu.c`)
- Main menu gains a "Start Game" entry that opens a dedicated **Map Select**
  screen (`GAME_STATE_MAP_SELECT`): lists the registered maps (W/S or arrows,
  ENTER plays, ESC backs out), shows the highlighted map as a mini-map preview
  panel + its theme/size. `reset_game()` picks the active map.

## Entities
- Player, zombies, bullet, grenade, rocket, medkit, ammo, speed boost, sword
  blade all switch from colored shapes to texture sprites. Zombie variants use
  different source rects/tints resolved through the theme.

## Tests
- Map parser unit tests (layout->tile parity, spawn from `S`, various sizes
  set world_pixel_w/h correctly), theme-registry sanity, walkable parity.
- Keep existing suite green (137 passing).

## Phases
1. SDL_image + AssetManager + sprite texture path.
2. Themes + textured tile rendering.
3. Map parser + registry + `world_load_map` + spawn marker.
4. Menu map picker + `reset_game` wiring (implemented as the map-select screen).
5. Entity textures (py art tool) + replaceable art.
6. Tests, EXTENDING.md docs, build verification.

## Worktree
`git worktree add .../open-world-zombie-waves-assets -b feature/assets-maps`