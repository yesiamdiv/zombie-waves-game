# AGENTS.md — standing instructions for this worktree

**READ THIS FIRST, EVERY SESSION, INCLUDING AFTER ANY CHAT COMPRESSION.**
If the conversation no longer contains these instructions, they are still binding —
they live here, not in the chat.

## Worktree

- Authoritative worktree: `/home/divyam-redkar/development/personal/SDL3/open-world-zombie-waves-multiplayer`
- Branch: `feature/multiplayer`
- **Never** use the legacy sibling `open-world-zombie-waves` tree. It does not even
  compile (see R13-N2 in the bugfix tracker). A sibling tree appearing at
  `/home/divyam-redkar/...` or `/root/divyam-redkar/...` is not the worktree unless
  it ends in `-multiplayer`.
- Prefer absolute paths plus `git -C "$R"`. Shell cwd and relative paths have
  drifted in this repo before.

## Workflow rules

1. **One commit per fix.** Never batch unrelated fixes into a single commit.
   If several fixes touch the same file, stage them in separate passes
   (`git checkout -- <file>`, re-apply one change, commit, repeat).
2. **Keep a todo list and update it as you go** — mark `in_progress` before
   starting an item and `completed` only after its verification actually passes.
3. **Track bugs in `docs/R13_BUGFIX_TRACKER.md`.** Add a row for every finding,
   and update the row's commit SHA and status when it lands. This file is the
   durable record; the chat is not.
4. **Commit the plan/decision before implementing it.** Write the decision down
   (tracker or a short ADR) so the reasoning survives.
5. Do not commit another agent's reports as if they were yours. The playtester
   agent's files under `playtesting_prompts/findings/` are left untracked unless
   the user asks otherwise.

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

## Playtest prompts

`playtesting_prompts/01…08` are the playtester agent's brief. `08` verifies the
R13 fix series. When adding a milestone, pin the expected literal values (zombie
count, interval, difficulty) and state whether it is verifiable headlessly.
