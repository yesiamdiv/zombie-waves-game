# S2 - Survival stress (600s bot run)

- CMD:        `./build/open_world_zombie_waves --headless --ai=bot --run-seconds=600 --events=/tmp/opencode/s2_survival.log --seed=42`
- SEED:       42
- OUTPUTS:    event log `/tmp/opencode/s2_survival.log` (67028 lines); app log `game.log`
- EXIT CODE:  0 (sim ran full 600 s, t=600.000 f=72000)

## VERDICT
FAIL — run completes but the game is effectively dead from ~t=45.5: 554.5 s of the
600 s window produce nothing but item spawns and position samples. The wave
soft-lock (S1-Major) reoccurs identically on the same seed.

## Findings

### [Major] Wave-2 soft-lock stalls 554.5 s of the 600 s survival run
- REPRO:
  ```
  ./build/open_world_zombie_waves --headless --ai=bot --run-seconds=600 --events=/tmp/opencode/s2_survival.log --seed=42
  ```
- EVIDENCE: only 2 `WAVE_START` and 1 `WAVE_END` ever fire; the last zombie kill is at
  `t=45.467` and nothing after it is gameplay:
  ```
  t=23.383 f=2806 EVT=WAVE_START e=4294967295 k=- a=2.00 b=11.00
  t=45.467 f=5456 EVT=KILL e=1 k=zombie x=1478.3 y=778.7        <- last kill ever
  t=74.867 f=8984 EVT=WAVE_START e=4294967295 k=- a=2.00 b=3.00  <- never happens
  ...no WAVE_END a=2; tail of log is only ENTITY_SPAWN k=item + POSITION_SAMPLE
  t=585.042 ... ENTITY_SPAWN e=40 k=item x=2135.0 y=1069.0
  t=600.000 f=72000 EVT=POSITION_SAMPLE e=0 k=player ...
  ```
  The 19th zombie (wave-2 index 16) spawns frozen at `t=39.658 e=4 (884.4,1368.3)`
  (the same zombie as S1, seed-42 deterministic) and never moves nor is damaged;
  item spawns keep firing every 15 s so the app never looks crashed, but the wave
  cannot end. Entity table grows past `e=40` (no item pickup/despawn), `total alive`
  in game.log climbs with no cleanup during the stall.
- ROOT CAUSE: see S1 `src/world/waves.c:118` (zombie detection 400 < spawn ring max
  600) + `src/systems/zombie_ai.c:46-52` (idle + 0.95 velocity damping when out of
  range) + bot engage cap `src/ai/bot.c:29`.
- IMPACT: Survival mode is unplayable past wave 2. The only "content" after t≈45.5 is
  unlimited medkit/speed-boost spawns accumulating at the map edges.
- SUGGESTED FIX: same as S1 (spawn-to-chase, detection >= 600, bot seek, wave timeout).

## Metrics
- Waves cleared: 1 complete (wave 2 stalls at 10/11).
- Wave 1: t=2.992 -> t=20.383, clear 17.39 s, 8/8 kills.
- Wave 2: t=23.383 -> (never), 10/11 kills, stalls.
- Total zombie kills: 18; player deaths: 0 (no `KILL k=player`, no `DAMAGE` on player).
- Dead air after last kill: t=45.467 -> t=600.000 = 554.53 s.
- Item spawns during dead air: continue every 15 s; entity ids `e=1..40+` accumulate.
- Errors: 0 `ERROR`/`FATAL` in event log or game.log.