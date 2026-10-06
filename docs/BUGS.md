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
| B19 | Clients render entities as flat circles, not sprites (R13-I5) | `f7c44f3` | FIXED |
| B20 | Zombie size and colour variant never replicated | `f7c44f3` | FIXED |
| B21 | Pickup subtype never replicated | `f7c44f3` | FIXED |
| B22 | Client HUD health reads the client's own stale ECS | `e9d2d71` | FIXED |
| B23 | Client HUD points/weapon/ammo read the client's own stale inventory | `e9d2d71` | FIXED |
| B24 | No asset version — peers could connect and render different art | `2219792` | FIXED |
| B25 | A refused connection was invisible to the player | `2219792` | FIXED |
| B26 | The client HUD's wave panel never updated | `56cce96` | FIXED |
| B27 | Beacon positions were re-derived on the client | `56cce96` | FIXED |
| B28 | Remote health bars divided by a hardcoded maximum | `56cce96` | FIXED |
| B29 | Particles and effects are not replicated | — | OPEN (accepted) |
| B30 | A client could "buy" from the shop with no effect | `e9e5107` | FIXED |
| B31 | Zombie animation state never replicated | — | RETRACTED (no code path) |
| B32 | Client shows a blank screen when its player dies, and never shows its own name tag | `e5c6dec` | FIXED |
| B33 | Client shop prices are drawn from a frozen local inventory, not host state | `d5da693` | FIXED |
| B34 | Player name tag overlaps the zombie/wave counter | `50a2365` | FIXED |
| B35 | A client's own name tag shows "Player" in the wrong colour | `7d0cb15` | FIXED |
| B36 | The documented `-Werror` / zero-warnings gate was never enabled in the build | — | AUDITED (fix pending) |

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
- **Fix**: `f7c44f3` transmits appearance (`art`, `size_q`, `tint[3]`) in the
  snapshot and the mirror reproduces the host's sprite verbatim. The per-kind
  size table is deleted. `NET_WIRE_VERSION` 3 -> 4.
- **Status**: FIXED — but **unproven visually**; see `docs/PENDING_VERIFICATION.md`.

### B20. Zombie size and colour variant are unreplicated
- **Symptom**: even after B19, a client cannot match the host's zombies.
- **Root cause**: zombie size is `10 + rand()%6` and the colour variant is a
  `waves.c:180` local variable that never reaches the ECS — it exists only baked
  into `CSprite.color`. Nothing on the wire describes either.
- **Fix**: `f7c44f3`; `size_q` and `tint` are exactly these two values.
- **Status**: FIXED — **unproven visually**.

### B21. Pickup subtype is unreplicated (all three look identical to a client)
- **Symptom**: medkit, ammo and speed pickups are indistinguishable on a client.
- **Root cause**: one `NET_ENT_ITEM` kind covers all three; the real subtype
  lives in `CItemTag.type`, which is not replicated.
- **Fix**: `f7c44f3`; `art` carries the subtype on the wire.
- **Status**: FIXED — **unproven visually**.

### B22. Client HUD health is the client's own never-simulated ECS
- **Symptom**: a client's own health readout does not fall when the host damages
  them; it can read full while dead.
- **Root cause**: `hud.c:140` calls `ecs_get_health(ecs, local->entity)` on the
  local ECS, but a render-only client never simulates, so that entity is never
  damaged. The authoritative value is already on the wire as `NetEntitySnap.hp`.
- **Fix**: `e9d2d71` replicates a `NetPlayerState` per roster slot in the
  snapshot (`NET_WIRE_VERSION` 5 → 6) and `hud_draw()` reads HP from it.
- **Status**: FIXED in code — **unproven on screen**. Note it could *not* be
  read from `NetEntitySnap.hp` as originally planned: `slot_entities[s]` is 0
  whenever the host has the player dead, so a dead client — exactly when the
  respawn overlay matters — would have had no entity to ask.

### B23. Client HUD points / weapon / ammo are the client's own stale inventory
- **Symptom**: a client's points, weapon, grenade and launcher-ammo readouts do
  not reflect scoring or pickups that the host recorded.
