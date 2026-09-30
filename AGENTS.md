# AGENTS.md — standing instructions for this worktree

**READ THIS FIRST, EVERY SESSION, INCLUDING AFTER ANY CHAT COMPRESSION.**
If the conversation no longer contains these instructions, they are still binding —
they live here, not in the chat.

> **R13 merge note (2026-09-30).** This file is the **union** of the two
> branches' standing rules: `feature/multiplayer` and `feature/assets-maps`.
> Both used to say "only ever work in *my* worktree", which cannot both be
> true — the collision is resolved below. `docs/CROSS_BRANCH_CONFLICTS.md` is
> the analysis that drove the merge; read it before touching world, sprite, or
> net handshake code, because it documents four silent breaks that git reports
> **no** conflict for.

## Roles (always maintain this structure)

- **PM (Planner)** plans: defines sprint scope, picks backlog items, writes
  `docs/SPRINT_PLAN.md`, assigns work in todo lists, runs meetings, reviews
  results, and keeps reporting/standup structure.
- **DEV (Worker)** implements: writes code/assets, verifies builds and tests,
  makes small commits, updates `docs/PROGRESS.md` and `docs/BUGS.md`.
- Reporting is done in PM/standup style: what was planned, what was done, what's
  next, what's blocked. Keep the meeting structure visible in replies and in the
  progress log.

## Worktree discipline

- Integration work happens in the **multiplayer worktree**
  `/home/divyam-redkar/development/personal/SDL3/open-world-zombie-waves-multiplayer`
  (branch `feature/multiplayer`), which now contains both engines.
- Prefer absolute paths plus `git -C "$R"`. Shell cwd and relative paths have
  drifted in this repo before.
- The `main` worktree (`open-world-zombie-waves`) and the assets worktree
  (`open-world-zombie-waves-assets`) hold uncommitted WIP. Do not edit or commit
  in either until their WIP is committed or the user says otherwise.
- **Never** use the legacy sibling `open-world-zombie-waves` tree as a scratch
  build directory. A sibling tree appearing at `/home/divyam-redkar/...` or
  `/root/divyam-redkar/...` is not this worktree unless it ends in
  `-multiplayer`.

## Workflow rules

1. **One commit per fix.** Never batch unrelated fixes into a single commit.
   If several fixes touch the same file, stage them in separate passes
   (`git checkout -- <file>`, re-apply one change, commit, repeat).
2. **Keep a todo list and update it as you go** — mark `in_progress` before
   starting an item and `completed` only after its verification actually passes.
3. **Track bugs in `docs/R13_BUGFIX_TRACKER.md`** and `docs/BUGS.md`. Add a row
   for every finding, and update the row's commit SHA and status when it lands.
   These files are the durable record; the chat is not.
4. **Commit the plan/decision before implementing it.** Write the decision down
   (tracker or a short ADR) so the reasoning survives.
5. Do not commit another agent's reports as if they were yours. The playtester
   agent's files under `playtesting_prompts/findings/` are left untracked unless
   the user asks otherwise.
6. Use conventional-commit prefixes seen in history: `feat(scope):`,
   `fix(scope):`, `docs(scope):`, `build(scope):`, `merge:`. Commit constantly —
   every completed unit of work gets a commit; the history is the record.

## Non-negotiable gate: single-player byte-identity

`feature/multiplayer` exists to add multiplayer **without changing
single-player**. Every change must pass all of these before it is considered done:

