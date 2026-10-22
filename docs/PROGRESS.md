# Progress Log

Running narrative of what was found and what was done, per session. PM notes +
DEV work interleaved (team structure per `AGENTS.md`).

---

## Session 2026-09-29 — Post-playtest code sweep, Sprint 3 setup

### PM standup — planned
- Sweep all systems for remaining bugs / dead code after the B1–B3 fixes.
- Write down future ideas + features in docs.
- Open sprint 3 with the bugs found.

### DEV — done
- **Code sweep** across `src/systems/*`, `src/ui/*`, `src/items/*`,
  `src/weapons/*`, `src/world/*`, `src/main.c`, `src/events/*`, `src/ecs/*`:
  - Confirmed B2/B3 art fixes are sound; alpha + blend mode + pickup scale OK.
  - Found `ITEM_AMMO` is a no-op (items.c:117-119).
  - Found HUD damage flash is dead code (never set, only decayed/drawn).
  - Found camera never clamps to world bounds (void at map edges).
  - Noted `draw_text*` in hud.c/menu.c recreate an SDL surface+texture every
    frame (perf; parked as engineering idea).
  - Confirmed item `bob_timer` field exists but is unused (idea).
  - Clean: `rm /tmp/opencode` leftovers none; worktree clean before docs commit.
- **Docs created/updated** (this commit):
  - `AGENTS.md` — persistent team process/roles/commit/worktree rules.
  - `docs/SPRINT_PLAN.md` — Sprint 3 (B4–B6) with PM/DEV structure.
  - `docs/FUTURE_IDEAS.md` — parked backlog (muzzle flash, shake, damage
    numbers, item bob, separation, text caching, camera clamp note).
  - `docs/BUGS.md` — added B4, B5, B6 (OPEN).
- **Verification**: no code changed this commit; build/tests unaffected.

### PM — next
- DEV picks up B4, then B5, then B6 (one commit each), re-running the
  verification ritual per ticket and updating BUGS.md + this log.

### Blocked
- None.

---

## Session 2026-09-29 (cont.) — Sprint 3 DEV: B4 DONE

### PM standup — planned / in progress
- Sprint 3 tickets: B4 (HUD damage flash), B5 (ammo pickup no-op), B6 (camera
  pans past map edges). Each: one `fix(scope):` commit + DoD ritual.

### DEV — done (B4)
- **B4 fixed** — `fix(ui): HUD damage flash fires on player HP drop`.
  - Added `hud_track_player_hp()` (hud.c/h): records HP per frame, arms the red
    flash on any HP *drop*; first call sets baseline (no flash), healing never
    false-triggers. `hud_draw` calls it each frame.
  - Added `test_hud_damage_flash` (6 checks: baseline, same-HP, drop-arms,
    decay, re-arm, heal-no-flash).
- **Verification**: zero-warning build, `ctest` 1/1 pass, `zombie_tests`
  268/268 (was 262).
- Bugs sheet + sprint board updated.

### PM — next
- B5 (ammo pickup grants owned-weapon ammo), then B6 (camera clamp).

### Blocked
- None.

---

## Session 2026-09-29 (cont.) — Sprint 3 DEV: B5 DONE

### PM standup — planned / in progress
- B5 ticket: make `ITEM_AMMO` pickups meaningful.

### DEV — done (B5)
- **B5 fixed** — `fix(items): ammo pickup refills owned consumables`.
  - `items_check_pickup()` now takes `PlayerInventory*`; `ITEM_AMMO` grants +1
    grenade when grenades are unlocked, +2 rockets when the launcher is
    unlocked (pistol-only fresh runs stay a safe no-op).
  - Call site updated in `src/main.c`; added `test_item_ammo_pickup` (10 checks).
- **Verification**: zero-warning build, `ctest` 1/1 pass, `zombie_tests`
  278/278 (was 268).

### PM — next
- B6 (camera world-bounds clamp).

### Blocked
- None.

---

## Session 2026-09-29 (cont.) — Sprint 3 DEV: B6 DONE → Sprint complete

### PM standup — planned / in progress
- B6 ticket: stop the camera panning into the void past map edges.

### DEV — done (B6)
- **B6 fixed** — `fix(camera): clamp view inside world bounds`.
  - Added `camera_clamp_world()` (camera.c/h): clamps camera center to
    `[half-viewport, world-size - half-viewport]`, centers when the world fits
    the viewport. Wired after `camera_follow` in `main.c` each playing frame.
  - Added `test_camera_clamp_world` (7 checks).
- **Verification**: zero-warning build, `ctest` 1/1 pass, `zombie_tests`
  285/285 (was 278).

### PM — review
- Sprint 3 closed: B4 `81f3bd3`, B5 `da515eb`, B6 (this commit). All three
  bugs from the code sweep FIXED with tests; suite grew 262 → 285.

### Blocked
- None.
---

## Session 2026-10-05 — net-protocol → `main`, then a UI/asset/multiplayer audit

### PM — planned
- Merge the whole of `feature/net-protocol` into `main` and push it.
- Resume the audit of UI, assets and multiplayer for real bugs, incomplete
  behaviour, and wrong implementations.

### DEV — done (merge)
- **I5 retrospective written** (`docs/I5_FIX.md`, `c8a5dde`) and indexed in
  `docs/README.md`. Documents the fix as *transmitting appearance* — and, more
  usefully, why the first recorded decision for I5 (mirror the host's shape
  constructors client-side, explicitly rejecting sprite geometry on the wire) was
  wrong and got reversed: duplicating the shape rule makes two windows agree only
  until one side changes, so the client keeps guessing and now there are two
  guesses to keep in sync.
