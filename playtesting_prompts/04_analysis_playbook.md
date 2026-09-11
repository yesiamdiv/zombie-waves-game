# 04 - Analysis Playbook

Compute these from each event log. Use `t=` (sim seconds) for all timings.
Example helpers you may use with awk/grep/rg in a shell:

```bash
# spawn ring compliance: distance of each zombie ENTITY_SPAWN from player's first sample
grep EVT=ENTITY_SPAWN /tmp/opencode/s1_bot.log | grep k=zombie
# per-zombie time to kill: Dfirst damage for entity e=… to KILL e=…
# wave timing: first WAVE_START t=… to WAVE_END t=…
```

## Metrics

- **Spawn ring compliance.** For every zombie `ENTITY_SPAWN` (k=zombie), compute
  distance to the player's `POSITION_SAMPLE` (k=player) nearest in time. Accept
  400-600; report outliers (Fallback edge spawns are ~1400px from map center -
  flag any zombie spawning that far from the player).
- **Time-to-kill (TTK).** From first `DAMAGE` on entity e to its `KILL`. Wave-1
  zombie takes 4 × 25 = 100 = exactly dead; TTK should be ~4 consecutive shots.
- **Bot DPS.** `PLAYER_SHOT` count / active combat seconds (from first to last
  zombie killed). Compare against bot fire rate ~8.3 shots/s × 25 dmg ≈ 208 DPS.
- **Wave-clear time.** `WAVE_START` → `WAVE_END` per wave. Note growth over waves
  and any wave that never ends.
- **Player health trajectory.** Series of `PLAYER_HEALTH`/`DAMAGE k=player`; when
  health drops below ~30 and stays there, medkit pace matters.
- **Heal economy.** `ITEM_PICKUP a=1 b=30.00` count vs total player-damage in the
  same window (sum of `DAMAGE k=player a=`). If total healing << total damage,
  flag unsustainable difficulty. Also flag if a `SPEED_BOOST` pickup (`ITEM_PICKUP
  a=3`) permanently stacks speed (see `02`, known design smell).
- **Swarm pressure.** Concurrent alive zombies: count distinct zombie e-ids in
  `POSITION_SAMPLE` windows; compare peak vs bot throughput.

## Expected baselines (from `02` constants)

- Wave 1: 8 zombies, 4 hits/zombie, 0.12s min between shots.
- Spawn ring 400-600; difficulty ×1.15/wave; zombie speed grows too.
- Player: 200 HP, heals 30 max per medkit, medkit spawns every 15s.

## Determinism check

Byte-identical event logs for identical `--seed` (and identical command). Run
twice, `diff`. Nondeterminism sources to suspect: `rand()` misuse in gameplay
code, uninitialized values, timing-dependent logic.

## Crash / hang detection

- App log: grep for `FATAL|ERROR|Segmentation|abort`. Exit code != 0 is a
  Critical finding.
- Hang: run under `timeout 60`; if it doesn't reach `--run-seconds` and exit
  code is 124, suspect an infinite loop.

## Event-stream sanity

- Every spawned zombie eventually gets a `KILL` or a `GAME OVER` follows - no
  unkillable/stuck zombies.
- No `KILL`/`DAMAGE` with `e=4294967295` payloads that look malformed.
- Player `POSITION_SAMPLE` always present once playing, and stops at game over.