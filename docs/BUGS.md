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
| B12 | Cross-branch merge silently dropped seven shipped features | `eda5cd9` | FIXED |
| B13 | Joining client built the wrong world | `eda5cd9` | FIXED |
| B14 | Asset manager never initialized → all sprites/tiles render as flat shapes | `8824f73` | FIXED |
| B15 | Zombie sprite lost → zombies render as flat circles | `f6ba8e1` | FIXED |
| B16 | Bullet sprite lost → bullets render as flat circles | `f6ba8e1` | FIXED |
| B17 | Grenade sprite lost → grenades render as flat circles | `f6ba8e1` | FIXED |
| B18 | Rocket sprite lost → rockets render as flat circles | `f6ba8e1` | FIXED |
| B19 | Clients render entities as flat circles, not sprites (R13-I5) | — | PLANNED |
| B20 | Zombie size and colour variant never replicated | — | PLANNED |
| B21 | Pickup subtype never replicated | — | PLANNED |
| B22 | Client HUD health reads the client's own stale ECS | — | PLANNED |
| B23 | Client HUD points/weapon/ammo read the client's own stale inventory | — | PLANNED |

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
## B12. Cross-branch merge silently dropped seven shipped features

- **Status**: FIXED (commit `eda5cd9`)
- **Symptom**: No build error and no git conflict. `feature/multiplayer` had
  been developed against an older `main`, so features simply were not present:
  zombie melee contact dealt no damage and no movement slow, the sword hit a
  single point instead of sweeping a blade, and the game aborted with
  `free(): invalid pointer` in the test suite.
- **Root cause (confirmed)**: git only reports overlapping *edits*. Where
  `feature/multiplayer` had reverted a file to an older state and
  `feature/assets-maps` had not touched it, the merge silently kept the revert.
  `CROSS_BRANCH_CONFLICTS.md` predicted 3 such traps; there were 7. Six were
  assets features; the seventh was **`main`'s own** behaviour
  (`CPlayerTag.slow_timer`, `zombie_damage_player()`, `PLAYER_HURT_SLOW_*`,
  zombie-contact damage, `CZombieTag.sword_hit_timer`, `CSwordTag.outer_radius`,
  `vec2_closest_on_segment()`, blade renderer), so leaving it out would have
  regressed `main` rather than the assets branch.
- **Fix**: restored all seven, keeping the multiplayer behaviour:
  `zombie_damage_player()` takes its target explicitly so the "damage the player
  it actually chased" fix survives alongside main's knockback and slow; the
  sword regained per-zombie hit cooldowns so one sweep can slash several; the
  blade sweeps inner→outer radius with a segment test. Separately,
  `GameWorld` owns a heap `tiles` buffer and must be zero-initialized before
  `world_init()`/`world_free()` — the multiplayer tests had dropped the `= {0}`
  and aborted once the heap world was restored. See `MERGE_DECISIONS.md`.

## B13. Joining client built the wrong world

- **Status**: FIXED (commit `eda5cd9`)
- **Symptom**: With the map-select screen added, a joining client built whichever
  map its own never-shown picker happened to hold. If the host had chosen a
  different map, the two windows silently disagreed about terrain, spawn point
  and theme.
- **Root cause (confirmed)**: the world identity sent in `NET_PKT_HELLO` was a
  seed and a world-generation version — no map index — so the client had no way
  to learn the host's choice.
- **Fix**: `NET_PKT_HELLO` carries `uint8_t map_index` after `world_gen`
  (`NET_WIRE_VERSION` 2→3, `NET_WORLD_GEN_VERSION` 1→2). `reset_game()` loads
  the host's map when one was received; headless runs still use the fixed
  classic world so the determinism gate holds. An out-of-range index is
  **rejected** rather than falling back to a default world, which would keep the
  desync. New `--map=N` flag presets the map-select index so this is testable
  without a GUI.

## B14. Asset manager never initialized — all art renders as flat colored shapes

- **Status**: FIXED (commit `8824f73`)
- **Symptom**: In a windowed run every entity and tile renders as a plain
  colored shape — no player/zombie/pickup sprites, no tile textures. Looks like
  the pre-assets build.
- **Root cause (confirmed)**: The cross-branch merge (`eda5cd9`) dropped the
  asset-manager wiring from `src/main.c` while keeping the asset code that
  consumes it:
    - no `#include "assets/asset_manager.h"`
    - no `AssetManager assets;` field in `Game`
    - no `asset_manager_init(&game.assets, game.renderer)` after renderer
      creation
    - no `asset_manager_shutdown()` before renderer teardown
  With `g_assets == NULL`, `sprite_tex()` (sprite.c:6-7) and `world_draw()`
  (world.c:192) get a NULL manager; `asset_manager_get()` returns NULL
  **silently** (its early-out for `!am->renderer` logs nothing), so every draw
  site falls back to its flat-color path. `--headless` was unaffected because
  it never initializes a renderer, which is why all automated gates stayed green.
- **Fix**: Restored all four pieces in `src/main.c`, matching the assets
  worktree. Verified with a temporary probe that `player.png`, `zombie.png`,
  `city_ground0.png`, `city_wall.png` and `snow_wall.png` all load (LOADED, not
  NULL), then removed the probe.
- **Why the gates missed it**: headless render() returns immediately, so no
  texture is ever requested in CI. This is the known blind spot in
  `PENDING_VERIFICATION.md`; a windowed screenshot is the real regression test.