- **Merged to `main`** as a **fast-forward** (`398f8d1..c8a5dde`), pushed.
  `main` was sitting exactly on `feature/multiplayer`'s tip, so nothing could
  conflict and no merge commit was needed. Untracked WIP in the main worktree
  (`docs/CROSS_BRANCH_CONFLICTS.md`, `playtesting_prompts/qa_recheck/findings/`,
  `session-ses_f745.md`) was left untouched throughout.
- **Gates re-verified in `main`'s own worktree**, not inherited from the branch:
  Release zero warnings, `zombie_tests` 1186/1186, `ctest` 3/3, seed-42
  single-player log byte-identical.

### DEV — done (audit: UI)
Read the UI layer; verified every candidate against the code before filing it.
**Two of five did not survive as live bugs** — both unreachable, and now
ticketed in `docs/FUTURE_IDEAS.md` (Engineering, deferred) rather than closed.
Recorded in `docs/BUGS.md` so they are not re-raised as new findings:
- Health-bar ratio exceeding its track, and a divide by zero — unreachable. Every
  write to `hp->current` clamps (`items.c:110`, `waves.c:252`) or derives from a
  percentage the CLI already clamps to 0-100, and `hp->max` is never assigned
  after init. (The original claim also named `--player-hp-pct`; the flag is
  `--player-hp=` and it *is* clamped.)
- `ecs_get_health(ecs, ECS_NULL_ENTITY)` reading one past `World.healths` — real
  in the accessor, but unreachable from `hud_draw`, which returns earlier when
  `!alive`. Scoping note written into the ticket: `ecs_get_entity_index` *does*
  check but returns `ECS_MAX_ENTITIES`, which for a 2048-long array is itself one
  past the end — so the existing guard emits exactly the unsafe index, and
  clamping it to `- 1` is the fix that touches no caller.

**"Rejected" was the wrong word for all three**, and that is now corrected in
`BUGS.md`/`PENDING_VERIFICATION.md`: closing a real defect as "not a bug" because
something else happens to check is how it gets re-introduced silently. They are
deferred with their reachability evidence intact.

Also pruned a stale entry: `FUTURE_IDEAS.md` still carried "camera world-bounds
clamp so the void beyond map edges isn't visible" as open work, but B6 fixed it in
`8967a99` (`src/world/camera.c:27`, applied at `src/main.c:867`) and
`test_camera_clamp_world` covers it. Removed rather than reworded — a finished
item listed as a backlog idea invites someone to do it twice.

Three confirmed, all fixed:
- **B32 (`e5c6dec`)** — `hud_draw`'s `multi` argument was `!render_only_client()`,
  which is true in single-player, true as host, false as client: right only for
  the host. A client whose player died got a **completely blank HUD**, because
  `hud_draw` does `if (!alive) { if (multi) {...} return; }` and skipped the
  overlay entirely; and the name tag never drew on a client at all.
- **B33 (`d5da693`)** — **my own regression from N5.** The snapshot-derived shop
  view was threaded into `shop_menu_update()` but not `shop_menu_draw()`, so the
  client menu decided what you may buy from host state and rendered the numbers
  from its own frozen struct. No test caught it: the N5 assertions cover
  `weapons_shop_apply()`, the codecs and the loopback, and none of them care
  which pointer the draw call receives.
- **B34 (`50a2365`)** — the name tag drew at y=76 against the zombie counter at
  y=75. Only reachable on a host, because B32 meant it never drew on a client;
  fixing B32 exposes it on both windows, so they land together.

### DEV — done (audit: assets)
- `gen_assets.py --verify` — **33 textures, byte-identical regen**, tree clean.
- `NET_ART_*` table is complete in both directions: all 9 entity textures have an
  art id, all 9 ids are used, every `textures/**.png` referenced in `src/` exists
  on disk. (`textures/ground.png` appears only inside a doc comment.)
- No zombie size/speed variants are generated, so there is nothing for the art
  table to disagree about.

### DEV — done (audit: multiplayer)
Swept for remaining client-invented state. Beacons (B27), wave panel (B26), health
bars (B28), appearance (B19-B21), per-slot HUD state (B22/B23) and the shop
(B30/B33) all read host-authoritative values. One deferred candidate recorded:
a client's slot-0 beacon is skipped by `sync_client_beacon_slots()` and so never
read from the snapshot — it agrees today only because `beacon_pos` is written
once at respawn and never moved. Follow-up, not a bug.

One further bug, found *because* B32 made the name tag visible on clients:
- **B35 (`7d0cb15`)** — the tag drew `local->name` / `local->color` from the
  unsimulated slot-0 `Player`, whose name is the literal `"Player"` and whose
  colour is the single-player default. A client that joined as `Guest` saw
  `Player` in the wrong colour. Identity now comes from the host's roster via
  `HudPlayerState`, the same path the lobby screen already used.

Worth recording as a class: **a tag that draws nothing cannot be wrong**, so any
fix that makes it start drawing has to check *what* it now draws.

### PM — review
Four bugs found and fixed in one audit pass; four commits, one per fix. Suite
held at 1186/1186, `ctest` 3/3, Release zero warnings, single-player
byte-identical — unchanged throughout, because none of the fixes touch the
simulation. All four are visual/HUD defects, so **the gates prove nothing about
them**; the two-window checklist is in `docs/PENDING_VERIFICATION.md`.

