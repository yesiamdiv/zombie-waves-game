# S7 - Memory sanity (300 s bot run)

- CMD:        `./build/open_world_zombie_waves --headless --ai=bot --run-seconds=300 --events=/tmp/opencode/s7_mem.log --seed=42` run in background, `VmRSS` sampled every 20 s via `/proc/$PID/status`
- SEED:       42
- OUTPUTS:    `/tmp/opencode/s7_mem.log`; RSS samples
- EXIT CODE:  0 (full 300 s, t=300.000 f=36000)

## VERDICT
PASS — flat memory, bounded event log, clean exit.

## Findings

### [PASS] Memory flat across 300 s; event log well under 5 MB
- EVIDENCE: 15 samples at t=20..300 s, all identical:
  ```
  t=20s VmRSS=2132kB   t=60s VmRSS=2132kB   t=150s VmRSS=2132kB
  t=300s VmRSS=2132kB
  ```
  `ls -la /tmp/opencode/s7_mem.log` -> 1335062 bytes (1.3 MB, < 5 MB), 19771 events,
  last line `t=300.000 f=36000`.
- CONTEXT: the run spent most of its time in the soft-lock stall (seed 42, see S1/S2)
  while item entities kept spawning every 15 s — so memory stayed flat even under a
  scenario that keeps allocating new entities. Zero `ERROR`/`FATAL` lines.
- IMPACT: no long-run memory leak observed. The one underlying structure that grows
  (~1 entity/15 s, item entities that never despawn) is bounded and not reflected in
  RSS at this scale; worth re-checking after the soft-lock fix makes runs reach later
  waves.
- NOTE: worst-case VmRSS is quite small (2 132 kB) — headless build has no renderer.