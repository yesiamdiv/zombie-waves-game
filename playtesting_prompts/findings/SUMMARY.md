# SUMMARY - Playtesting results

Dates: S1-S7 run headless (source frozen, no edits). S8 (windowed) not run — no
display in this environment.

## Overall VERDICT
**FAIL** — a Critical w-soft-lock prevents passing wave 1/2 on any evaluated
configuration and makes survival/game-over paths unreachable. Everything around it
(harness, determinism, script system, memory) is solid.

## Blocking issues (Critical / Major)

| ID | Sev | Issue | Repro (seed) |
|----|-----|-------|--------------|
| B1 | **Critical** | Zombie can spawn at 400-600 px but has `detection_range=400`; it idles forever and its wave never ends. Confirmed on seeds 42 (wave 2, S1/S2/S7) and 12 (wave 1, S6c). Game reaches no further waves; survival = item spam for 555/600 s. | `--ai=bot --run-seconds=60 --seed=42` |
| B2 | Major | `--script-loop=N` before the script path silently disables the script (runs with no AI, exit code still 0). | `--ai=script --script-loop=3 <path>` |
| B3 | Major | Headless ignores SIGINT/SIGTERM; `--run-seconds=0` runs are only killed by SIGKILL. Orphans possible in CI. | `kill -INT <pid>&` |
| B4 | Minor | ECS recycles entity ids (`e=`) so per-zombie log analysis is ambiguous. | any multi-zombie log |
| B5 | Minor | Bot never picks up items (medkits/speed) -> heal economy untestable. | `grep -c ITEM_PICKUP` on any bot log=0 |
| B6 | Minor | Item entities accumulate forever during stalls (RSS flat, but unbounded e-table). | S2 log tail |

## RESOLUTION STATUS (post-fix)

**All of B1-B6 are FIXED and re-verified** — see `findings/FIX_VERIFICATION.md`
for per-issue fix + evidence logs. Highlights:
- B1: detection 700 + bot hunts + 60s wave timeout; seed 42 completes waves 1-4, no timeouts.
- B2: `--script-loop=N` before script path now loads and runs the script.
- B3: SIGINT/SIGTERM handled; clean shutdown, no orphaned processes.
- B5: bot heals at HP<75%; 6 pickups in a 60s wounded start (was 0 everywhere).
- B6: items capped at `MAX_ALIVE_ITEMS=8` (down from unbounded).
- Regression: ctest 100%, S3 determinism byte-identical, unit suite green.

**Correction to the S6c assumption:** making B1 fixable did NOT by itself make the
death/game-over path reachable — the elite bot takes zero zombie melee hits at
normal settings (confirmed empirically), a pre-existing trait that B1 had masked.
Death/heal paths are now exercised deterministically via debug knobs
`--player-hp=<pct>`, `--player-damage-mult=<f>`, `--zombie-speed-mult=<f>`
(game-over verified end-to-end with them).

## Non-blocking observations
- S3 determinism is byte-perfect (incl. f= frame numbers); great for CI.
- S4 script happy path works end-to-end incl. `quit`; `Nit` = fires before wave 1.
- S5 missing/malformed scripts degrade gracefully (warn + best-effort); loop count
  `N` = 1 initial + N repeats, matches docs.
- S6a/6b (menu-idle, 0.1 s run) clean.
- S7 no memory growth (2132 kB flat, log 1.3 MB/300 s).

## Suggested fix for B1 (blocks everything else)
In `src/world/waves.c` spawn zombies already in CHASE with an initial velocity toward
the player, or set `detection_range >= 600` (spawn ring max), and add a wave timeout
that force-ends a wave. Then re-run S1/S2/S6c — game-over path becomes reachable and
the heal economy (items) finally gets exercised by the bot.

*(Implementing this suggestion = the B1-B6 fix pass referenced above; see
FIX_VERIFICATION.md. The "game-over becomes reachable" expectation was partially
wrong, corrected above.)*

## Artifacts
- Reports: `findings/S1_baseline.md` .. `findings/S7_memory_sanity.md`;
  `findings/FIX_VERIFICATION.md` (post-fix evidence).
- Logs (seed 42): `s1_bot`, `s2_survival`, `s3_a/b`, `s7_mem`; seed 12: `s6_nodef`;
  seed 7: `s4`; seed 1: `s5_*`; post-fix: `/tmp/opencode/fix_*.log`.