Also repaired documentation drift: `docs/BUGS.md`'s tracking table had stopped at
B23 with B22/B23 still marked PLANNED though both shipped in `e9d2d71`; B24-B30
had no rows at all; and B27/B28 cited `f09cb29` as their fix when that commit is
**docs-only** — the fix is `56cce96`. Misattributing a planning doc to a code fix
is how a bug gets re-declared open.

### Blocked
- Nothing blocking.
- Every fix in this session is visually unverified by construction: `render()`
  returns immediately when `game.headless` is set.

## Session 2026-10-06 — Gates re-verified, co-op mechanism check, backlog

### PM — decisions committed first
- **`458fb9b`** — the two unreachable UI findings were *deferred*, not rejected;
  retitled the section in `BUGS.md`, `PROGRESS.md` and `PENDING_VERIFICATION.md`
  and filed them as tickets in `FUTURE_IDEAS.md` (health-bar guards, slot-0
  beacon, `ecs.h` accessors). The ECS ticket was rewritten after reading
  `ecs_get_entity_index()` (`src/ecs/ecs.c:73`): it already guards, but returns
  `ECS_MAX_ENTITIES`, which is one past the end of a 2048-long array — the cheap
  fix is to return `ECS_MAX_ENTITIES - 1`, and the earlier "return NULL" advice
  was wrong because the accessors are `static inline` in the header.
- **`4b3df18`** — Windows build/run parked as a low-priority backlog task at the
  user's instruction, with the reported symptoms (blank menu, assorted visual
  problems on an old build) recorded as *to re-confirm*, not as diagnosed cause.
  The user also scoped this session: confirm mechanisms, don't go deep on visuals.

### DEV — gates, re-run against the current tree
- Release `-Werror` build clean; `zombie_tests` 1186/1186; `ctest` 3/3.
- **Single-player byte-identity holds**: two `--seed=42 --run-seconds=25
  --ai=bot --headless` runs produce **byte-identical** logs (125938 bytes,
  1603 lines), wave 1 is exactly `8 zombies, interval: 1.90s, difficulty: 1.00`,
  and the log ends on `t=25.000` — i.e. it is complete, not truncated.

### DEV — co-op mechanism check (headless host + client, both `--ai=bot`)
Host `--auto-start --run-seconds=60 --seed=42`, client joined 8s later with
`--run-seconds=52 --seed=43`, events written under `build/verify/`.
- Host: `WAVE 1 STARTED (8 zombies, interval: 1.90s, difficulty: 1.00)` →
  `broadcast player list (2 players)` → `Spawned remote player entity for slot 1
  ('Guest')` → `WAVE 1 COMPLETE (total kills: 8)` → `WAVE 2 STARTED (13 zombies,
  interval: 1.75s, difficulty: 1.15)`.
- Client: 52× `CLIENT mirror seq=... wave=2 ents=17`, i.e. the mirror tracks the
  host through a wave transition.
- **The client's gameplay events match the host's counts exactly** — `KILL`
  16/16, `POINTS` 15/15, `DAMAGE` 82/82, `ITEM_PICKUP` 1/1, `PLAYER_HEALTH` 1/1.
  Client `ENTITY_SPAWN` 0 is expected: the client builds its view from the
  snapshot mirror, not a local spawner. Both logs run to their full
  `--run-seconds` (`t=60.000` / `t=52.000`).

### Finding — `--auto-start` makes wave 1 single-player sized (harness artifact, not a bug)
`waves_start_wave()` takes the budget from `player_count` *at the moment the wave
starts*. With `--auto-start` the host starts the match before any client exists,
so wave 1 is computed as one player (8 zombies) and its `WAVE_START` is relayed
before the client has connected — a client joining that way never sees it.
Wave 2, started with both players present, correctly applies the per-player
bonus (13 = 11 + 2). Real play is unaffected: the host presses Enter only after
clients have joined. Recorded here rather than as a bug row.

### DEV — false alarm worth recording: `/tmp` is quota'd
Empty and 4096-byte-aligned event logs looked like an unflushed exit (SIGKILL
before `event_bus_flush`). It was neither: `/tmp` is a RAM-backed tmpfs hitting
a per-user quota (`EDQUOT — Disk quota exceeded`), so writes silently produced
0-byte files while pipes still worked. `event_bus_flush()` ends in `fflush()` every
frame and the SP log terminates exactly on `--run-seconds`, so the bus is fine.
**Write verification artifacts under `build/` (ignored, real disk), never
`/tmp`.** Scratch was moved to `build/verify/` so it cannot be staged.

### DEV — numeric re-check of the name-tag geometry (B34)
Pixel analysis of a captured host window during an active wave: red counter ink
occupies y79–91, blue name-tag ink y104–116 → a 13 px gap, no overlap, and the
host's own tag is present in slot-0 blue. This confirms geometry only — the text
content was never read, and the guest frame showed no red/blue band at all, so
B35's colour on the *client* is still unproven.

### Blocked
- Visual confirmation remains open by construction: no `grim`/`gnome-screenshot`/
  `xdotool`/imagemagick, the GNOME screenshot portal returns response `2`, and
  XWayland's root is not the composited screen so region grabs come back black
  (`ffmpeg -f x11grab -window_id <xid>` does work). The model also cannot view
  images, so every check stays numeric. `docs/PENDING_VERIFICATION.md`'s
  two-window checklist (B32 client death screen, SP overlay absent) is still the
  outstanding item.

---

## Session 2026-10-22 — Animation transport spike (AT3): DESIGN A wins, measured

