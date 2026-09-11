# FIX_VERIFICATION - B1-B6 all resolved, re-verified post-fix

Date: current sprint. Source was EDITED to fix B1-B6; all verification below was
re-run against the fixed binary. Logs in `/tmp/opencode/fix_*.log`.

## RESOLVED

| ID | Sev | Fix | Evidence (fixed binary) |
|----|-----|-----|-------------------------|
| B1 | Critical | Zombie `detection_range` = `SPAWN_RING_MAX + 100` (700); bot always hunts nearest zombie (wall-slide approach); `WAVE_MAX_DURATION 60s` force-end safety net | Seed 42 120s bot run: waves 1-4 all COMPLETE, `WAVE_END` for every wave, **no** force-end hits (was: wave 2 soft-locked 60+s). Stuck-zombie analyzer found no zombie static >15s. |
| B2 | Major | `--ai=script` decoupled from path; `--script-loop=N` order-independent; bare trailing path accepted | `--ai=script --script-loop=2 /tmp/opencode/s4.script` loads script and exits 0 via `quit` (previously: script silently dropped). |
| B3 | Major | SIGINT/SIGTERM handler sets stop flag; clean shutdown log + exit | `kill -INT <pid>` mid-run => `main.c:558 Interrupt received; shutting down cleanly`; no orphaned process. |
| B4 | Minor | Monotonic `sid=` serial on every line + header | All lines carry `sid=`; header format updated; `docs/EVENT_FORMAT.md` documents it. |
| B5 | Minor | Bot seeks medkits at HP<75% (item within 500); items also drop ~40% per zombie kill (near the fight); wall-slide item approach | `--ai=bot --run-seconds=60 --seed=42 --player-hp=40 --player-damage-mult=2`: **6 `ITEM_PICKUP`** events incl 3 health +30 (previously 0 in every run). Bot survives full 60s from 80 HP under 2x damage - healed up. |
| B6 | Minor | Item spawns (rain + kill-drops) gated by `items_count_alive() < MAX_ALIVE_ITEMS(8)` | 120s/50-kill run: only 7 items ever alive at once (peak), kill-drops suppressed vs ~20 candidates (0 pickups all run). |

## New correction to the original S6c assumption

Original finding claimed fixing B1 makes the death/game-over path reachable and
exercises the heal economy naturally. **Verified FALSE post-fix**: the elite bot
takes **zero** zombie melee hits at normal settings (600 player-damage samples in
120s are all `POSITION_SAMPLE`, and `grep PLAYER_HEALTH` = 0) - pre-existing
behavior that B1's soft-lock previously masked. Zombies are shredded at range
before closing to melee, so the player essentially never dies with this bot.

To keep those paths **deterministically testable**, three debug knobs were added:

- `--player-hp=<pct>` start wounded (exercises heal-seeking without waiting to get hit)
- `--player-damage-mult=<f>` raise zombie melee damage
- `--zombie-speed-mult=<f>` make the horde close to melee before being shredded

Death/game-over verified end-to-end:
```
$BIN --headless --ai=bot --seed=42 --zombie-speed-mult=10 --player-damage-mult=5
=> GAME OVER - Score: 5100, Wave: 4, Kills: 36  (t=83.983, KILL k=player; 4 melee hits x 50 = 200 HP)
```

## Regression sweep (must stay green)

- Unit tests: `ctest` 100% pass (incl. new `test_wave_timeout`).
- Determinism (S3): two same-seed runs still byte-identical (`cmp` clean).
- `--script-loop` before script path: script loads + runs.
- SIGINT: clean shutdown, no orphans.
- Item cap holds (B6) even with kill-drops added.

## Known remaining item

`ITEM_SPEED_BOOST` still permanently stacks +5 speed per pickup (no unbounded
health/ammo accumulation thanks to B6, but speed stacking is still a balance
smell - noted in S1/02, not part of this fix set).