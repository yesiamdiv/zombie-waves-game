# Tmp scratch rule

<!-- Project knowledge (Intercom-AI). Agents append entries; edit freely. -->

## 2026-10-28 06:14 UTC - decision by pm: /tmp is off-limits for scratch; use build/logs/ or ~/.cache/ with bounded captures

Nothing writes scratch files to /tmp on this machine. /tmp is RAM-backed tmpfs with quota; unbounded captures (like pw-record) can exhaust quota and cause system-wide issues. All logs, captures, and scratch must go to build/logs/ or ~/.cache/. The "Launching two local instances" pattern must use build/logs/. Third incident: dep cache moved (b369a05), test scratch moved (f260c55), audio capture caused outage. Kill processes by explicit PID only - pkill -f matches the calling shell. After any audio capture or game launch, verify no leftover processes and /tmp usage.

**Why:** Third /tmp incident caused outage (audio capture wrote 8.4G to /tmp, deleted-but-held-open)

## 2026-11-10 - note by assets: build-temp half of the rule + active docs fixed

Follow-up to the task "Move all /tmp/opencode logs to build/logs/": the
*compiler's* temp files also count. When `/tmp` is full, the link step dies with
`fatal error: error closing /tmp/ccXXXX.s: Disk quota exceeded`; point `TMPDIR`
at real disk for that build, e.g.:

```sh
mkdir -p "$HOME/tmp_build"
TMPDIR="$HOME/tmp_build" cmake --build build
```

`AGENTS.md` ("Launching two local instances") and `docs/QA_MULTIPLAYER_S5.md`
now write `build/logs/coop_*.log` (absolute `$R/build/logs/` in the snippet,
because the shell is already in `$R/build`). Existing `/tmp/opencode/*.log`
files in this checkout are another agent's live scratch and were left alone;
the rule is about not creating new ones. Historical mentions in
`docs/PROGRESS.md` and `docs/knowledge/audio-measurements.md` are records of
past runs, not instructions.