### PM standup — planned
- AT3: run the Phase-0 spike and decide frame storage and wire design **from
  measurement, not argument**; commit the verdict before any implementation
  code; answer net's NT4 sword question in the same verdict.
- Plan of record: `docs/knowledge/animation-plan.md` §4 (Spikes A/B/C), §8
  (verdict slots), `docs/knowledge/animation-transport.md` (the parked A-vs-B
  decision).

### DEV — done (spike; no implementation code)
- **Spike A — frame storage.** One 8-frame zombie walk cycle, both ways, then
  thrown away. Individual PNGs: 8 files, 2234 bytes, generator +24 lines,
  `--verify` 0.11 s, clean tree; a one-frame change touches **1 file**
  (`zombie_walk3.png`). Sheet: 1 file (256×32, 629 bytes), generator +30 lines,
  `--verify` 0.11 s; a one-frame change touches **1 file** but re-encodes the
  whole sheet (629 → 709 bytes; 77 pixels differ, all inside one frame cell).
  **The decider was not the file count.** `asset_manager_name_of()` resolves a
  texture to a *path*, and all 8 frames of a sheet share one path, so
  `net_art_from_path()` hands back the **same art id for every frame** — the
  frame identity is gone before it reaches the wire. Making a sheet work was
  measured by writing it: `net_art.h` +8/-1, `net_art.c` +39/-10, **one line
  in `src/net/net_snapshot.c` (net's file)**, `render.c` +5/-1, `tests/tests.c`
  +5/-2 — and the current art-table test **fails 7 times** until its invariant
  becomes `(path,src) → id`. With individual files: 8 distinct paths → 8
  distinct art ids → **no change outside the art table, and no client render
  change at all** (`system_render_mirror()` already resolves art id → path →
  texture).
- **Spike B — the gate cannot see animation.** `COMP_ANIMATION` attached to
  zombies: `total_frames=1` → ctest 3/3, `zombie_tests` 1186/0, two
  same-seed SP runs diff-empty (125938 bytes / 1603 lines) and identical to the
  pre-spike baseline; `total_frames=8` at 8 fps → the same green gate, the same
  byte-identical SP runs. Only `render.c` and `net_snapshot.c` ever read
  `CSprite`; every other reference in the tree is a write at spawn.
- **Spike C — 20 Hz aliasing, on the real `system_animation`.** Host ticks at
  60 Hz, snapshot at `NET_SNAP_HZ` 20, the client holds the last art id for one
  transport interval (50 ms). Per 8-frame cycle, over 8 s: **30 fps → 5..6 of 8
  frames (68 skipped steps); 24 fps → 6..7 of 8 (30 skipped); 20 fps → 7..8 of
  8 (0 skipped); 15 fps and slower → 8 of 8, every cycle, 0 skipped, 0
  backwards.** The cycle skips below ~15 fps; it **never runs backwards** at
  any rate, because the client only holds ids it was given.
- **Verdict (committed): DESIGN A, individual PNGs, frame period >= 67 ms
  (<= 15 fps, target 10 fps).** No new snapshot field, no `NET_WIRE_VERSION`
  bump (stays 8), no `NET_SNAP_ENTRY_BYTES` change (stays 35). The frame index
  lives in the ECS (`COMP_ANIMATION.current_frame`); frame → art id → texture
  is a render-side table. Art id budget: 10 today, 96 for a realistic content
  set → 106 of 255, 149 spare. **B is not built.**
- **NT4 (sword) answered in the same verdict, as net asked: (b) procedural
  blade.** The blade angle is continuous and unbounded (5.0 rad/s = 286°/s), so
  frame-based sword art would need ≥ 12.7 frames/s — exactly on the measured
  aliasing limit — and would change the single-player picture. The snapshot
  carries the angle (int16 centidegrees, wrapping at 180°); the client draws
  the blade with the same `sprite_draw_blade()` path and interpolates
  wrap-safe between two host frames. Fields: kind + owner + art + size_q +
  tint + angle, plus owner D1's extensible attribute block. Rides B38's single
  wire bump.
- **Housekeeping:** every spike change was thrown away — the tree is clean at
  `68280a3`, `git status --short -- src/net` is empty (net's condition for the
  measurement patch), and the gate is green again (3/3, 1186/0, 33 textures,
  two SP runs byte-identical).

### PM — next
- Rebuild the task board from the dev's own breakdown (AT3 closed by this
  commit; AT4 implementation, AT5 two-window check).
- AT4 starts from this verdict: art table append + `NET_ASSET_VERSION` 1 → 2,
  generator support for one walk cycle, the frame-selection function, and the
  <= 15 fps rule written into the content code.

### Blocked
- **AT5 / Phase D stays blocked on a human** watching two windows and
  confirming I5 renders ([i5-unverified-on-screen]). The spike's verdict is
  arithmetic; the walk cycle's on-screen look is **unproven** and is recorded
  in `docs/PENDING_VERIFICATION.md`.
- Net's freeze is not active yet: post "starting" immediately before the art
  append commit and wait for net to confirm.

## 2026-10-22 — animation (DEV)

### AN2 — generator support for one walk cycle (done, `18c95af`)
- The generator emits the zombie walk cycle as EIGHT individual 32x32 PNGs
  (`tools/gen_assets.py`: `_zombie_walk_grid` + `entity_zombie_walk`), because
  an art id is resolved from a texture's PATH — frames of one sheet cannot be
  named, so they cannot travel.
