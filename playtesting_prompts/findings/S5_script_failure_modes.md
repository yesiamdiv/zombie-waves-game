# S5 - Script failure modes

Test matrix (all headless, `--seed=1`):
1.  Missing script path
2.  Malformed script content
3.  `--script-loop=N` placed before the script path
4.  `--script-loop=3` after the path (correct order)
5.  `--run-seconds=0` (infinite)
6.  SIGINT/SIGTERM handling

## VERDICT
FAIL-WITH-CAVEATS — graceful handling for missing/malformed scripts is good and loop
semantics match docs, but CLI option-order is fragile (silently drops the script) and
the headless runner ignores SIGINT/SIGTERM.

## Findings

### [Major] `--script-loop=N` before the script path silently disables the script
- REPRO:
  ```
  ./build/open_world_zombie_waves --headless --ai=script --script-loop=3 /tmp/opencode/s5_loop.script --run-seconds=7 --events=/tmp/opencode/s5_order.log --seed=1
  ```
- EVIDENCE (console; exit code still 0):
  ```
  [WARN]  main.c:391: --ai=script requires a script path
  [WARN]  main.c:424: Unknown argument: /tmp/opencode/s5_loop.script
  ```
  The script is dropped and the game runs the whole 7 s with **no AI at all**
  (`ai_driver ... mode: none`), not the script.
- ROOT CAUSE: `main.c` parses the bare positional script path immediately after
  `--ai=script` as a single payload; any option token (`--script-loop=`) is taken as
  the path, and then the real path falls through to the unknown-argument stop.
- IMPACT: automation that reorders flags loses the "player" silently (the run looks
  normal, just idle). Should error loudly or reorder-tolerant.
- SUGGESTED FIX: options should be order-independent (`--script + --script-loop` both
  grabbed in one pass); fail hard (non-zero exit) when `--ai=script` has no script.

### [Minor] Headless process ignores SIGINT and SIGTERM; only SIGKILL stops it
- REPRO:
  ```
  ./build/open_world_zombie_waves --headless --ai=script /tmp/opencode/s5_loop.script --run-seconds=0 --events=/tmp/opencode/s5_zerorun.log --seed=1 &
  kill -INT $!    # process keeps running; log stops only on SIGKILL
  ```
- EVIDENCE: after `kill -INT` no shutdown message appears (game.log silent); `timeout`
  without `-k` cannot stop the process (long run only ended via SIGKILL, exit 137).
- ROOT CAUSE: no signal handler installed in headless path; loop only exits on
  run-window end / window close / quit script action.
- IMPACT: interrupted CI runs leave orphaned processes; `--run-seconds=0` is
  unstoppable without SIGKILL. User-facing controls are unaffected.
- SUGGESTED FIX: handle SIGINT/SIGTERM with a clean shutdown flag.

### [PASS] Missing script file: clean error + fallback to no AI
- REPRO: `--ai=script /tmp/opencode/nope.script --run-seconds=5`
- EVIDENCE (exit 0):
  ```
  [ERROR] script.c:21: Script: cannot open '/tmp/opencode/nope.script'
  [WARN]  main.c:387: Could not load script '<path>'; falling back to no AI
  ```
  Game runs the full window (idle), no crash.

### [PASS] Malformed script: per-line warnings, valid lines still run
- REPRO: `--ai=script /tmp/opencode/s5_bad.script --run-seconds=5` (bad lines: unknown
  commands `nope`/`keey`/`xyzzy`, `aim` missing args x2, blank line)
- EVIDENCE (exit 0): warnings with file:line for every bad line, then
  ```
  [INFO]  script.c:94: Scriprt loaded '<path>': 6 actions (line end 12)
  ```
- IMPACT: good. Open question: unknown *timing* formats are skipped (warn + continue),
  which matches doc intent ("warn unknown commands"); no priority issue.

### [PASS] `--script-loop=3` runs the script once + 3 repeats (doc-consistent)
- REPRO: `--ai=script /tmp/opencode/s5_loop.script --script-loop=3 --run-seconds=7`
  (script: `@0.0 aim 1600 1600`, `@0.2 click 1 down`, `@1.4 click 1 up`)
- EVIDENCE: 4 down/up bursts as documented ("repeat N times" = 1 + 3):
  ```
  button1 down t=0.192  t=1.592  t=2.992  t=4.392
  button1 up   t=1.392  t=2.792  t=4.192  t=5.592
  ```
  Note: loop restart happens even mid-hold (down at 1.592 comes 0.2 s after the up at
  1.392 — clean), and iteration does not re-run the `aim` line (aim once at 0.0).
- IMPACT: docs accurate; no action.

## Metrics (6 sub-scenarios)
- exit codes: 5x 0, 1x forced 137 (SIGKILL).