- **Root cause**: `hud.c:137` reads `players[0].inventory` from local state the
  client never simulates. The host *does* relay `GE_POINTS`, `GE_ITEM_PICKUP`
  and `GE_KILL` (`main.c:1214`), but `drain_net_events()` only logs them and
  applies them to nothing.
- **Fix**: `e9d2d71` replicates the inventory itself (points, grenades,
  launcher ammo, selected weapon, unlocked mask) in the same per-slot block.
- **Status**: FIXED in code — **unproven on screen**.
- **Deviation from plan, deliberately.** The plan said to *derive* the display
  inventory by applying relayed `GE_POINTS` / `GE_ITEM_PICKUP` events. Deriving
  it was the wrong call and would have shipped a subtler bug than the one being
  fixed: events can be missed, arrive out of order, or simply have not happened
  yet, and the join-time baseline is not on the wire at all — so a client would
  have started every session at zero and climbed toward the truth at the mercy
  of the event stream. Replicating the authoritative value instead cannot
  drift. Deriving is only safe when the two peers provably start identical.

## B24. No asset version — peers could connect and render different art

- **Symptom**: none. This is the interesting one: nothing breaks, the session
  runs, and the two windows simply look different.
- **Root cause**: the handshake carried `NET_WIRE_VERSION` and
  `NET_WORLD_GEN_VERSION` but nothing about *content*. Two peers built from
  different asset trees agreed perfectly on the protocol and disagreed
  completely on the pictures — the B15–B18 failure class, except between two
  live players instead of between a branch and a build.
- **Fix**: `2219792` adds `NET_ASSET_VERSION` (1) to both the JOIN and the
  HELLO, with a distinct `NET_REJECT_ASSET` reason and
  `NET_FLAG_ASSET_MISMATCH`. The host refuses before allocating a slot; the
  client refuses on the HELLO. `NET_WIRE_VERSION` 4 → 5.
- **Status**: FIXED — loopback-tested. The human-readable refusal is **unproven
  on screen**; see `docs/PENDING_VERIFICATION.md`.

## B25. A refused connection was invisible to the player

- **Symptom**: a client that was rejected dropped silently to the menu. A
  version mismatch looked identical to the game failing to launch, because the
  only record was a `LOG_ERROR` in a console nobody had open.
- **Root cause**: `main.c` logged `net_reject_reason_name(...)` — a short label
  like "version mismatch" — and returned to `GAME_STATE_MENU`. `MainMenu` had no
  message field at all, so there was nowhere to put one even if it had been
  written. The REJECT packet also carried no version numbers, so the message
  *could not* have named both sides.
- **Fix**: `2219792` adds a `message` line to `MainMenu` with
  `menu_set_message()`/`menu_clear_message()`, drawn under the subtitle;
  `net_version_conflict_message()` which names both numbers and the remedy; and
  a version field on the REJECT body so the refused player learns what the host
  has.
- **Status**: FIXED in code — **unproven on screen**.

---

# Sprint N4 — full host/client divergence audit

Method: every read of client-local simulation state on a path a render-only
client executes, plus every value the client re-derives instead of receiving.
`render_only_client()` breaks out of the update switch *before* any system runs,
so the invariant "the client simulates nothing" holds; the bugs below are all
about what the client *displays* or *derives* despite simulating nothing.

| # | Symptom | Class | Status |
|---|---|---|---|
| B26 | Client HUD shows `Wave: 0`, `Kills: 0`, no zombie count, no next-wave countdown, forever | stale local state | **FIXED (N4)** |
| B27 | Beacons positioned by a formula duplicated in two places | fragile derivation | **FIXED (N4)** |
| B28 | Remote zombie health bars overflow at wave 2+ | hardcoded constant | **FIXED (N4)** |
| B29 | No particles/effects on clients at all | unreplicated cosmetic | ACCEPTED |
| B30 | Client can open the shop; purchases mutate a local struct the host never sees | authority violation | FIXED (N5) — **unproven on screen** |
| B31 | Animation phase not replicated | ~~cosmetic, small~~ | **RETRACTED — not a bug** |

## B26. The client HUD's wave panel never updated

