# QA-Recheck 02 - Exact Scenarios and Pass Criteria

Run in order R1..R11. Base setup:

```bash
cd /home/divyam-redkar/development/personal/SDL3/open-world-zombie-waves
BIN=./build/open_world_zombie_waves
LOG=/tmp/opencode            # all logs here
```

For EVERY run capture BOTH outputs:
```bash
$BIN ... --events=$LOG/<name>.log > $LOG/<name>.app 2>&1; echo "exit=$?"
```
Must always confirm: `exit=0`, no `FATAL|ERROR|Segmentation|abort` in the `.app`
log, and expected events present in the event log.

## Verified source constants you will compare against

- Wave N spawns `5 + N*3` zombies (wave 1 = 8), one per `max(0.3, 2.0 - N*0.1)`s,
  3s cooldown between waves. First `WAVE_START` at ~t=3.
- Zombies spawn in a walkable annulus **400-600px** around the player; zombie
  `detection_range` = **700** (so ring-edge spawns must chase immediately).
- Zombie HP `100*difficulty`, speed `80*difficulty`; difficulty `1+(N-1)*0.15`.
  Player: 200 HP, speed 200. Bullet 25 dmg (4 hits = wave-1 kill). Zombie melee 10.
- Bot: detection 600, standoff 260, flee 70, fire ~8.3 shots/s; always hunts the
  nearest zombie; seeks items when HP < 75% and an item is within 500.
- Wave force-end safety net: `WAVE_MAX_DURATION = 60s`.
- Items: 15s world rain + ~40% kill-drop at each zombie death, capped at
  `MAX_ALIVE_ITEMS = 8` live entities. Medkit heals 30 (`ITEM_PICKUP a=0 b=30`).
- Event log: every line has a strictly increasing `sid=` serial (entity `e=` is
  recycled). Header mentions `sid`.

aka quick reference for check commands: `grep -c`, `rg '^'`, `awk`.

---

## R1 - Baseline bot run (Regression: B1 soft-lock, seed 42)

```bash
$BIN --headless --ai=bot --run-seconds=60 --events=$LOG/r1_s42.log --seed=42 > $LOG/r1_s42.app 2>&1; echo "exit=$?"
```

PASS if ALL of:
- exit=0.
- `WAVE_START a=1 b=8.00` at ~t=3.0.
- Every wave-1 zombie spawn gets a `KILL` (count `ENTITY_SPAWN k=zombie` pre-`WAVE_END a=1` == count `KILL k=zombie` in that window == 8).
- `WAVE_END a=1` fires (this was impossible before the fix), then `WAVE_START a=2`.
- NO log line matching `timed out` or `force-ending` anywhere (the 60s net did NOT trigger).
- Player alive at t=60 (`PLAYER_HEALTH` present and > 0, or simply no `KILL k=player`).

## R2 - Baseline bot run, the other freeze seed (seed 12)

```bash
$BIN --headless --ai=bot --run-seconds=60 --events=$LOG/r2_s12.log --seed=12 > $LOG/r2_s12.app 2>&1; echo "exit=$?"
```

PASS if identical shape to R1: wave 1 completes (8/8 killed), `WAVE_END a=1`,
`WAVE_START a=2`, no timeout WARN, player alive. (Before the fix this seed froze
inside wave 1.)

## R3 - Determinism, byte-identical (unchanged requirement)

```bash
$BIN --headless --ai=bot --run-seconds=60 --events=$LOG/r3_a.log --seed=7 >/dev/null 2>&1
$BIN --headless --ai=bot --run-seconds=60 --events=$LOG/r3_b.log --seed=7 >/dev/null 2>&1
cmp $LOG/r3_a.log $LOG/r3_b.log && echo BYTE-IDENTICAL
```

PASS: prints `BYTE-IDENTICAL`. Any diff = Critical (non-deterministic sim).

## R4 - Script CLI order-independence (Regression: B2)

