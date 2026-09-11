# S6 - Edge cases (menu-idle, tiny run, run-to-game-over)

Scenario set:
1.  `--ai=none --run-seconds=5` (idle on menu)
2.  `--ai=bot --run-seconds=0.1` (tiny positive run)
3.  `--ai=bot --run-seconds=0.1` absent `--run-seconds` (bare run to game over)

## VERDICT
PASS for 6a/6b; **FAIL for 6c** — the bare run never reaches game over because the
soft-lock reproduces again, this time in wave 1 and on a new seed.

## Findings

### [Major] Soft-lock reproduces on a NEW seed (12) and inside wave 1 — game-over path unreachable
- REPRO (capped externally):
  ```
  timeout -k 5 120 ./build/open_world_zombie_waves --headless --ai=bot --seed=12 --events=/tmp/opencode/s6_nodef.log
  ```
- EVIDENCE: wave 1 spawns 8 (`t=4.900..18.258`), 7 killed (last `t=26.508`); the 4th
  spawn freezes and never dies, so `WAVE_END a=1` never fires; no `KILL k=player` ever
  happens either:
  ```
  t=14.442 f=1733 EVT=ENTITY_SPAWN e=4 k=zombie x=1017.0 y=314.6   <- frozen
  t=26.508 f=3181 EVT=KILL e=6 k=zombie x=1153.9 y=1088.0          <- last kill ever
  t=14.600 ... t=125.000 EVT=POSITION_SAMPLE e=4 k=zombie x=1017.0 y=314.6  (0.2 s cadence, unchanged)
  ```
  After t=26.5 the run idles 98+ s (only `ENTITY_SPAWN k=item` every 15 s), the bot
  wanders in circles, the process was SIGKILLed by timeout (exit 137).
- ROOT CAUSE: same as S1-Major — the freeze is not seed-42-specific. Different seed =>
  a *different* zombie entity freezes, confirming the 400 vs 400-600 range mismatch is
  the trigger, not a bad seed.
- IMPACT: game-over is effectively unreachable for the bot (wave N's last zombie keeps
  freezing), and the "run to game over" automation path has no bounded behavior.
- SUGGESTED FIX: see S1 (plus this report's evidence that the fix must handle ANY
  spawn distance in 400-600, not a single seed).

### [PASS] `--ai=none` idles on the menu without crashing
- REPRO: `--ai=none --run-seconds=5 --seed=0 --events=/tmp/opencode/s6_none.log`
- EVIDENCE: exit 0; event log contains only the header (no events because the game
  never auto-starts with no AI). Correct.

### [PASS] Tiny run (`--run-seconds=0.1`) aware & clean
- REPRO: `--ai=bot --run-seconds=0.1 --seed=42 --events=/tmp/opencode/s6_tiny.log`
- EVIDENCE: exit 0, no negative timestamps, no `ERROR`. Empty event log is expected:
  first `POSITION_SAMPLE` is throttled to 0.2 s, first wave starts at 3 s.

## Metrics
- 6c: 8 spawns vs 7 kills preserved; frozen zombie distance to player at spawn: 553 px
  (out of its own 400 detect); dead air 98+ s; no game over.