- **Symptom**: on a client the HUD read `Wave: 0`, `Kills: 0`, and showed
  neither "Zombies: N" nor "Next wave in Xs" — for the entire match. The
  gameplay itself was fine; only these readouts were wrong. Easy to miss because
  the health bar and world render correctly.
- **Root cause**: `hud_draw()` took `WaveSystem *waves` and read `wave_number`,
  `wave_active`, `zombies_alive`, `between_waves`, `wave_cooldown`,
  `total_kills` from it. On a client `waves_update()` is never called — the
  `render_only_client()` break sits at main.c:765, `waves_update()` at
  main.c:793 — so those fields kept their `waves_init()` zeros. The snapshot
  already carried `wave_number`, `wave_active` and `total_kills`; nothing read
  them.
- **Fix**: N4 adds the missing wave fields to the snapshot and routes the HUD
  through the same authoritative-view struct used by N3. `NET_WIRE_VERSION` 6 → 7.
- **Status**: FIXED.

## B27. Beacon positions were re-derived on the client

- **Symptom**: none today.
- **Root cause**: `sync_client_beacon_slots()` computed
  `spawn + (slot*70, slot*30)` — byte-for-byte the same expression
  `reset_game()` uses on the host. So the two agreed *by coincidence of
  duplication*, not because the client was told. This is precisely the
  "client re-derives replicated state" anti-pattern that caused R13-I5; it was
  one edit to either copy away from silently wrong beacons on every client.
- **Fix**: N4 replicates each slot's `beacon_pos` in the snapshot and the
  client reads it, so the formula lives in exactly one place.
- **Status**: FIXED.

## B28. Remote health bars divided by a hardcoded maximum

- **Symptom**: zombie health bars overfilled their background from wave 2
  onward, and player bars would have done the same under `--player-hp-pct`.
- **Root cause**: `system_render_mirror()` used
  `e.hp / (kind == PLAYER ? 200.0f : 100.0f)`. Those are the *base* values.
  `waves_update()` scales zombie health by `difficulty_multiplier`
  (waves.c:250), so from the first difficulty increase the true maximum is
  above 100 and the ratio exceeds 1.0.
- **Fix**: N4 puts `hp_max` in the entity snapshot and uses it. This also
  removes the kind-switch on a gameplay value — the same "switch on `kind` and
  guess" shape that caused R13-I5, one level up.
- **Status**: FIXED.

### B31 — RETRACTED. "Animation phase is not replicated" was never a bug

I listed this as an N4 candidate in the planning doc (`f09cb29`), under N1's
"out of scope" list, and then carried it into the audit table without ever
checking the claim. Checking it now:

- **Nothing ever adds `COMP_ANIMATION`** to any entity — there are zero call
  sites for it in `src/`.
- **Nothing ever reads animation state.** `system_animation()` is the only
  consumer of `CAnimation`, and it only ever advances `current_frame` itself.

So `system_animation()` is a no-op that walks every entity and finds none with
the component. No entity animates on the host either, therefore nothing can
diverge, and there is nothing to replicate. It is unused code, not a
multiplayer divergence.

I should not have promoted an unverified line from a planning list into a
numbered bug with a disposition. Retracted; recorded here rather than deleted so
the mistake is visible instead of quietly reappearing.

## B29. Particles and effects are not replicated

- **Symptom**: clients see no death particles, muzzle flashes, blood or
  explosions. Entities pop out of existence instead.
- **Why accepted, not fixed**: `NetEntitySnap` has no particle kind, and
  particles are short-lived, numerous and purely decorative. Replicating them
  means a new entity kind plus spawn/lifetime handling on a channel that is
  already carrying 20 Hz snapshots — a real change with no gameplay value and
  real bandwidth cost. Cosmetic-only, host-authoritative by construction (the
  client cannot invent them, it simply omits them). Recorded so it is a
  decision, not an oversight.
- **Status**: ACCEPTED.

## B30. A client could "buy" from the shop with no effect

- **Symptom**: pressing B on a client opened the shop; buying decremented a
  local inventory the host never reads, so the purchase silently vanished on
  the next snapshot.
