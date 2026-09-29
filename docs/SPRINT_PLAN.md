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
| **B4** HUD damage flash never triggers | DEV | High | `hud->damage_flash` is only decremented/drawn (hud.c:8,15-17,189-192); nothing sets it on player hurt. Dead overlay | `hud_track_player_hp()` arms the flash on any HP drop (first call = baseline; healing never false-triggers); `hud_draw` calls it per frame | FIXED `81f3bd3` |
| **B5** Ammo pickup does nothing | DEV | Medium | `ITEM_AMMO` case empty in `items_check_pickup` (items.c:117-119); pistol ammo unlimited | `items_check_pickup` gains a `PlayerInventory*`; ammo pickup refills +1 grenade (grenades unlocked) / +2 rockets (launcher unlocked) | FIXED `da515eb` |
| **B6** Camera shows void at map edges | DEV | Medium | `camera_follow` never clamps to world bounds (camera.c:14-17) | `camera_clamp_world()` clamps camera center to `[half-view, world-size - half-view]`; centers when the world fits the viewport; called after `camera_follow` | FIXED |

## 3. Acceptance criteria (definition of done, per ticket)

- **B4**: player takes a hit → red flash fades; headless check that damage path
  sets the flash (may add a small unit test). — **DONE**: `test_hud_damage_flash`
  covers baseline/no-flash-on-heal/arm-on-drop/decay; overlay draws in `hud_draw`.
- **B5**: picking up `ITEM_AMMO` with grenades/rockets unlocked adds ammo; with
  none granted it remains a safe no-op pickup (no crash). — **DONE**:
  `test_item_ammo_pickup` covers pistol-only no-op and owned-weapon refill.
- **B6**: walking to a map corner keeps the whole view inside the map; camera
  still centers correctly on the player mid-map. — **DONE**:
  `test_camera_clamp_world` covers mid-map no-op, corner clamping, and the
  world-smaller-than-viewport centering case.

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