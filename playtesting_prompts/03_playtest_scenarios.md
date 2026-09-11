# 03 - Playtest Scenarios

Run scenarios in order. Put a fresh binary `build/open_world_zombie_waves` first
(done in `02`). Always capture full logs to stable paths:

```bash
cd /home/divyam-redkar/development/personal/SDL3/open-world-zombie-waves
BIN=./build/open_world_zombie_waves
```

After every run, check: exit code == 0, no "FATAL"/"ERROR"/crash in the app log,
and confirm the expected events appear in the event log.

## S1 - Baseline bot run

```bash
$BIN --headless --ai=bot --run-seconds=60 --events=/tmp/opencode/s1_bot.log --seed=42
```

Verify ALL of:
- `WAVE_START` at ~t=3.0 (3s first cooldown), `a=1 b=8.00` (wave 1, 8 zombies).
- Zombie `ENTITY_SPAWN` positions are 400-600 from the player's `POSITION_SAMPLE`.
- `PLAYER_SHOT`/`DAMAGE`/`KILL` all occur; every spawned zombie gets a `KILL`.
- `WAVE_END a=1` then `WAVE_START a=2` (correct progression).
- Player alive at t=60 (final `PLAYER_HEALTH` > 0).

## S2 - Survival stress

```bash
$BIN --headless --ai=bot --run-seconds=600 --events=/tmp/opencode/s2_survival.log --seed=42
```

- Wave count reached, where the bot died (`PLAYER_HEALTH` → 0, `KILL k=player`),
  whether deaths are fair vs an unavoidable difficulty spiral (check per-wave
  clear times and `GE_DAMAGE` burst patterns).
- Note if any wave stalls (no `WAVE_END` even though all zombies died).

## S3 - Determinism (byte-identical)

```bash
$BIN --headless --ai=bot --run-seconds=60 --events=/tmp/opencode/s3_a.log --seed=42
$BIN --headless --ai=bot --run-seconds=60 --events=/tmp/opencode/s3_b.log --seed=42
diff /tmp/opencode/s3_a.log /tmp/opencode/s3_b.log
```

`diff` must be EMPTY. Any difference is a Critical finding (non-deterministic
sim). Also diff with a different seed (`--seed=7`) - difference is expected.

## S4 - Scripted mode happy path

Create `/tmp/opencode/s4.script`:

```
@0.0 aim 1600 1600
@0.2 click 1 down
@2.0 click 1 up
@3.0 quit
```

```bash
$BIN --headless --ai=script /tmp/opencode/s4.script --run-seconds=10 --events=/tmp/opencode/s4.log --seed=7
```

Verify: script loads, `PLAYER_SHOT` occurs while clicking, `INPUT ia=1` lines
mark injected input, and the app exits code 0 after `t=3.0` (not 10).

## S5 - Script failure modes

- Missing script: `--ai=script /tmp/opencode/no_such.script` → must warn and
  NOT crash (should fall back, then idle; still exit 0 under --run-seconds).
- Malformed lines: key with typo, unknown command, bad numbers → parser must
  skip them with warnings and continue, not crash.
- `--script-loop=3` with a `/tmp/opencode/s4.script`-style run: verify looping
  replays the actions and exits cleanly.
- `--run-seconds=0`: runs indefinitely without auto-exit (use Ctrl-C / timeout
  only if needed; timeout 10s and treat clean Ctrl-C as OK).

## S6 - Flag edge cases

```bash
$BIN --headless --ai=none --run-seconds=5 --events=/tmp/opencode/s6_none.log --seed=0   # idle on menu, no crash
$BIN --headless --ai=bot --run-seconds=0.1 --events=/tmp/opencode/s6_tiny.log --seed=1   # tiny run
$BIN --headless --ai=bot --seed=12 --events=/tmp/opencode/s6_nodef.log                   # run to game over (no --run-seconds)
```

- `--ai=none` headless: must idle in menu forever without crashing.
- `--run-seconds=0.1`: clean exit, 0 or a handful of events, no negative times.
- Bare bot run: run until natural game over (or a hard cap you impose, e.g.
  `timeout 120`), report the outcome.

## S7 - Memory sanity (long session)

```bash
$BIN --headless --ai=bot --run-seconds=300 --events=/tmp/opencode/s7_mem.log --seed=42 &
PID=$!; for i in 1 2 3; do sleep 8; grep VmRSS /proc/$PID/status; done; wait $PID
```

- RSS must be stable (no unbounded growth) across 300s.
- Also confirm the event log file size stays reasonable (< ~5MB for 300s).

## S8 - Optional: windowed human sanity (skip if no display)

```bash
$BIN --run-seconds=20 --events=/tmp/opencode/s8_window.log
```

Only if `$DISPLAY`/Wayland is available. Verify it opens, runs, and exits
cleanly. Prefer skipping over hanging on a headless box.