- **Root cause**: `GAME_STATE_SHOP` is reachable on a client, and
  `shop_menu_update()` is handed `&game.players[0].inventory` — the client's own
  unsimulated struct. There was no purchase-request packet, so there was nothing
  the client *could* legitimately do.
- **Fix (N4, interim)**: blocked client purchases with an explicit "host only,
  not networked yet" message instead of letting the UI imply the purchase
  worked. Honest, but it left the feature missing.
- **Fix (N5, real)**: co-op shopping now works. Wire version 7 -> 8 adds two
  reliable packets: `NET_PKT_SHOP_REQUEST` (client -> host, one item byte) and
  `NET_PKT_SHOP_RESULT` (host -> client, item + `NET_SHOP_RES_*` reason).
  - The host resolves the slot from its own `peer->data`, **never** from the
    packet's `from_slot`, so one client cannot spend another's points.
  - A request means "equip this, buying it first if not owned" — the same
    semantics as the single-player shop, so host and solo player cannot drift.
  - The rule lives in `weapons_shop_apply()` (weapons layer, where the economy
    is) rather than in the net layer, so it is unit-testable; the net layer only
    chooses the slot. Wire ids alias `SHOP_ITEM_*` with `_Static_assert`s, so
    the host cannot mistranslate a client's id.
  - The client's menu displays a snapshot-derived copy
    (`client_inventory_view()`) and mutates nothing; every press leaves as a
    request.
  - The host answers **every** request, including refusals. Without that a
    refusal is invisible: an unaffordable purchase and one still in flight both
    leave the numbers unchanged, and the player cannot tell them apart.
- **Status**: FIXED in code — **unproven on screen**. See
  `docs/PENDING_VERIFICATION.md` for the two-window check.
- **Note on a bug found while building this**: the first version of the host
  handler tested the decoder's return with `!= 0`, but every decoder in this
  codebase returns the number of bytes consumed, so success is `8`, not `0`.
  Every request was rejected as malformed. The end-to-end loopback test is what
  caught it; the codec unit tests could not, because they never made that
  mistake themselves. Worth remembering whenever a `net_decode_*` return value
  is compared.

## B32. A client shows a blank screen when its player dies, and never sees its own name tag

Found 2026-10-05 auditing the UI layer on `main`. High severity.

- **Symptom**: two symptoms, both on the *client* only. (1) When the client's own
  player dies, the HUD goes completely empty — no respawn countdown, no
  `ELIMINATED` mark, nothing. (2) The player's coloured name tag never appears
  in a client's viewport, which is the one place a spectator most needs to know
  whose view they are looking at.
- **Root cause**: an inverted flag at the single call site. `hud_draw()` takes a
  `multi` parameter documented in `src/ui/hud.h:67` as *"When `multi` is set the
  player's colored name tag is shown"*, and used for two things: the name tag
  (`src/ui/hud.c:201`) and the post-death overlay (`src/ui/hud.c:126`). The only
  caller passes `!render_only_client()` (`src/main.c:1195`), which inverts the
  meaning. `render_only_client()` is true **only** on a connected client, so the
  argument means "true in single-player, true as host, false as client":

  | session | `render_only_client()` | `multi` passed | should be |
  |---|---|---|---|
  | single player | false | **true** | false |
  | host | false | **true** | true |
  | client | true | **false** | true |

  The expression is correct only for the host. Every other case is backwards.
- **Impact**: the death overlay is *skipped entirely* on a client, because
  `hud_draw` does `if (!alive) { if (multi) {...} return; }` — so a dead client
  falls straight through the `return` having drawn nothing at all.
- **Fix**: pass "is this a networked session" rather than the negation of "is
  this the client", i.e. `game.net_host_mode || render_only_client()`.

## B33. A client's shop prices are drawn from a frozen local inventory

Found 2026-10-05 auditing the UI layer. High severity. **This is a regression
introduced by the N5 shop commit `e9e5107` — my own bug, caught by the audit
rather than by any test.**

- **Symptom**: on a co-op client the shop menu's *logic* reflects host state but
  the *numbers on screen* do not. A purchase that the host refuses leaves the
  displayed prices, credits and ammo unchanged, and buying medkits/ammo on the
  client changes nothing visible — because the client is drawing its own
  unsimulated inventory, which the host never reads.
