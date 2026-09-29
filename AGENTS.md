# AGENTS.md — Persistent Team Instructions

These instructions are the source of truth for how this project is run. They are
meant to survive context compression — re-read this file at the start of any new
session and follow it.

## Roles (always maintain this structure)

- **PM (Planner)** plans: defines sprint scope, picks backlog items, writes
  `docs/SPRINT_PLAN.md`, assigns work in todo lists, runs meetings, reviews
  results, and keeps reporting/standup structure.
- **DEV (Worker)** implements: writes code/assets, verifies builds and tests,
  makes small commits, updates `docs/PROGRESS.md` and `docs/BUGS.md`.
- Reporting is done in PM/standup style: what was planned, what was done, what's
  next, what's blocked. Keep the meeting structure visible in replies and in the
  progress log.

## Worktree discipline (critical)

- **Only ever work in the assets worktree**:
  `/home/divyam-redkar/development/personal/SDL3/open-world-zombie-waves-assets`
  (branch `feature/assets-maps`).
- **Do NOT touch** the main worktree (`open-world-zombie-waves`, branch `main`)
  — it holds broken uncommitted WIP in `src/main.c`, `src/ui/menu.c`,
  `src/ui/menu.h`. Never edit, commit, or run from there.
- **Do NOT touch** the multiplayer worktree (`open-world-zombie-waves-multiplayer`,
  branch `feature/multiplayer`).
- All docs/commits land on `feature/assets-maps`.

## Commit discipline (change history matters)

- Commit **one commit per fix / significant change** (user's standing request).
- Use conventional-commit prefixes seen in history:
  `feat(scope):`, `fix(scope):`, `docs(scope):`, `build(scope):`.
- Commit constantly — every completed unit of work gets a commit; the history
  is the record.

## Docs housekeeping (keep these current, they are part of the workflow)

- `docs/BUGS.md` — bug sheet; add an entry (symptom/root cause/fix/status +
  commit ref) for each found bug, mark FIXED when the fix lands.
- `docs/SPRINT_PLAN.md` — current sprint: goals, scope, ticketed bugs/features,
  acceptance, definition of done, owner tags (PM/DEV).
- `docs/FUTURE_IDEAS.md` — backlog of ideas not yet started.
- `docs/PROGRESS.md` — session log: what was found and what was done (with
  commit refs), kept as a running narrative.
- `docs/ASSET_MAP_PLAN.md` / `docs/EXTENDING.md` — asset & API docs; keep in
  sync with shipped code.

## Verification ritual (always before declaring done)

1. `cmake --build build` — must be zero-warning.
2. `ctest --test-dir build` — all tests pass.
3. `./build/zombie_tests` — assertion suite green (currently 262 checks).
4. Art regen when touching assets: `python3 tools/gen_assets.py --verify`.
5. When asked to run the game for playtesting:
   `./build/open_world_zombie_waves` (launch in background, give controls).

## Session start ritual

1. `git -C <assets-worktree> status --short && git -C <assets-worktree> log --oneline -10`
2. Read this file, `docs/PROGRESS.md`, `docs/SPRINT_PLAN.md`.
3. Set up todos (todowrite) reflecting PM plan + assigned DEV work.