## B15–B18. Zombie / bullet / grenade / rocket sprites lost in the merge

- **Status**: FIXED (commit `f6ba8e1`)
- **Symptom**: Even after B14 (asset manager restored), four entity types still
  rendered as plain colored circles: zombies, bullets, grenades and rockets.
  Player, pickups and the sword blade were textured correctly.
- **Root cause (confirmed)**: The cross-branch merge `eda5cd9` resolved
  `waves.c`, `player_input.c`, `grenades.c` and `rockets.c` toward the
  multiplayer versions, which had reverted the sprite code to `sprite_circle()`
  for determinism — but unlike `players.c` (explicitly annotated as an R13
  adaptation), these four got **no** re-texture patch. Git reported no conflict,
  so the loss was silent.
  - `waves.c` — zombie spawned `sprite_circle(size, color)`; assets used
    `sprite_texture(zombie_tex)` with a theme tint at `scale = size/16`.
  - `player_input.c` — bullet spawned `sprite_circle(4, yellow)`; assets used
    the 8px `bullet.png` at `scale = 1.0`.
  - `grenades.c` — grenade spawned `sprite_circle(GRENADE_RADIUS, ...)`; assets
    used `grenade.png` at `scale = 0.375` (12 world units).
  - `rockets.c` — rocket spawned `sprite_circle(ROCKET_RADIUS, ...)`; assets
    used `rocket.png` at `scale = 0.3125` (10 world units).
- **Fix**: Restored the texture path in all four call sites, keeping each
  entity's flat-color fallback so headless runs and missing art still render.
  Scales are set so each sprite keeps its previous on-screen diameter, and the
  zombie keeps its per-variant theme tint.
- **Determinism**: `sprite_tex()`/`sprite_texture()` consume no RNG, and the
  `rand()` call order around each spawn is unchanged — the single-player
  determinism gate stays byte-identical.

## B19–B23. Host-authority violations found in the 2026-10-03 audit

Found while planning the net-protocol sprints, not during a playtest. All five
are the same class of defect: **the client supplies a value the host owns**, or
**the client displays its own state instead of the host's**. Planning and
rationale: `docs/NET_PROTOCOL_DESIGN.md`; work: `docs/NET_SPRINT_PLAN.md`.

### B19. Clients render entities as flat circles instead of sprites (R13-I5)
- **Symptom**: the hosting player sees textured zombies, bullets, grenades and
  rockets. A player who joined sees coloured dots for everything, including
  their own character.
- **Root cause**: `system_render_mirror()` (`src/systems/render.c:97`) is a
  second, independent renderer used for every client frame via `render_only_client()`
  (`src/main.c:1051`). `NetEntitySnap` carries only `{id, kind, pos, vel, hp,
  flags, owner}` — never an appearance — so the mirror hardcodes a size per kind
  and calls `sprite_circle()` at `render.c:152`.
- **Fix**: transmit appearance (`art`, `size_q`, `tint[3]`) in the snapshot and
  have the mirror reproduce the host's sprite verbatim. Sprint N1.
- **Status**: PLANNED.

### B20. Zombie size and colour variant are unreplicated
- **Symptom**: even after B19, a client cannot match the host's zombies.
- **Root cause**: zombie size is `10 + rand()%6` and the colour variant is a
  `waves.c:180` local variable that never reaches the ECS — it exists only baked
  into `CSprite.color`. Nothing on the wire describes either.
- **Fix**: same as B19; `size_q` and `tint` are exactly these two values.
  Sprint N1.
- **Status**: PLANNED.

### B21. Pickup subtype is unreplicated (all three look identical to a client)
- **Symptom**: medkit, ammo and speed pickups are indistinguishable on a client.
- **Root cause**: one `NET_ENT_ITEM` kind covers all three; the real subtype
  lives in `CItemTag.type`, which is not replicated.
- **Fix**: `art` carries the subtype (`NET_ART_MEDKIT`/`AMMO`/`SPEED`) on the
  wire. Sprint N1.
- **Status**: PLANNED.

### B22. Client HUD health is the client's own never-simulated ECS
- **Symptom**: a client's own health readout does not fall when the host damages
  them; it can read full while dead.
- **Root cause**: `hud.c:140` calls `ecs_get_health(ecs, local->entity)` on the
  local ECS, but a render-only client never simulates, so that entity is never
  damaged. The authoritative value is already on the wire as `NetEntitySnap.hp`.
- **Fix**: read HP from the mirror entity for the local slot. Sprint N3.
- **Status**: PLANNED.

### B23. Client HUD points / weapon / ammo are the client's own stale inventory
- **Symptom**: a client's points, weapon, grenade and launcher-ammo readouts do
  not reflect scoring or pickups that the host recorded.
- **Root cause**: `hud.c:137` reads `players[0].inventory` from local state the
  client never simulates. The host *does* relay `GE_POINTS`, `GE_ITEM_PICKUP`
  and `GE_KILL` (`main.c:1214`), but `drain_net_events()` only logs them and
  applies them to nothing.
- **Fix**: apply the relayed events to a client-side display inventory, keeping
  the host authoritative. Sprint N3.
- **Status**: PLANNED.

### Related gap (not numbered — by design)
Two peers built from different asset trees render different pictures, and nothing
detects it. That is the failure class of B15–B18, and there is no asset/content
version in the handshake. Tracked as Sprint N2 scope
(`NET_ASSET_VERSION`, `NET_REJECT_ASSET`) rather than as a bug, because no
current build exhibits it.
