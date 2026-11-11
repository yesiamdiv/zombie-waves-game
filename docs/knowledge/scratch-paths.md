# Scratch paths: nothing writes to /tmp on this machine

<!-- Project knowledge (Intercom-AI). Agents append entries; edit freely. -->

## 2026-11-10 - standing rule (pm task task_01a181228a813f65de5be75f)

**Nothing writes scratch, logs, or build intermediates to `/tmp` on this
machine.** `/tmp` is a per-user tmpfs with a hard quota; when it fills, builds
fail mid-link with `fatal error: error closing /tmp/ccXXXX.s: Disk quota
exceeded`, and prior sessions have lost dep caches, test scratch and audio
captures that way (this is the third recurrence).

Where things go instead:

- Run logs and event logs: `<worktree>/build/logs/` (e.g.
  `build/logs/coop_host.log`, `build/logs/sp_a.log`). `mkdir -p` it first.
- Compiler temporaries when the default `/tmp` is full: point `TMPDIR` at a
  real-disk directory, e.g. `TMPDIR=$HOME/tmp_build cmake --build build`.
- Test scratch: the test helper already uses a repo-local temp path
  (`test_tmp_path`), not `/tmp`.
- Audio captures / probes: also under the worktree (or a real-disk dir), never
  `/tmp`.

Historical docs that mention `/tmp/opencode/...` (e.g. `docs/PROGRESS.md`,
`docs/knowledge/audio-measurements.md`) are records of past runs, not
instructions; do not follow those paths. Active instructions (`AGENTS.md`,
`docs/QA_MULTIPLAYER_S5.md`) have been updated.
