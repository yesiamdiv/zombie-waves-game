# QA-Recheck 03 - Report Format

Same template/severity rules as the original playtesting (`05_findings_report.md`).
Write one file per scenario under `playtesting_prompts/qa_recheck/findings/`:
`R<N>_<scenario>.md`, plus a master `SUMMARY.md` you keep updated.

## Per-scenario template

```markdown
# R<N> - <scenario name>

- CMD:        <exact command run>
- SEED:       <seed used>
- OUTPUTS:    <event log, app log paths>
- EXIT CODE:  <0 = clean / other>

## VERDICT
PASS | PASS-WITH-CAVEATS | FAIL   (one)

## Findings
### [Critical|Major|Minor|Nit] <short title>
- REPRO:     <copy-paste command / script>
- EVIDENCE:  <quote exact log lines with t=.../sid=...>
- ROOT CAUSE: <file_path:line and function> - or "none (expected behavior)"
- IMPACT:    <what a player experiences>
- SUGGESTED FIX: <one paragraph, no code changes by you>
```

## Severity taxonomy

- **Critical** - crash, hang, nondeterminism, memory blow-up, corrupted logs.
- **Major** - game-breaking: wave/path soft-lock returns, script won't load,
  signals ignored, game-over/heal unreachable, item cap exceeded.
- **Minor** - balance/feel: TTK, heal economy ratios, spawn-ring outliers,
  difficulty spikes.
- **Nit** - log-format nits, misleading payloads, cosmetic.

## Things that are NOT findings (do not report)

- `PLAYER_HEALTH` count == 0 at default settings (elite bot takes no melee; known).
- `exit=124` on the R8 game-over run (game idles on the game-over screen; judge by
  the `GAME OVER` line).
- Item peak sitting at 8 (that is the cap working).
- Static map detail like the fixed 15s world rain, the 3s wave cooldown, the
  known `ITEM_SPEED_BOOST` permanent +5 speed stack (already documented).

## Evidence rules

- Every finding must quote real log lines (with `t=`/`sid=`) or console output.
- Every finding needs a single-copy REPRO command.
- If you cannot reproduce, say so explicitly. Never invent evidence.

## Final summary (last chat message + SUMMARY.md)

1. Overall VERDICT: PASS / PASS-WITH-CAVEATS / FAIL.
2. Blocking outcomes first: any R1/R2 soft-lock, R3 nondeterminism, R4 script
   regression, R5 orphan, R7 heal untriggerable, R8 game-over unreachable, R10 cap
   exceeded => overall FAIL.
3. Metrics table: waves cleared in R1/R2, first-wave time, survival time, DPS, TTK,
   pickups (R7), time-to-death (R8).
4. Assumptions you made (skipped S8, shortened S2-equivalent, used `timeout` bounds).
5. Replay pack: every copy-paste command/script from the report in one block so an
   engineer can reproduce everything deterministically.