- Clean Release build with `-Werror`, and **zero warnings in project sources**
  (SDL3's own bundled XML-validation warnings are noise — ignore those).
- `ctest` 3/3: `net_spike`, `net_loopback`, `core_tests`.
- Two same-seed single-player runs are `diff`-empty
  (`--headless --ai=bot --run-seconds=25 --events=a.log --seed=42`).
- Single-player wave 1 is exactly `8 zombies, interval: 1.90s, difficulty: 1.00`.
- Multiplayer-only behaviour stays behind `player_count > 1`.

If a change cannot pass the SP gate, it is not ready to commit.

**Why the gate survived the assets merge:** headless runs deliberately keep the
fixed classic 50x50 world (`world_init`), never a map file, so map selection
cannot perturb the sim. Do not "clean that up" — it would break determinism.

## Verification ritual (always before declaring done)

1. `cmake --build build` — zero warnings in project sources.
2. `ctest --test-dir build` — all tests pass.
3. `./build/zombie_tests` — assertion suite green.
4. Art regen when touching assets: `python3 tools/gen_assets.py --verify`.
5. When asked to run the game for playtesting:
   `./build/open_world_zombie_waves` (launch in background, give controls).

## Verifying multiplayer

`--headless` cannot verify anything on the render path: `render()` returns
immediately when `game.headless` is set. Beacons, mirror interpolation, HUD and
pause overlays need a real on-device two-instance run (`--host` + `--join`).

For headless net verification, the host and client must both use
`--auto-start`/`--run-seconds`, and the useful signals are:

- host stdout: `Spawned remote player entity for slot N`, `WAVE n STARTED`
- client stdout: `CLIENT mirror seq=... ents=N`
- client `--events` log: `WAVE_START`, `KILL`, `POINTS`, `DAMAGE`, `ITEM_PICKUP`

Client-side input and AI are only exercised if the client runs `--ai=bot`; the
client builds its AI view from the snapshot mirror, not from a local ECS.

**The client's world comes from the host.** The map index travels in the HELLO
(`net_encode_hello`), and `reset_game()` loads *that* map when
`net_client.map_index_valid` is set. If you change the HELLO layout, bump
`NET_WIRE_VERSION` and `NET_WORLD_GEN_VERSION` together and update
`test_net_codec` — a stale peer would otherwise read the map byte as the first
character of the host name.

## Launching two local instances

GUI runs must be detached or the tool call hangs waiting on the window:

```bash
R=/home/divyam-redkar/development/personal/SDL3/open-world-zombie-waves-multiplayer
cd "$R/build"
IP=$(hostname -I | awk '{print $1}')
setsid ./open_world_zombie_waves --host --port=5123 --name=Host \
  >/tmp/opencode/coop_host.log 2>&1 </dev/null &
sleep 2
setsid ./open_world_zombie_waves --join=$IP:5123 --name=Guest \
  >/tmp/opencode/coop_join.log 2>&1 </dev/null &
```

**Do not** use `pkill -f open_world_zombie_waves` — the pattern matches the shell
running the command and kills the tool call. Use explicit PIDs, or
`pgrep -f ... | xargs -r kill`.

## Known unverified work

`docs/PENDING_VERIFICATION.md` lists what is **believed** working but not
proven, and what is deliberately open (R13-I5: the client renders dots rather
than sprites). Headless gates are structurally blind to visual defects — check
that file before claiming a visual fix.

## Docs housekeeping (keep these current)

- `docs/BUGS.md` — bug sheet; symptom/root cause/fix/status + commit ref.
- `docs/R13_BUGFIX_TRACKER.md` — the multiplayer fix series and its gate.
- `docs/PENDING_VERIFICATION.md` — unproven and deliberately-open items.
- `docs/CROSS_BRANCH_CONFLICTS.md` — the cross-branch merge analysis.
- `docs/SPRINT_PLAN.md` — current sprint: goals, scope, acceptance, owner tags.
- `docs/FUTURE_IDEAS.md` — backlog of ideas not yet started.
- `docs/FEATURES.md` — feature sheet for both engines.
- `docs/PROGRESS.md` — session log: what was found and done, with commit refs.
- `docs/EXTENDING.md` / `docs/ASSET_MAP_PLAN.md` — asset & API docs.

## Session start ritual

1. `git -C "$R" status --short && git -C "$R" log --oneline -10`
2. Read this file, `docs/CROSS_BRANCH_CONFLICTS.md`, `docs/PENDING_VERIFICATION.md`,
   and `docs/R13_BUGFIX_TRACKER.md`.
3. Set up todos (todowrite) reflecting PM plan + assigned DEV work.