- Placeholder poses: the idle blob, the trailing arm swinging, the reaching arm
  the other way, the body bobbing twice a cycle, the head lopping down over
  the second half of it (a limp, and the term that makes all eight frames
  differ). Frame 0 is the idle pose EXACTLY, so a zombie that starts walking
  does not pop. All eight frames are pairwise distinct; the art is hand-tuned
  in AN5.
- `--verify` now covers them: 33 → 41 textures, declared in
  `docs/knowledge/gate-baseline.md` in the same commit.

### AN1 — the art table names the eight walk frames (done, `35a8b46`)
- `net_art.h`/`net_art.c`: `NET_ART_ZOMBIE_WALK0 = 10` .. `WALK7 = 17`,
  `NET_ART_COUNT` 10 → 18, `NET_ART_ZOMBIE_WALK_COUNT 8`. Append only.
- `src/net/net.h`: `NET_ASSET_VERSION` 1 → 2 (one line; the version bump rides
  with the rows because the PNGs of the previous commit are inert until the ids
  that name them exist). `NET_WIRE_VERSION` stays 8, `NET_SNAP_ENTRY_BYTES`
  stays 35 — no wire field was added.
- Net's freeze was taken and released around this commit (net confirmed, I
  posted the SHA).
- The art table test now checks each new row resolves to the expected file name
  and that the file is really shipped: 1186 → 1253 checks (declared in the same
  baseline commit).

### AN3 — the frame-selection function and the host wiring (this commit)
- `src/graphics/anim.{h,c}` (new): `anim_walk_art_id(frame)`,
  `anim_walk_path(frame)`, `anim_walk_texture(frame)`. The frame number maps to
  an art id by adding; the art table's contiguity is asserted at compile time.
- `src/systems/animation.c` (new): `system_animation()` moved out of
  `render.c` and given the wiring — the frame's texture goes ON THE SPRITE.
- `src/world/waves.c`: a spawned zombie gets `COMP_ANIMATION` and starts on
  frame 0 (the idle pose).
- **Why the sprite:** the renderer draws it, the snapshot builder reads the
  texture's name out of it, and the client resolves the art id it carries. One
  answer to "which frame is this", so the host and the client cannot drift.
  `net_snapshot.c`, `net_codec.c` and the client's render path are UNTOUCHED.
- The rate is now code: `ANIM_FRAME_PERIOD` = 0.1 s (10 fps), with the ≤ 15 fps
  ceiling written beside it. AN4 pins that rule with a test.
- Tests: `test_animation_frames` (the frame → id → file mapping, the wrap, the
  distinctness) and `test_animation_frame_advance` (a zombie is on the cycle,
  it advances on time, it wraps). The texture itself cannot be checked — the
  test binary is headless, so there is no texture to swap.

### Gate (this commit)
- build clean (zero warnings in project sources), `ctest` 3/3, `zombie_tests`
  1253 → 1347 / 0, `gen_assets.py --verify` 41 textures with a clean tree
  afterwards, two `--seed=42` single-player runs byte-identical (125938 bytes /
  1603 lines), wave 1 still `8 zombies, interval: 1.90s, difficulty: 1.00`.
- The on-screen walk cycle is still **UNPROVEN** — a gate cannot see a picture.

### Next
- AN4 (rate rule as code: the ≤ 15 fps constant with a test that fails if the
  period is too short), then AN5 (content: walk/attack/death, pickup bob,
  missing-texture fallback), then AN7 (the sword art ids and the angle contract
  for net).

### AN4 — the rate rule as code (≤ 15 fps constant + a test with teeth)
- `src/graphics/anim.h`: `ANIM_MAX_FPS 15`, `ANIM_FRAME_PERIOD_MIN
  (1.0f / ANIM_MAX_FPS)`, and `ANIM_FRAME_PERIOD` (0.1 s = 10 fps) checked
  against them.
- `tests/tests.c`: `test_animation_rate_rule` simulates the CLIENT the way the
  network does it — the host ticks at 60 Hz, a snapshot goes out every
  `NET_SNAP_HZ` (20), the client holds the last art id it was given — and
  counts, per cycle, how many of the 8 frames the client saw and how many
  steps it missed.
- **The teeth were checked by breaking it on purpose:** with the period set to
  1/30 the suite fails on the constant AND on the behaviour (5..6 of 8 frames
  per cycle, steps missed). Restored: green.
- The simulation reproduces the spike's table in float: 10 fps 8/8, 15 fps 8/8,
  12 fps 8/8, 24 fps 6/8 with 2 missed per cycle. The 15 fps boundary is safe
  but tight (4 host ticks per frame, so one tick of jitter can cost a frame) —
  which is why the project runs at 10 fps (6 ticks per frame).
- Gate: build clean, ctest 3/3, `zombie_tests` 1347 → 1382 / 0 (declared in
  `docs/knowledge/gate-baseline.md` in the same commit), `gen_assets.py
  --verify` 41 textures, two `--seed=42` SP runs byte-identical (125938 bytes /
  1603 lines).

### AN7 — the sword angle contract (no new code; the contract recorded)
- Net's v9 (`99f8db4`) landed first and consumed what the animation lane
  already had: `NET_ART_SWORD = 9`, `textures/entities/sword_blade.png`,
  `base_px 40`, and the host spawn already gives the blade that texture. AN7
  therefore needed **no animation code change** — no new art row, no
  `NET_ASSET_VERSION` bump, no `net.h` edit.
- Verified against the art: the PNG is 40x16 and the generator draws the
  handle on the left, tip on the right, so `sprite_draw_blade`'s handle->tip
  texture mapping is the right way round.
