# S3 - Determinism (two identical runs)

- CMD:        `./build/open_world_zombie_waves --headless --ai=bot --run-seconds=60 --events=/tmp/opencode/s3_a.log --seed=42` then same with `s3_b.log`
- SEED:       42
- OUTPUTS:    `/tmp/opencode/s3_a.log`, `/tmp/opencode/s3_b.log`
- EXIT CODE:  0, 0

## VERDICT
PASS — byte-identical event logs within the pair.

## Findings

### [PASS] Two runs of the same seed produce byte-identical event logs
- EVIDENCE:
  ```
  $ cmp /tmp/opencode/s3_a.log /tmp/opencode/s3_b.log ; echo $?    -> 0
  $ diff /tmp/opencode/s3_a.log /tmp/opencode/s3_b.log             -> empty
  ```
  Every event line survives exactly, including derived `f=` frame numbers and all
  coordinates/fractions (e.g. both logs have 19 `ENTITY_SPAWN k=zombie` with identical
  `x= y=` values, both logs end `t=60.000 f=7200`).
- ADDITIONAL: `/tmp/opencode/s1_bot.log` (S1, same CMD + seed) is also byte-identical
  to `s3_a.log` — determinism holds across separate invocations, days apart.
- INTERPRETATION: the soft-lock bug (S1-Major) is reproduced bit-for-bit in every run.
  Determinism is reliable both for the good behavior and the bug; a wave-timeout or
  counter-metric added by the fix must preserve this (e.g. via `f=`-based seeds).
- IMPACT: deterministic headless CI is feasible. Note that determinism made S1's
  soft-lock highly reproducible for debugging, but also means bot-based playthroughs on
  this seed always hit the same stall (see S2).