- **Root cause**: the N5 fix was applied to the update path but not the draw path.
  In `GAME_STATE_SHOP` the update builds a snapshot-derived copy via
  `client_inventory_view(&view)` and hands *that* to `shop_menu_update()`
  (`src/main.c:920-928`) — correct. But the draw call a few lines later still
  passes the raw local struct:

  ```c
  shop_menu_draw(game.renderer, &game.shop_menu,
                 &game.players[0].inventory, win_w, win_h, game.font_large);
  ```

  So `shop_menu_update` decides "you already own this" from host state while
  `shop_menu_draw` renders ownership and prices from stale local state. The two
  disagree by construction, which is exactly the class of bug B30 was supposed
  to end.
- **Why no test caught it**: the N5 assertions exercise `weapons_shop_apply()`,
  the wire codecs, and the host/client loopback — all of which pass. Nothing
  compares what `shop_menu_draw` reads against what `shop_menu_update` reads,
  because that is a wiring property, not a logic property.
- **Fix**: hoist the same `view` used by the update path into the enclosing
  scope and draw from that, so the menu cannot present state the host never
  sent.

## B34. The player's name tag overlaps the wave counter

Found 2026-10-05 auditing the UI layer. Low severity, cosmetic.

- **Symptom**: on a host in multiplayer, during an active wave, the player's name
  tag is drawn over the "Zombies: N" line and both are unreadable.
- **Root cause**: two HUD rows share a baseline. The name tag is drawn at
  `y = 76.0f` (`src/ui/hud.c:205`) and the zombie counter at `y = 75.0f`
  (`src/ui/hud.c:212`), both at `x = 20.0f`. The "Next wave in..." row has the
  same collision at `src/ui/hud.c:216`.
- **Why it was invisible until now**: B32 means the name tag only ever drew on
  the host, where this overlap is now live. Fixing B32 alone would newly expose
  the collision on *both* windows, so the two fixes must land together.
- **Fix**: give the name tag its own row.

## Rejected during this audit (recorded so they are not re-raised)

- **"The health bar can exceed its track / divide by zero."** `src/ui/hud.c:176`
  clamps `ratio` below but not above, and the `else` branch at
  `src/ui/hud.c:159-160` has no `hp_max > 0` guard while the `use_auth` branch
  does. Neither is reachable: `hp->max` is never assigned anywhere after
  `spawn_player_entity` initialises it to 200, every write to `hp->current`
  either clamps (`src/items/items.c:110`, `src/world/waves.c:252`) or derives
  from a percentage the CLI already clamps to 0-100 (`src/main.c:1589-1590`), and
  the networked values come from the host's own `CHealth`. Worth hardening
  eventually; not a bug today. (The original report also claimed
  `--player-hp-pct` was unclamped — the flag is `--player-hp=` and it is clamped.)
- **"`ecs_get_health(ecs, ECS_NULL_ENTITY)` reads one past `World.healths`."** The
  accessor genuinely does not bounds-check, so this *would* be an out-of-bounds
  read — but it is unreachable from `hud_draw`. The local branch sits after
  `if (!alive) { ... return; }` (`src/ui/hud.c:125-142`), and `alive` is false
  whenever `local->entity == ECS_NULL_ENTITY`. Not a live bug.

## B35. A client's own name tag reads "Player" in the wrong colour

Found 2026-10-05, **as a direct consequence of fixing B32**. High severity for a
UI defect: the one element B32 existed to restore labelled the wrong player.

- **Symptom**: with B32 fixed, a client's HUD finally draws a name tag — and it
  says `Player` in the single-player default colour, regardless of what the
  client typed for `--name=`.
- **Root cause**: `hud_draw` renders the tag from `local->name` / `local->color`,
  where `local` is `&game.players[0]`. On a render-only client that slot is never
  simulated, and `reset_game()` fills it with the literal name `"Player"` and
  `COLOR_BLUE` (`src/main.c:188-195`) — the same values single-player uses. The
  client's real identity lives in `game.net_client.slot` and its name in
  `net_player_name`; neither reaches the tag.