Write `/tmp/opencode/r4.script`:
```
@0.2 aim 1600 1600
@0.4 click 1 down
@1.5 click 1 up
@2.5 quit
```

Run BOTH of these - they must behave identically:
```bash
$BIN --headless --ai=script --script-loop=2 /tmp/opencode/r4.script --run-seconds=15 --seed=7 --events=$LOG/r4_order1.log > $LOG/r4_order1.app 2>&1; echo "exit=$?"
$BIN --headless --script /tmp/opencode/r4.script --script-loop=2 --ai=script --run-seconds=15 --seed=7 --events=$LOG/r4_order2.log > $LOG/r4_order2.app 2>&1; echo "exit=$?"
```

PASS if BOTH: exit=0; the `.app` log shows `Script loaded ... 4 actions`;
`PLAYER_SHOT` events occur; `INPUT ia=1` marks injected input; the app exits after
the script's `quit` at t=2.5 (runs ~2.5-3s, NOT 15s). Non-zero exit, "requires a
script path" warning, or a run that ignores the script = FAIL (B2 regression).

Extra (same run): missing-script fallback must NOT crash
```bash
$BIN --headless --ai=script /tmp/opencode/does_not_exist.script --run-seconds=5 --seed=1 --events=$LOG/r4_missing.log > $LOG/r4_missing.app 2>&1; echo "exit=$?"
```
PASS: exit=0, a WARN about the script, runs (idles) 5s, no crash.

## R5 - SIGINT / SIGTERM clean shutdown (Regression: B3)

```bash
$BIN --headless --ai=bot --seed=1 --events=$LOG/r5_sigint.log > $LOG/r5_sigint.app 2>&1 &
PID=$!
sleep 3
kill -INT $PID
sleep 1
if kill -0 $PID 2>/dev/null; then echo "STILL_ALIVE_FAIL"; kill -9 $PID; else echo "exit_clean"; fi
pgrep -x open_world_zombie_waves   # must print nothing
```
Repeat once with `kill -TERM $PID`.

PASS: process is gone within ~1s of the signal; `.app` log ends with
`Interrupt received; shutting down cleanly`; `pgrep` shows no orphan afterward.
A process that ignores the signal until SIGKILL = FAIL (B3 regression).

## R6 - `sid=` serial identity (Regression: B4)

Use the R1 log (`$LOG/r1_s42.log`). Verify EVERY line carries a `sid=` and the
serial is strictly increasing (no wraps/duplicates for the whole run):
```bash
rg -c '^t=' $LOG/r1_s42.log                                   # total event lines
rg -c 'sid=' $LOG/r1_s42.log                                  # must equal the total
rg -o 'sid=[0-9]+' $LOG/r1_s42.log | sort -t= -k2 -n | head -1   # first line all-zero? == sid=0
rg -o 'sid=[0-9]+' $LOG/r1_s42.log | sed 's/sid=//' | awk 'NR>1 && $1<=prev{print "NON_MONOTONIC at", NR} {prev=$1}'
```
PASS: every line has `sid=`, and the monotonic check prints nothing. Also confirm
the file header line contains `sid=<serial>`.

## R7 - Heal economy reachable (Regression: B5)

```bash
$BIN --headless --ai=bot --run-seconds=60 --seed=42 --player-hp=40 --player-damage-mult=2 --events=$LOG/r7_heal.log > $LOG/r7_heal.app 2>&1; echo "exit=$?"
```

PASS if ALL of:
- exit=0 and no GAME OVER (bot survives 60s from 80/200 HP under 2x melee damage).
- `ITEM_PICKUP` count > 0 (was 0 in every pre-fix run).
- At least one `ITEM_PICKUP a=0 b=30` (a health medkit actually collected).
- Player was damaged at least once (`PLAYER_HEALTH` events > 0) - otherwise the
  heal was never needed and the scenario was invalid (report it).

