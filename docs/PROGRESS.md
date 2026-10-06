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
