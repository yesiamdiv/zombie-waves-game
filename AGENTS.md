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

- **All worktrees live inside the main checkout, never beside it.** The layout is:

  | Path | Branch | Owner |
  |---|---|---|
  | `/home/divyam-redkar/development/personal/SDL3/open-world-zombie-waves` | `main` | — (integration + the owner's chats) |
  | `<main>/worktrees/multiplayer` | `feature/net-protocol` | agent `net` |
  | `<main>/worktrees/assets` | `feature/assets-maps` | agent `assets` |

  `worktrees/` is in `.git/info/exclude` and is never committed. A sibling tree at
  `/home/divyam-redkar/development/personal/SDL3/open-world-zombie-waves-*` is a
  **stale path** — those were moved with `git worktree move` on 2026-10-09 and no
  longer exist. If you are told to work in one, use the `worktrees/<name>` path instead.
- Prefer absolute paths plus `git -C "$R"`. Shell cwd and relative paths have
  drifted in this repo before.
- Work in your own worktree only: `git -C worktrees/<name> ...`, committing on that
  worktree's branch. Never edit or `git add -A` in another agent's worktree.
- The `main` worktree holds uncommitted WIP. Do not edit or commit there until that
  WIP is committed or the user says otherwise.
- Never `mv` a worktree — use `git worktree move`, and `git worktree repair` if one
  was moved by hand.

## Coordination (Intercom-AI)

This project is an Intercom-AI company, **Zombie Waves Studio**. Hired chats:
`net` (Netcode Developer, owns `worktrees/multiplayer`) and `assets`
(Assets Developer, owns `worktrees/assets`). There is no manager; the owner
coordinates directly.

Talk to the other agents through the `intercom_*` MCP tools rather than guessing
what they are doing — this replaces eyeballing the `agents/` files for
agent-to-agent questions. `docs/AGENT_RULES.md` §11 still describes the older
file-based channel; the intercom tools are now the primary path. Intercom's own
state lives in `.agents/` and `.opencode/`, both excluded from Git.

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

This branch adds multiplayer **without changing single-player**. One gate proves
it, and it is also the verification ritual — run all of it before declaring
anything done:

```sh
cmake --build build                  # zero warnings in project sources
ctest --test-dir build               # 3/3: net_spike, net_loopback, core_tests
./build/zombie_tests                 # green
python3 tools/gen_assets.py --verify  # when assets were touched
# two same-seed SP runs, diff-empty:
./build/open_world_zombie_waves --headless --ai=bot --run-seconds=25 --events=a.log --seed=42
```

Plus: multiplayer-only behaviour stays behind `player_count > 1`, and
single-player wave 1 is unchanged.

**The exact numbers a green gate produces** — test counts, texture count, the
byte/line reference for the SP events log, the wave-1 string — are in
`docs/knowledge/gate-baseline.md`, declared once. Do not copy them into another
doc; cite the topic. Moving the byte/line reference is a deliberate re-baseline,
not a silent drift.

If a change cannot pass this gate, it is not ready to commit — including when
the gate is inconvenient because the change is visual or audible and the gate is
headless. That is the normal condition of this work, not an exemption: a passing
gate proves nothing about what you see or hear, so write `unproven`, not
`fixed`, until a human has looked or listened (`docs/SYSTEM_DESIGN_RULES.md` §4,
`docs/knowledge/i5-unverified-on-screen.md`).

**Why the gate survived the assets merge:** headless runs deliberately keep the
fixed classic 50x50 world (`world_init`), never a map file, so map selection
cannot perturb the sim. Do not "clean that up" — it would break determinism.

When asked to run the game for playtesting, launch
`./build/open_world_zombie_waves` in the background and give the controls.

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
R=/home/divyam-redkar/development/personal/SDL3/open-world-zombie-waves/worktrees/multiplayer
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
- `docs/README.md` — index of every doc above.

## MANDATORY STANDARDS (added 2026-10-03 — read before implementing anything)

Two documents are binding on every agent working in this repo. They are short.
They are not optional. Each rule in them exists because this project has already
been broken that way once:

- **`docs/SYSTEM_DESIGN_RULES.md`** — how to design systems so they do not break
  each other, and above all multiplayer. The headline rule: **the host is the
  single source of truth; a client may only consume host state**, and may add
  computation only if it is cosmetic *and* deterministic across machines. Five
  separate bugs (B19–B23) came from clients inventing or self-supplying values
  the host owns. Also covers the single-player invariance gate, the
  headless/visibility blind spot, determinism discipline, asset rules, and the
  per-system checklist.
- **`docs/VERSIONING.md`** — the project's version-numbering standard. One
  number, declared once, guarding exactly one concern. Bump a version in the
  **same commit** as the change that invalidates it, update the codec test in
  that commit, and record why in the declaration's comment. Adding a system,
  wire field, asset, or map format means checking this file first.

If a change cannot be reconciled with both documents, stop and ask rather than
shipping it.

## Session start ritual

1. `git -C "$R" status --short && git -C "$R" log --oneline -10`
2. Read this file, `docs/CROSS_BRANCH_CONFLICTS.md`, `docs/PENDING_VERIFICATION.md`,
   and `docs/R13_BUGFIX_TRACKER.md`.
3. **Check your inbox and the project knowledge.** `intercom_whoami` tells you
   who you are and which worktree you own; `intercom_inbox` shows anything
   waiting. `intercom_knowledge` lists the decision topics in `docs/knowledge/`
   — open the ones touching your area before you plan, and record new decisions
   with `intercom_save_knowledge` rather than only in chat.
   The old `agents/` file channel and `tools/agent_channel.sh` are **retired**;
   see `docs/knowledge/worktree-layout.md`.
4. Set up todos (todowrite) reflecting PM plan + assigned DEV work.
