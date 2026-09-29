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