Also confirm drop-on-kill exists (items land near the action):
`rg 'EVT=ENTITY_SPAWN' $LOG/r7_heal.log | rg 'k=item'` - each item pos should be
within ~50px of a recent `KILL k=zombie` position (`rg 'EVT=KILL' $LOG/r7_heal.log`).

## R8 - Death / game-over path reachable (new debug knobs)

```bash
timeout -k 3 150 $BIN --headless --ai=bot --seed=42 --zombie-speed-mult=10 --player-damage-mult=5 --events=$LOG/r8_die.log > $LOG/r8_die.app 2>&1; echo "exit=$?"
```

PASS if ALL of:
- `GAME OVER` appears in the `.app` log (score/wave/kills line).
- `KILL e=... k=player` appears in the event log.
- `PLAYER_HEALTH` events show HP decreasing to 0 before that.
- The run reached GAME OVER BEFORE the 150s timeout expired.
(Expected: ~15-90s to death; the reference run died in wave 4, t=83.9, 36 kills.)
Note: `exit` may be 124 (timeout) - the game idles on the game-over screen and does
not self-exit; that is EXPECTED, judge by the GAME OVER line, not the exit code.

CONTROL (documents known behavior - NOT a defect): default settings
```bash
$BIN --headless --ai=bot --run-seconds=60 --seed=42 --events=$LOG/r8_control.log >/dev/null 2>&1
rg -c 'PLAYER_HEALTH' $LOG/r8_control.log    # expect 0 melee hits on the elite bot
```
If `PLAYER_HEALTH` == 0 at default settings that is the KNOWN elite-bot trait
(no natural death). Do NOT file it as a bug; it is why the debug knobs exist.

## R9 - Wave-timeout safety net present but inactive in normal play

- PASS if the R1/R2 logs contain NO `timed out`/`force-ending` WARN in any normal
  run (net is a backstop, never trips on healthy waves).
- The net's code path is already covered by the unit test `test_wave_timeout`
  (ran green in the build gate). If you want extra evidence, grep the unit run
  output for `Test: Wave timeout force-end` passing.

## R10 - Item-cap holds (Regression: B6)

From ANY run of >=100s (use `$LOG/r7_heal.log`, which also dropped items):
```bash
python3 - <<'EOF'
import re
alive=peak=0
# items never die except via pickup: LIVE = item-ENTITY_SPAWNs minus ITEM_PICKUPs
sp_re = re.compile(r'EVT=ENTITY_SPAWN.*k=item')
pk_re = re.compile(r'EVT=ITEM_PICKUP')
for line in open('/tmp/opencode/r7_heal.log'):
    if sp_re.search(line): alive+=1; peak=max(peak,alive)
    elif pk_re.search(line): alive-=1; alive=max(0,alive)
print("peak live items:", peak)
EOF
```
PASS: printed peak <= 8 (`MAX_ALIVE_ITEMS`). It is expected to sit at or near 8 in
kill-heavy / no-pickup log slices - that is the cap doing its job.

## R11 (optional if time) - Memory sanity

```bash
$BIN --headless --ai=bot --run-seconds=300 --events=$LOG/r11_mem.log --seed=42 > $LOG/r11_mem.app 2>&1 &
PID=$!; for i in 1 2 3; do sleep 8; grep VmRSS /proc/$PID/status; done; wait $PID
```
PASS: VmRSS stable across samples (no runaway growth); event log `< ~5MB`.

---

## After all runs

1. Collate evidence, write `findings/R<n>_*.md` per scenario + `findings/SUMMARY.md`
   (template in `qa_recheck/03_report_format.md`).
2. Priority: any R1/R2 soft-lock, R3 nondeterminism, R4 script not loading, R5
   ignored signals, R7 heal never triggering, R8 game-over unreachable, R10 cap
   exceeded are all fail-the-blocker outcomes.
3. End your last chat message with: overall VERDICT, top findings + exact replay
   commands, and the metrics table (waves cleared, first-wave time, survival time,
   DPS, TTK, pickups).