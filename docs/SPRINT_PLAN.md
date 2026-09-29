# Sprint 3 — Post-Playtest Sweep: Bugs B4–B6

PM: planning/scope/board | DEV: implementation | Status: PLANNED (target: FIXED)
Raised: 2026-09-29 (post-playtest code sweep) | Board: docs/FUTURE_IDEAS.md for parked items

---

## 0. Sprint ceremony / reporting structure (maintained every sprint)

- **Planning (PM)**: pick ticket scope, accept/estimate, record here.
- **Daily standup log**: kept in `docs/PROGRESS.md` (found / done / next /
  blocked per session).
- **Review (PM)**: verify build + tests + playtest; mark tickets FIXED.
- **Retro notes**: appended to PROGRESS for the next sprint.

## 1. Goals

1. Close the three new bugs found in the post-playtest code sweep.
2. Keep the assets/UI polish direction from previous sprints (one FIXED commit
   per ticket).
3. Zero-warning build + full test suite green (262 assertions) after each ticket.

## 2. Tickets (in scope)

| Ticket | Owner | Priority | Root cause (from code sweep) | Plan (DEV) | Status |
|--------|-------|----------|------------------------------|------------|--------|
| **B4** HUD damage flash never triggers | DEV | High | `hud->damage_flash` is only decremented/drawn (hud.c:8,15-17,189-192); nothing sets it on player hurt. Dead overlay | Emit a red flash when `GE_PLAYER_HEALTH` shows HP loss (or direct set in hurt path); decay over ~0.3s | OPEN |
| **B5** Ammo pickup does nothing | DEV | Medium | `ITEM_AMMO` case empty in `items_check_pickup` (items.c:117-119); pistol ammo unlimited | Grant grenades/rockets when the owning weapon is unlocked; still log pickup | OPEN |
| **B6** Camera shows void at map edges | DEV | Medium | `camera_follow` never clamps to world bounds (camera.c:14-17) | Clamp camera center to `[half-view, world-size - half-view]` in `camera_follow`; skip clamp if view bigger than world | OPEN |

## 3. Acceptance criteria (definition of done, per ticket)

- **B4**: player takes a hit → red flash fades; headless check that damage path
  sets the flash (may add a small unit test).
- **B5**: picking up `ITEM_AMMO` with grenades/rockets unlocked adds ammo; with
  none granted it remains a safe no-op pickup (no crash).
- **B6**: walking to a map corner keeps the whole view inside the map; camera
  still centers correctly on the player mid-map.

## 4. Definition of done (all tickets)

1. `cmake --build build` — zero warnings.
2. `ctest --test-dir build` green.
3. `./build/zombie_tests` green (262 checks).
4. One FIXED commit per ticket with `fix(scope):` prefix.
5. `docs/BUGS.md` entries updated (symptom/root cause/fix/status + commit).
6. `docs/PROGRESS.md` standup rows added.

## 5. Not in scope this sprint (→ backlog)

`docs/FUTURE_IDEAS.md`: muzzle flash, screen shake, damage numbers, item bob,
zombie separation, HUD text caching, aim indicator.