# Cross-Branch Conflict Analysis

Analysis date: 2026-09-30 (re-evaluated)

Context for agents: two feature branches were forked from `main` and are
being worked on simultaneously. This doc lists every known interplay so
the eventual merge can be planned instead of discovered.

## Current topology (verified 2026-09-30)

`origin/*` is **stale** (nothing pushed since the last batch):

| Branch | `origin` | Local |
|---|---|---|
| `main` | `e8bb53b` | `61a0b6b` (8 ahead) |
| `feature/assets-maps` | `0520d30` | `4bd1143` (18 ahead) |
| `feature/multiplayer` | `7c6b93d` | `b365330` (39 ahead) |

`merge-base` of all three branches = `61a0b6b` (= local `main` tip). So
main is a strict ancestor of both feature branches: merging either branch
into main is a fast-forward, and the *only* real merge surface is
assets-maps vs. multiplayer directly.

What `61a0b6b` (main) already contains - **wave-1 of the assets work**:
SDL_image, asset manager, theme system, map parser + registry, textured
entity sprites (including `world_load_map` and the map-based world). So
the old "assets first, then MP" concern has partly dissolved: the world
rewrite and sprite textures are already on main.

What changed since the 2026-09-18 analysis:

- main moved from `e8bb53b` -> `61a0b6b` and absorbed the first assets wave.
- `feature/multiplayer` did `f8891ec` *"Merge main + feature/multiplayer:
  union resolution"* - it absorbed main but **deliberately reverted the
  world and sprite texture work** (details in §1).
- `feature/assets-maps` grew the dedicated **map-select screen**
  (`GAME_STATE_MAP_SELECT`, `c005d95`), theme-tinted entities (`f91b13f`),
  alpha fixes, item/pickup rendering fixes, and B4-B6 bug fixes.

---

## 1. The critical finding: MP resolved main's assets work away

`feature/multiplayer` already merged main at `f8891ec`, but the merge
resolution kept MP's **old single-player world and rendering**:

- `src/world/world.c/h` on MP = fixed `tiles[WORLD_TILES_Y][WORLD_TILES_X]`,
  **no** `world_load_map`, no map files. Assets/main = dynamic, map-loaded,
  themed world.
- `src/graphics/sprite.c` on MP = flat-color only, **zero texture support**
  (`sprite_tex`/`SPRITE_SHAPE_TEXTURE`). Main/assets = full texture path.
- MP's `sprite.h` keeps `SPRIPE_SHAPE_TEXTURE` in the enum but comments it
  *"future use"*; MP's `main.c` never calls `sprite_tex()`.
- MP's `CMakeLists.txt` **does** link SDL_image, compile `src/assets/*.c`
  and `src/players.c`, and copies the `assets/` dir next to the binary -
  but the running code never loads or uses any of it (flat colors render).

Motivation is determinism: MP's wave/gameplay tests are byte-identical to
old SP and it wants to keep that gate stable. But it means **merging
assets into MP will silently resurrect every one of these divergences.**

---

## 2. Merge simulation: assets-maps -> multiplayer (2026-09-30)

`git merge-tree --write-tree feature/multiplayer feature/assets-maps`
reports **8 conflicting files**:

| File | Conflict hunks | Nature |
|---|---|---|
| `AGENTS.md` | add/add | each branch made its own standing rules */
| `src/items/items.c` | content | MP plain-rect pickup vs. assets textured pickup (2x scale) */
| `src/main.c` | **9** | Game struct, `create_player` vs `player_respawn`, world init (map vs fixed), menu entry, pickup loop, `camera_clamp` |
| `src/ui/hud.h` | content | MP per-player `hud_draw` vs. assets `hud_track_player_hp` decl |
| `src/ui/menu.c` | **7** | option_count 4 (Solo/Host/Join/Quit) vs. 2 (Start/Quit); address overlay vs. map-select entry |
| `src/ui/menu.h` | content | GameState enum: MP's CONNECTING/LOBBY vs. assets' MAP_SELECT |
| `src/world/waves.h` | content | MP player-table signatures vs. assets theme-param spawn |
| `tests/tests.c` | **3** | MP net/gameplay tests vs. assets map/theme/parser tests |

