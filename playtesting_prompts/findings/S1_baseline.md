# S1 - Baseline bot run

- CMD:        `./build/open_world_zombie_waves --headless --ai=bot --run-seconds=60 --events=/tmp/opencode/s1_bot.log --seed=42`
- SEED:       42
- OUTPUTS:    event log `/tmp/opencode/s1_bot.log`; app log `game.log`
- EXIT CODE:  0

## VERDICT
PASS-WITH-CAVEATS

## Findings

### [Major] Wave can soft-lock: zombie spawns asleep and never moves, wave 2 never ends
- REPRO:
  ```
  ./build/open_world_zombie_waves --headless --ai=bot --run-seconds=60 --events=/tmp/opencode/s1_bot.log --seed=42
  ```
- EVIDENCE: wave 2's 11th zombie spawns frozen and is untouched for 20 s until the run
  window ends; the player (bot) position and the zombie position both stop changing:
  ```
  t=39.658 f=4759 EVT=ENTITY_SPAWN e=4 k=zombie x=884.4 y=1368.3
  t=39.800 f=4776 EVT=POSITION_SAMPLE e=4 k=zombie x=884.4 y=1368.3
  ... (identical x/y every 0.2s to run end)
  t=60.000 f=7200 EVT=POSITION_SAMPLE e=4 k=zombie x=884.4 y=1368.3
  ```
  No `DAMAGE`/`KILL` ever references this zombie; only 18 of 19 zombies get a `KILL`
  (wave 2 kills stop at `t=45.467` killing `e=8`). `WAVE_END a=2` never appears.
- ROOT CAUSE: `src/world/waves.c:118` sets zombie `detection_range = 400.0`, but the
  spawn ring is 400-600 px around the player. `src/systems/zombie_ai.c:46-52` idles a
  CHASE-state zombie when `dist >= detection_range`, damping velocity by 0.95
  (starting at 0). The frozen zombie spawned 537 px from the player and the bot
  (`src/ai/bot.c:29`) only engages the nearest zombie within its own 600 px detect
  range, so when the player settles >400 px from the sole surviving zombie, neither
  entity acts: the wave cannot end.
- IMPACT: Any run where the last zombie(s) of a wave end up out of both ranges leads to
  an infinite stall. Confirmed in S2: 555 s of the 600 s run produce nothing but item
  spawns/position samples after the stall starts (~t=45.5).
- SUGGESTED FIX: Make spawned zombies aware immediately (spawn in CHASE and give a
  starting velocity toward the player), enlarge zombie detection to >= spawn ring max
  (>=600), and/or make the bot patrol/seek the nearest living zombie beyond detect
  range. Also add a wave timeout that force-ends a wave.

### [Minor] Zombie entity IDs (`e=`) are re-used, corrupting per-entity log analysis
- REPRO: same command as above.
- EVIDENCE: `e=1` is re-used 9x across spawns (t=4.900, 6.808, 8.717, 10.625, 16.350,
  25.192, 32.425, 41.467), and `e=4`/`e=6`/`e=8` similarly:
  ```
  t=4.900 f=588 EVT=ENTITY_SPAWN e=1 k=zombie x=900.1 y=1430.6
  t=6.808 f=817 EVT=ENTITY_SPAWN e=1 k=zombie x=1407.7 y=1913.2
  ...
  t=41.467 f=4976 EVT=ENTITY_SPAWN e=1 k=zombie x=765.5 y=1976.1
  ```
- ROOT CAUSE: ECS frees a dead entity's slot immediately and the next spawn reuses it
  (`src/ecs/ecs.c`). The event log then cannot uniquely identify a zombie.
- IMPACT: The event format's `e=` cannot be used to pair spawn-damage-kill per entity;
  swarm-pressure and TTK metrics in the analysis playbook are unreliable.
- SUGGESTED FIX: Use monotonically increasing ids for the log (log a separate
  `sid=`/serial) instead of recycling ECS slots.

### [Minor] No healing is ever picked up; `ITEM_SPEED_BOOST`/medkit economy unverifiable in bot runs
- REPRO: same command; grep the log:
  ```
  grep -c "EVT=ITEM_PICKUP" /tmp/opencode/s1_bot.log   -> 0
  ```
- EVIDENCE: medkits spawn every 15 s (t=15.000 at 2449,1668; t=30.008; t=45.017), a
  Speed Boost at `t=15.000 ... x=2449.0 y=1668.0` (a=2, b=5), yet `ITEM_PICKUP` count is
  0. `PLAYER_HEALTH` count is also 0 (player never hurt in this run).
- ROOT CAUSE: bot only fights; it never collects items, and pickups require standing on
  the item tile.
- IMPACT: The heal economy and the known speed-stack smell (`ITEM_SPEED_BOOST` adds +5
  permanently, unlimited) are not covered by any bot scenario.
- SUGGESTED FIX: Bot should opportunistically path to in-detect-range items (especially
  medkits when HP < 100); cap speed-boost stacking or make it temporary.

## Metrics
- Waves cleared this run: 1 complete (wave 2 in progress, stalls).
- Wave 1: start t=2.992, end t=20.383, clear time 17.39 s, 8/8 kills.
- Player survival: alive at t=60 (last `POSITION_SAMPLE e=0 k=player` t=60.000).
- Bot DPS in active combat window (16.658-45.467): 95 shots * 25 dmg / 28.81 s = 82.4 DPS.
- TTK: sequential spawn->kill pacing ~1.6-2.2 s per spawn-kill for wave 1; all 4-hit
  (`DAMAGE a=25.00` x4) kills confirmed on wave-1 zombies.
- Spawn ring compliance: 19/19 within 400-600 px of nearest player sample (0 outliers).
- Swarm pressure: 1-2 concurrent zombies; never exceeded bot throughput.
- Pickups/heals: 0 pickups, 0 player damage.