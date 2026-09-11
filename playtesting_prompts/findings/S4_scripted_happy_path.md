# S4 - Scripted happy path

- CMD:        `./build/open_world_zombie_waves --headless --ai=script /tmp/opencode/s4.script --run-seconds=30 --events=/tmp/opencode/s4.log --seed=7`
- SCRIPT:     `/tmp/opencode/s4.script`
  ```
  @0.0 aim 1600 1600
  @0.2 click 1 down
  @1.4 click 1 up
  @3.0 quit
  ```
- SEED:       7
- OUTPUTS:    event log `/tmp/opencode/s4.log`
- EXIT CODE:  0 (script `quit` exits cleanly at t=3.000, before the 30 s window)

## VERDICT
PASS — script parsing, time/offset scheduling, aim + fire, and `quit` all work.

## Findings

### [PASS] Script drives input and cleanly quits
- `Script loaded '/tmp/opencode/s4.script': 4 actions (line end 12)` in game.log.
- `aim 1600 1600` then `click 1 down/up` map to `INPUT` events with exactly 0.2 s
  scheduling granularity and real bullets:
  ```
  t=0.192 f=23   EVT=INPUT e=4294967295 k=- x=1600.0 y=1600.0 a=1.00 b=1.00  (down)
  t=0.192 f=23   EVT=PLAYER_SHOT e=1 k=bullet x=1452.7 y=1452.7 a=0.71 b=0.71
  t=1.392 f=167  EVT=INPUT e=4294967295 k=- x=1600.0 y=1600.0 a=1.00 b=0.00  (up)
  ```
  9 bullets fired in total (0.192..1.992) at 5/s as expected for the held button.
- `@3.0 quit` -> process exits 0; last event `t=3.000 f=360`.

### [Nit] Script-fire racers before the first wave: bullets fly at empty world
- EVIDENCE: bullets at `t=0.192..1.992` fire while wave 1 only starts `t=2.992`; the
  9 bullets hit nothing (`PLAYER_SHOT` have no matching `KILL x=1452.y`).
- IMPACT: cosmetic for scripts; for players it is fine (they choose when to shoot).
- SUGGESTED FIX: none required; optionally note in script docs that firing starts
  immediately.

### [Nit] `aim` accepts off-viewport targets without validation
- EVIDENCE: `aim 1600 1600` on a 1280x720 logical viewport is accepted and yields
  `x=1600.0 y=1600.0`; shots travel to (1452.7, 1452.7) and keep going off-map.
- SUGGESTED FIX: clamp/validate on load (warn) — no functional breakage seen.

## Metrics
- Bullets fired: 9; zombie kills: 0 (fired before wave 1; script quits at t=3.0).
- Wave 1 start: t=2.992 (spawned but never engaged before quit).
- Errors: 0.