- Recorded the contract in `docs/knowledge/animation-transport.md`: signed
  int16 **centidegrees** in (-18000, +18000], wrap-safe interpolation through
  180, `size_q` = blade length, width = separate attr byte. Net confirmed the
  "signed, not unsigned" point independently — an unsigned half-turn would
  draw half the spin backwards.
- Replied to net: AN7 does not change the id/size/pivot, so it is not a v9
  blocker.
- Added `docs/PENDING_VERIFICATION.md` M9 for the client's mirrored sword
  (angle/width/tint/wrap) — v9 draws it, but headless cannot see it.

### AN5 — the pose follows the state; the pickup bob; the missing-art fallback
- `src/systems/animation.c`: the pose now follows `CZombieTag.state` — CHASE
  walks, ATTACK holds `ANIM_ATTACK_FRAME` (frame 6, the cycle's furthest
  reach, so it reads as a lunge instead of walking in place), HURT/DEAD
  freeze, IDLE holds frame 0. Entering ATTACK/IDLE resets the frame timer.
- `src/items/items.c` + `ecs.h`: the pickup bob. `items_tick_bob()` moves the
  entity's `CPosition` (drawn and replicated) while `CItemTag.base_pos` keeps
  the true position for the pickup range and the `GE_ITEM_PICKUP` payload.
  Display-only by construction: the single-player events log is unchanged
  byte-for-byte, which is why the split exists. Vertical, 1.5 Hz, 2.5 px,
  accumulated from `dt` (no `rand()`, so the seed-RNG order is untouched).
- Missing-art fallback tested: headless (no asset manager) the zombie keeps
  its flat shape and the frame index still advances.
- No death pose, deliberately: a zombie is destroyed the instant it dies and
  the death is its particle burst; a corpse state would delay destruction and
  move the death timestamp in the events log (single-player invariance).
  Flagged to pm as a gameplay item if wanted.
- Gate: build clean, ctest 3/3, `zombie_tests` 1382 → 1654 / 0 (declared in
  `docs/knowledge/gate-baseline.md` same commit), `gen_assets.py --verify` 41,
  SP runs byte-identical (125938 bytes / 1603 lines), wave 1 unchanged.

### AN8 - the player walk cycle (2026-10-22)

- `13621b8` art: `_player_walk_grid()` / `entity_player_walk()` in
  `tools/gen_assets.py`, eight 32x32 PNGs. The art has no legs, so the walk is
  the arm swing + a 1 px lift/sag; the eight poses are a table because a pure
  sine swing makes frame 0 and frame 4 identical. Frame 0 is pixel-identical to
  `player.png` (asserted by the generator and compared as files), so starting
  to move does not pop. `--verify` 41 -> 49.
- `e4f730f` ids: `NET_ART_PLAYER_WALK0..7 = 18..25`, `NET_ART_COUNT 26`,
  `NET_ASSET_VERSION 2 -> 3` (WIRE stays 8, the frame is still the art id),
  `test_net_art_table` mirrored for the player cycle. Landed under net's freeze,
  which was released back to net in the same pass.
- `AN8 wiring`: `players.c` spawn gets `COMP_ANIMATION` (frame 0, 8 frames,
  `ANIM_FRAME_PERIOD`); `system_animation()` reads the player's velocity and
  holds frame 0 while it is zero, so a standing player does not run in place and
  stopping returns to the idle pose; `anim_show_frame()` picks the player frames
  by `COMP_PLAYER_TAG` so the renderer and the snapshot builder agree.
  `anim_player_walk_*()` added beside the zombie helpers.
- Gate: build clean, ctest 3/3, `zombie_tests` 1721 -> 1759 / 0 (declared in
  `docs/knowledge/gate-baseline.md` same commit), `--verify` 49, SP runs
  byte-identical (125938 bytes / 1603 lines). Teeth: forcing the movement gate
  open fails four checks.
- Unproven on screen (PENDING_VERIFICATION M11): does the walk read as walking,
  and does the mirrored player show the same frame on the other peer.
## Session 2026-10-22 — Sprint 6 DEV (net): NT2 GE_EXPLOSION

### Planned
Land `GE_EXPLOSION` (D3) as a relayed blast event with a locked payload
(`x,y` origin, `a` radius, `b` spare, `kind` grenade/rocket), no wire bump, and
re-measure the single-player gate reference.

### Done
- Added `GE_EXPLOSION` before `GE_COUNT` in `src/events/event_bus.h` (no existing
  enum value moves) and `EXPLOSION` to `event_type_names[]` (`event_bus.c`).
- Emitted host-side in `grenades.c` (`explode_grenade`, `a=90.0`) and
  `rockets.c` (terminal lifetime/out-of-bounds, `a=ROCKET_RADIUS`).
- Relayed in `relay_net_events()` (`src/main.c`), and removed the duplicated
  `GE_PLAYER_SHOT` clause in that filter while adding the new type.
- Tests: extended `test_grenade_detonation` and
  `test_rocket_damage_and_destruction` to init a bus and assert the logged
  `EVT=EXPLOSION` with the right `kind` and `a`. Suite now 1194/0.
- Follow-up from audio's AT6 finding: a rocket's blast now emits at its FIRST
  CONTACT (`CRocketTag.impact_emitted`), not at expiry/out-of-bounds. That
  branch is a despawn (it also covers "flew off the map") and now emits only
  `GE_ENTITY_DEATH`; the rocket pierces, so the first hit is the impact. Gate
  unchanged (build clean, ctest 3/3, 1194/0, SP 125938/1603 identical).