Auto-merged without conflict: `src/graphics/sprite.h`, `src/graphics/sprite.c`,
`src/ui/hud.c`, `src/world/waves.c`.

### The silent build breaks (git reports NO conflict, build fails anyway)

Verified byte-for-byte against the simulated merge tree:

1. **sprite.c** auto-merges to MP's **texture-less** version (0 texture
   fns) while the auto-merged `sprite.h` keeps `SPRIPE_SHAPE_TEXTURE` and
   the auto-merged `main.c` calls `sprite_tex()` twice -> compiler error.
   Resolution must take assets' `sprite.c`/`sprite.h`.
2. **world.c/world.h** auto-merge to MP's **fixed-array, map-less** world
   (no `world_load_map`, no theme), yet the auto-merged `main.c` calls
   `map_registry_get()` and `world_load_map` -> compiler error. Resolution
   must take assets' (== main's) world.
3. **hud.c** auto-merges to a version that **keeps both** MP's per-player
   `hud_draw` and assets' `hud_track_player_hp()` (2 refs). Therefore
   `hud.h` MUST keep assets' `hud_track_player_hp` declaration during
   conflict resolution - resolving `hud.h` to MP's side breaks the build.

### The good news in the auto-merge

- **`src/world/waves.c` auto-merges correctly**: it keeps MP's
  player-table `waves_update` AND assets' theme-tinted
  `waves_spawn_zombie(ecs, pos, theme)` together (both present in merged
  blob). `waves.h` conflict is just a signature-union, resolved by taking
  both declarations.

---

## 3. `src/main.c` - 9 conflict regions (worst file)

Auto-merge cannot produce anything sensible here; both sides restructured
it. Landmarks in the simulated merged blob:

- Game struct: `GameMode mode` (MP) + `Player players[MAX_PLAYERS]` (MP) +
  `MapSelectMenu map_select` (assets) all coexist in the blob - union is
  mechanically fine here.
- `map_select_init/free` (assets) survive the auto-merge; state machine
  (`GAME_STATE_LOBBY`, `GAME_STATE_CONNECTING`, `GAME_STATE_MAP_SELECT`)
  all reference each other.
- The menu flow, world init in `reset_game()`, and the pickup loop are the
  three places that need real decision, not text surgery (see §5).

---

## 4. Main menu - now a REAL conflict (was "future" in old doc)

MP landed its 4-option menu: `Solo / Host Co-op / Join Co-op / Quit`,
with `GAME_STATE_CONNECTING` + `GAME_STATE_LOBBY`. Assets reshaped the
menu to 2 options (`Start / Quit`) with a dedicated `GAME_STATE_MAP_SELECT`
screen and map preview. The two collide in `menu.c` (7 hunks) and
`menu.h` GameState enum.

Decide the merged layout deliberately, don't resolve by hand-waving:

- `menu.update`: MP's switch (Solo/Host/Join/Quit) vs. assets'
  `if (selected == 0) -> MAP_SELECT`.
- GameState enum needs the union of all three states.

---

## 5. Design gap that must be solved before merging MP

The assets world is **map-file-driven**; MP's determinism gate assumes a
fixed `world_init()` world. Once MP adopts the map/theme world:

- **The chosen map + seed must travel in the host/join handshake**, or
  host and client render different terrain and MP's determinism gate
  silently breaks. Decide this before the merge, not during.

---

## 6. `tests/tests.c` - port the old signatures

- MP: net codec + kill credit + nearest-targeting + TDM respawn tests
  using NEW `Player*` system signatures.
- Assets: map parser + theme registry + sword-sweep/zombie-contact tests
  **written against the OLD signatures** (`system_sword(&ecs, &input, &inv, dt)`,
  `waves_spawn_zombie(&ecs, pos, THEME_GRASSLAND)`, old HUD).
- Those old-signature tests must be ported to the new API as part of any
  merge - they will not compile otherwise.

---

## 7. Net-new file inventory (no conflicts)

- Assets: `src/assets/asset_manager.{c,h}`, `src/world/map_def.h`,
  `src/world/map_registry.{c,h}`, `src/world/theme.{c,h}`,
  `tools/gen_assets.py`, `assets/maps/*.map`, `assets/textures/**`.
- Multiplayer: `src/players.{c,h}`, `src/game_mode.h`, `tests/net_spike.c`.
- Committed PNGs must stay committed (`tools/gen_assets.py` is the
  generator, but no CI regeneration exists yet).