- **Why it was missed**: before B32 the tag never drew on a client at all, so the
  wrong name was invisible. Fixing B32 made a latent wrong-value bug visible.
  Worth noting as a class: a tag that draws nothing cannot be wrong, so any fix
  that makes it draw has to check *what* it draws.
- **Fix**: the host already echoes every player's name back in the roster, and the
  lobby screen uses it correctly (`src/main.c:1052-1057`). The HUD's
  `HudPlayerState` gains `name` / `slot_color` / `has_identity`, filled from the
  roster entry matching `net_client.slot`. Falls back to `local` when there is no
  authoritative identity, which is the correct single-player path.

## Rejected during the multiplayer sweep (same session)

- **"A client's slot-0 beacon is re-derived and can drift."**
  `sync_client_beacon_slots()` skips slot 0 on a client because it holds a
  "real local entity" (`src/main.c:301`), leaving `beacon_pos` at the client's own
  `world_get_spawn_point` rather than reading it from the snapshot like every
  other slot. It agrees today only because `beacon_pos` is written exactly once,
  at respawn (`src/players.c:75`), and never moved afterwards. So there is no
  divergence today, but the skip is load-bearing on a property nothing enforces —
  the day slot 0's beacon becomes movable, this silently disagrees. Worth a
  follow-up, not a bug now.

## B36. The documented "-Werror, zero warnings" gate was never enabled

Found 2026-10-05 while running the gate as AGENTS.md describes it. Medium
severity — a process defect rather than a shipped one, and it had been silently
green for a long time.

- **Symptom**: none. Every build reported success, and the standing instructions
  in `AGENTS.md` say a change is only done when there is "a clean Release build
  with `-Werror` and zero warnings in project sources."
- **Root cause**: `CMakeLists.txt` contained **no warning flags at all** — no
  `-Wall`, no `-Wextra`, no `-Werror`, no `target_compile_options` anywhere. The
  compiler's default warning set does not cover unused parameters, sign-compare,
  type-limits or missing field initializers, so the build was genuinely
  "warning-free" while saying nothing. The gate was checking for a condition the
  build was never capable of producing.
- **How it was found**: compiling the project's own translation units from
  `compile_commands.json` with `-Wall -Wextra -Werror` showed **5 of 39 units
  failing** — and then, once those were fixed, several more behind them. Two
  `net_spike` / `log.c` warnings only appeared *after* the first batch, because
  the build stops at the first `-Werror` and had never gotten that far.
- **Fix**: `PROJECT_WARNINGS` applied to every project target (and **only** those
  targets — fetched SDL3 sources have their own unused-parameter and sign-compare
  warnings, and inheriting ours would break them).

What the newly-enabled gate surfaced:

| Where | Warning | Nature |
|---|---|---|
| `src/ecs/ecs.c` | unused `world` parameter | API kept, marked unused |
| `src/systems/collision.c` | unused `world` parameter | marked unused |
| `src/main.c:1296` | `in.weapon >= WEAPON_PISTOL` always true | `uint8_t >= 0`, dead guard |
| `tests/tests.c`, `tests/net_test.c` | missing `owner` / `art` / `size_q` / `tint` initializers | 10 `NetEntitySnap` literals |
| `tests/net_spike.c` | dead `g_failed` | unused static variable |
| `src/core/log.c` | dead `level_colors` | see below |

The last one was a real latent bug rather than a lint: `level_colors[]` was
declared but never read, while `RESET_COLOR` **was** emitted at the end of every
console line. So each log line ended in an escape sequence that reset a colour
that had never been set — the array's obvious purpose, coloring by level, had
never been wired up. It is now applied.

**Note on the count**: the 10 `NetEntitySnap` literals are incomplete rather than
wrong — the omitted fields are trailing and zero-filled by C, so the tests were
correct throughout. `-Wmissing-field-initializers` is here because a *partial*
snapshot literal is exactly the pattern that goes stale when a field is appended
to `NetEntitySnap` - `owner` and the v7 appearance fields were both added after
these literals were written. It did not happen to cost anything here, since the
omitted fields were trailing; the warning makes it visible next time.