- Updated `docs/knowledge/gate-baseline.md` and `relayed-events.md`; replaced the
  copied gate literals in `docs/QA_MULTIPLAYER_S5.md` with a citation.

### Measured gate
- build `-Werror` clean; `ctest` 3/3 (three targets: net_spike, net_loopback,
  core_tests); `zombie_tests` **1194 passed / 0 failed**.
- Observed once: a single `net_spike` failure on the first `ctest` run, then
  four consecutive full runs each 3/3. Not reproduced, no code correlation with
  this change (net_spike does not touch the event bus); recorded here so a
  recurrence is treated as a real bug rather than another flake.
- Two seed-42 25s SP runs byte-identical at **125938 bytes / 1603 lines —
  unchanged**, because the bot only fires the pistol and never detonates an
  explosive. So the events reference did *not* move; only the test count did.
- `gen_assets.py --verify` not run (no asset change).

### Next
- Coordinate landing with [audio] (AT7) and report the corrected re-baseline
  finding to [pm]: the SP log reference is unchanged, contrary to the earlier
  prediction.

## Session 2026-10-22 — Sprint 6 DEV (net): wire v9 — B38 u16 count + NT4 sword

### Planned
Land the single `NET_WIRE_VERSION` 8 -> 9 commit pm specified: B38's entity
count widened to `u16`, plus NT4's sword replication via a trailing
extension-attribute table (angle + width), with client mirror interpolation.
Freeze lifted by animation when AN1 landed (35a8b46).

### Done
- `net.h` v9: `count` -> `uint16_t`; `NET_ENT_SWORD = 7`; new
  `NetEntityAttr` + `attr_count`/`attrs[]` on `NetSnapshot`; header count term
  1 -> 2; `NET_SNAP_MAX_BYTES` includes the attr table. `NET_SNAP_ENTRY_BYTES`
  stays 35.
- Codec: encode/decode the u16 count and the attr table; unknown attr kinds are
  preserved rather than rejected; bounds-checked on both sides.
- Snapshot builder: swords are no longer skipped. They carry `owner`, the art
  id, `size_q` = **blade length**, and one attr `{signed int16 centidegrees,
  u8 width_q}`. Signed because the blade is drawn from a pivot.
- Client: `net_mirror_sample_attr()` blends the angle the short way around the
  circle; `system_render_mirror()` draws the sword with `sprite_draw_blade()`.
- Tests: `test_net_snapshot_u16_count` (300 entities), sword attr round-trip,
  a builder-level check that a real ECS sword is emitted as NET_ENT_SWORD with
  the host's length/width/angle, and wrap-safe mirror interpolation. Suite
  **1235/0**.
- Docs: VERSIONING registry/worked note, BUGS B38 resolution, gate baseline.
- Review follow-up (33c4663): `net_mirror_push_removing()` compacted entities
  but not the new attribute table, so a dead sword's angle survived in the
  older/newer snapshots and a recycled id would blend through it. Attributes are
  now stripped by the same dead-id test; test asserts both snapshots and the
  sampler forget the id.
- B40: added its missing bug-table row and wrote the two-instance sword checks
  into `PENDING_VERIFICATION.md` as UNPROVEN (headless cannot see the blade).

### Measured gate
- build `-Werror` clean; `zombie_tests` **1235 passed / 0 failed**; `ctest` 3/3.
- Two seed-42 25s SP runs byte-identical at **125938 bytes / 1603 lines —
  unchanged** (snapshots are host-only and no relay runs in SP).
- `gen_assets.py --verify` not run (no asset change on this lane).

### Next
- Merge/rebase with animation's AN1 (this lane predates it) and audio's Phase D;
  re-run the integrated gate before landing. Do not push (pm's one-time push go
  covered only cd7e8ea).

---

## Session 2026-10-23 — Landing: net to main (NT2 + wire v9)

### Planned (pm standup)
- After animation lands on main, merge `feature/net-protocol` into main (local,
  no push), resolve the append-only docs conflicts by keeping both sides, and
  re-measure the gate on the actual main tree instead of quoting either lane's
  number.

### Done
- **`6801d7c`** `merge: land feature/net-protocol into main (NT2 + wire v9)`.
  - NT2 `GE_EXPLOSION` (locked payload; rocket blast at first contact).
  - Wire v9: B38 count `u8` -> `u16` (1024 cap kept), NT4 `NET_ENT_SWORD` with
    a signed-centidegree angle and the extensible attribute table.
  - Conflicts were only `docs/PROGRESS.md`, `docs/PENDING_VERIFICATION.md`,
    `docs/knowledge/gate-baseline.md`; resolved by keeping both sides.
- Merged gate on main: `zombie_tests` **1808/0**, ctest 3/3, `--verify` 49
  textures, two seed-42 SP runs byte-identical **125938 bytes / 1603 lines**,
  wave 1 unchanged. Recorded in `docs/knowledge/gate-baseline.md`.
- Headless two-instance on main: host spawned the remote slot, client mirror
  `wave=1 ents=2`, no rejects.
- Board: NT2, NT3, NT4 -> done (NT5 was already done).
- `render.c` checked at integration: net's mirrored-sword block and animation's
  `system_animation()` removal both survive.

### Next
- Audio can land Phase D on this main; net will confirm NT2 is present.
- Human checks remain: B40 sword on screen and AN8/M11 mirrored walk
  (`docs/PENDING_VERIFICATION.md`). Headless cannot see either.