---

## Recommended sequencing (revised)

1. **Push everything first** - origin is 8/18/39 commits behind (`main`,
   `feature/assets-maps`, `feature/multiplayer` respectively).
2. Merge `feature/assets-maps` into `main` (fast-forward, no conflicts -
   main is already its ancestor). This makes main = assets tip.
3. Merge `feature/multiplayer` into `main` **last**, hand-resolving the 8
   files. Rules:
   - `sprite.c`, `sprite.h`, `world.c`, `world.h` -> assets' versions win.
   - `waves.h` -> union of both signature sets (matches auto-merged waves.c).
   - `hud.h` -> keep MP's `hud_draw` AND assets' `hud_track_player_hp`.
   - `menu.h` -> union GameState (all three states).
   - `main.c` + `menu.c` -> hand-assemble; make map+seed part of the net
     handshake (§5).
   - `tests.c` -> port assets' OLD-signature tests to the `Player*` API.
   - `AGENTS.md` -> union the standing-worktree rules (both are needed).

Doing it the other way (MP into assets first) is equally valid; the
decisive point is that **the 8-file list above is the whole conflict
surface** and three of the "clean" auto-merges are traps (§2).

## Agreement needed before more work

The `Player`-slot system signatures and the assets world/sprite API are
both already on main and are the ground truth. Remaining open decision:
map+seed handshake for the determinism gate (see §5) - settle that on the
multiplayer side, and all future assets work can target main safely.

---

## Handoff instructions for the MULTIPLAYER AGENT (do the merge)

Read the whole tree and this report before touching anything.

**Where you work**: the multiplayer worktree
`<main>/worktrees/multiplayer`, on branch `feature/net-protocol`.
(Work only in that worktree. Do not touch the `main` worktree or the
assets worktree - they hold WIP and the assets agent's ground truth.)

**What to do** (in order):

1. **Merge the assets branch into your branch first.** From
   `feature/multiplayer`: `git merge feature/assets-maps` (from the shared
   repo root). Expect the 8 conflicting files listed in §2 - resolve with
   the rules in "Recommended sequencing". These are the *known* problems;
   resolve them all by hand, do not accept either side wholesale.
2. **Verify the three silent build traps in §2** actually disappear after
   resolution: `sprite.c`/`sprite.h` keep the texture path, `world.c`
   keeps the map/theme world, and `hud.h` keeps `hud_track_player_hp`
   (auto-merged `hud.c` calls it).
3. **Read the assets branch documentation** before deciding any
   gameplay/visual detail - it records the features, the bugs, and the
   rationale of the assets work:
   - `docs/FEATURES.md` - what the assets branch shipped
   - `docs/BUGS.md` - every bug found + fixed (B4-B6 etc.)
   - `docs/PROGRESS.md`, `docs/SPRINT_PLAN.md` - session + sprint logs
   - `docs/ASSET_MAP_PLAN.md`, `docs/EXTENDING.md` - asset/map/API docs
   Any code you add on top of assets' work must respect those docs.
4. **Solve the §5 handshake gap**: the selected map + seed must travel in
   the host/join handshake so both peers build the same world. Decide and
   implement this *during* the merge; it is a merge-blocking gap, not a
   follow-up.
5. **Create the tracking docs on your merged branch** so the project can
   keep accounting for both engines in one place:
   - `docs/FEATURES.md` - record *your* merged features (net, per-player
     game modes, kill credit, beacons - and that assets' textures/maps/
     themes now render in MP)
   - `docs/BUGS.md` - record every bug you hit *during the merge* (add
     episode: symptom / root cause / fix / status / commit ref)
   - Keep `docs/PROGRESS.md` / session logs updated as usual.
6. **Commit with the project's git discipline**:
   - one commit per fix / significant change (no giant "merged everything")
   - conventional prefixes from history: `feat(scope):`, `fix(scope):`,
     `docs(scope):`, `build(scope):`, `merge:`
   - commit after each resolution unit completes - the history IS the
     record reviewers will read.
7. **Verify before done** (assets agent's ritual, carry it on):
   `cmake --build build` (zero warnings), `ctest --test-dir build`, and
   `./build/zombie_tests` (assertion suite).