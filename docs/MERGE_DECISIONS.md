# Cross-Branch Merge Decisions (R13)

Union of `feature/multiplayer` and `feature/assets-maps` into
`feature/multiplayer`. Analysis that drove this: `CROSS_BRANCH_CONFLICTS.md`.

## Order of work

`feature/assets-maps` was merged **into** `feature/multiplayer` (not the other
way round). The analysis doc requires working in the multiplayer worktree, and
`main` holds uncommitted WIP that must not be clobbered. `main` is an ancestor
of both branches, so the integration is a single merge and `main` can take it
after its WIP is settled.

## Visible conflicts (8)

`AGENTS.md`, `src/items/items.c`, `src/main.c`, `src/ui/hud.h`,
`src/ui/menu.c`, `src/ui/menu.h`, `src/world/waves.h`, `tests/tests.c`.

Resolution rule: **multiplayer structure wins, assets behaviour wins.** Keep
the `Player *`/`MAX_PLAYERS`/netcode shape from the multiplayer branch; keep
textures, themes, maps, registry and pickup/camera behaviour from assets.

## Silent breaks git reported no conflict for

These are the dangerous ones: MP's tree was built against an older
`main`/`assets`, so whole features were simply absent rather than conflicted.

| # | File(s) | What was missing | Fix |
|---|---------|------------------|-----|
| 1 | `graphics/sprite.c/.h` | texture resolution through the asset manager | take assets' version |
| 2 | `world/world.c/.h` | `world_load_map`, heap `tiles` | take assets' version |
| 3 | `players.c` | textured player sprite | restore assets' constructor, keep slot-colour precedence |
| 4 | `items/items.c` | `graphics/sprite.h` include, textured pickups | restore include + texture path |
| 5 | `ui/menu.c`, `main.c`, `tests/tests.c` | `world/map_registry.h` include | add include |
| 6 | `tests/tests.c` | `test_map_registry_loads` dropped, `test_map_parser` truncated by the auto-merge | restore both whole |
| 7 | `ecs/ecs.h`, `systems/systems.h`, `systems/player_input.c`, `systems/zombie_ai.c`, `systems/collision.c`, `systems/sword.c`, `core/mathutil.h`, `systems/render.c` | **main's own features that MP had reverted**: `CPlayerTag.slow_timer`, `zombie_damage_player()`, `PLAYER_HURT_SLOW_*`, zombie-contact damage, `CZombieTag.sword_hit_timer`, `CSwordTag.outer_radius`, `vec2_closest_on_segment()`, blade renderer | restore all (see below) |

Items 1–6 are assets features. **Item 7 is worse than that: these were
`main` features, and restoring them is required to avoid regressing `main`.**
`CROSS_BRANCH_CONFLICTS.md` predicted 3 silent traps and found 7.

### Item 7 detail

- `CPlayerTag { int empty; }` → `{ float slow_timer; }`, and
  `system_player_input` applies `PLAYER_HURT_SLOW_FACTOR` while it is positive,
  using `max_speed` so the speed-boost pickup still works.
- `zombie_damage_player()` is now `zombie_damage_player(ecs, zombie, target,
  origin)` — the target is **passed in**, not scanned for. That keeps MP's D1
  fix ("damage the player it actually chased") while restoring main's knockback
  and slow. Both `system_zombie_ai` (nearest target) and `system_collision`
  (overlapping player) call it, sharing the zombie's `attack_timer` so a hit
  lands once per cooldown rather than every frame.
- Sword: the blade sweeps from the inner circle to `outer_radius` (26 → 58)
  using a segment-vs-circle test via `vec2_closest_on_segment()`. Cooldowns are
  **per zombie** (`CZombieTag.sword_hit_timer`), so one sweep can slash through
  several; MP had regressed this to a single global `CSwordTag.hit_timer`, which
  was removed. `render.c` draws the blade pivoted at the handle.
- `GameWorld` owns a heap `tiles` buffer, so it must be zero-initialized before
  `world_init()`/`world_free()`. MP's tests dropped the `= {0}` initialiser
  (harmless with main's old fixed-size array) and crashed with
  `free(): invalid pointer` once assets' heap world was restored. The contract
  is now documented in `world.h`.

## Map identity in the handshake (analysis gap 5)

A joining client used to build whichever map its own never-shown picker held,
so the two windows could disagree about terrain, spawn and theme.

- `NET_WIRE_VERSION` 2 → 3, `NET_WORLD_GEN_VERSION` 1 → 2.
- `NET_PKT_HELLO` carries `uint8_t map_index` after `world_gen`, before the host
  name. A stale peer would read it as the first character of the name, hence
  the version bumps.
- `net_server_host()` takes the map; the host sends `map_select.selected_option`.
- The client stores `map_index` / `map_index_valid`; `reset_game()` loads the
  **host's** map when set, otherwise its own selection. Headless runs still use
  the fixed classic world (`world_init`) so the determinism gate holds.
- A host naming a map this build does not have is **rejected**
  (`NET_REJECT_WORLD`) rather than silently falling back to a default world,
  which would desync the two instances.
- New `--map=N` flag presets the map-select index. It is applied after `init()`,
  because `map_select_init()` (inside `init()`) resets `selected_option` to 0.
  This makes the host's map scriptable and the handshake testable without a GUI.

## Verified

- Release `-Werror`, zero project warnings.
- `ctest` 3/3 (`net_spike`, `net_loopback`, `core_tests`).
- `zombie_tests`: 719 passed, 0 failed.
- SP byte-identity: two `--seed=42 --ai=bot --run-seconds=25` event logs are
  `diff`-empty (125938 bytes each); wave 1 is `8 zombies, interval: 1.90s,
  difficulty: 1.00`.
- 2P map handshake: host `--map=2` → `NET: hosting lobby ... (map=2)` → client
  `hello ... map=2` → client `Starting map: Snow (34x34, Snow)`, matching the
  host, and only after the hello is processed.
- `python3 tools/gen_assets.py --verify`: 33 textures verified.

## Still unverified / deliberately open

See `PENDING_VERIFICATION.md`. Headless gates are structurally blind to the
render path, so R13-I5 (client draws dots, not sprites), M8 (client freeze /
overlay), M5-GUI, M6 and M7 all still need a two-window on-device run.