- No push; `main` stays local until the owner's next go.

## Session 2026-10-22 — AT6 audio Phase-0 spike: backend chosen, triggers audited, double-fire ruled out, latency measured

### PM standup — planned
- AT6 spikes A (backend), B (trigger audit), C (double-fire), D (relayed
  latency), plus the soft-fail proof, all measured before any implementation.
- Deliverable: `docs/knowledge/audio-plan.md` §10 filled in, raw numbers in a
  measurements topic, verdict committed **before** Phase B writes a line of
  audio code.

### DEV — done
- **Baseline gate re-verified** in `worktrees/audio/build` before touching
  anything: build clean, `ctest` 3/3, `zombie_tests` 1186/0, two `--seed=42`
  single-player runs diff-empty at the [gate-baseline] byte/line reference.
- **Spike A — backend: SDL3's audio API, not vendored miniaudio.** Measured
  both: SDL3 = 0 bytes of binary, 0 s of build, no new link flag, no vendored
  file, no licence to track (SDL3 is already linked and exports
  `SDL_OpenAudioDeviceStream`/`SDL_LoadWAV`/`SDL_ResumeAudioStreamDevice`);
  miniaudio
  0.11.25 = a 4,108,168 B header, **19.27 s** of compile for one TU at `-O3`
  (6.64 s at `-O1`), 646-884 KB of binary, `-ldl -lpthread`. **Both were proven
  to put sound on the speakers** — a 12 kHz tone captured off the PipeWire sink
  monitor and detected with a Goertzel filter (SDL3 −34.0 dBFS, miniaudio
  −29.3 dBFS, silence control −46.6 dBFS), because ambient desktop audio makes
  RMS useless. So this is a cost decision, not a capability one. Losing SDL3's
  mixer costs ~150-250 lines we own and can unit-test.
- **Spike A — soft-fail proven at probe level:** no device
  (`SDL_AUDIODRIVER=nosuchdriver`), missing file (both backends), and an
  explicit no-device mode all degrade to silence with **exit 0**, no assert.
  Game level (`--headless` opens no device) is still a Phase B item.
- **Spike B — trigger audit complete**, one row per intended sound, read from
  the emit sites, not from `EVENT_FORMAT.md` (the table is audio-plan §6).
  Findings that change a design: the client cannot tell *who* hit it (`ib` is
  dropped by the relay); it cannot tell hurt from heal (`ia` is dropped)
  without keeping the previous `GE_PLAYER_HEALTH` event's hp; **a rocket
  impact has no event at all** (only its despawn does) so a rocket-explosion
  sound needs `GE_EXPLOSION` — net's NT2; and **there is no reload** to sound
  (pistol ammo is unlimited; `NetPlayerState` has no reload field).
- **Spike C — double-fire ruled out, by counting.** Two co-op runs (25 s
  headless, 18 s windowed, both peers `--ai=bot`). Every relayed type arrives
  at ratio **1.00** on the client — `PLAYER_SHOT` 102/102 and 63/63, `DAMAGE`
  42/42 and 25/25, `KILL` 10/10 and 5/5, `POINTS` 10/10 and 5/5, `WAVE_START`
  1/1, `WAVE_END` 1/1 — with matching entity ids and no duplicates.
  `ENTITY_SPAWN` is 0 on the client because it is not relayed: expected shape.
- **Spike D — relayed latency measured.** Temporary `CLOCK_MONOTONIC`
  instrumentation at the host relay and the client receive (reverted after;
  tree verified clean). Unthrottled: the relay itself costs **~1 ms**
  (102 matched shots, 0 entity-id mismatches, residual median 0.0 ms).
  Windowed: the client hears a relayed shot after **median 89.6 ms** (min
  52.6, max 106.1) — because the delay is **one host frame + one client frame
  + ~1 ms**, and this machine ran the windowed game at ~25 FPS (40 ms frames).
  At 60 FPS the same path costs ~33 ms. **The delay is frame-bound, not
  network-bound.**
- **Docs written:** `docs/knowledge/audio-measurements.md` (new topic — raw
  numbers, method, reproduction), `docs/knowledge/audio-plan.md` §6 trigger
  table and §10 verdicts, `docs/PENDING_VERIFICATION.md` (the listen-for list
  a human has to work through, and on which peer).
- **No product code touched.** The only source edits during the spike were the
  two temporary instrumentation hunks, both reverted; `git status` is clean
  apart from the docs in this commit.

### PM — next
- Decide the §4 option (c) question the latency measurement opened: play the
  local player's own gunfire locally and dedupe the relayed copy by bullet
  entity id (removes two frames of latency, costs a mirror-side derivation),
  or accept ~33 ms at 60 FPS. **Audio is not re-opening §4** — §4 stays
  closed; this is the PM's call on a measured input.
- Phase B (`src/audio/`, SDL3 backend, init/update/shutdown in `main.c`,
  `AUDIO_TICK` between `relay_net_events()` and `event_bus_flush()`), then
  Phase C (`tools/gen_audio.py`, `assets/audio/**`), then Phase D (consume the
  relayed events), then Phase E (a human listens, two windows).
- `GE_EXPLOSION` stays net's NT2; audio consumes it, audio does not add the
  event type.

### Blocked
- Nothing is blocked. Two open questions are owned by the PM (the option (c)
  latency decision) and one by net (the `GE_EXPLOSION` payload, which audio
  needs for a rocket explosion sound).
- Nothing in this session is `fixed`: no sound has been heard by a human, so
  the audio feature remains **unproven** by construction.
