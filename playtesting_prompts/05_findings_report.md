# 05 - Findings Report Format

## Where reports go

Write one report per scenario run under `playtesting_prompts/findings/`, e.g.
`playtesting_prompts/findings/S1_baseline.md`. Name files `<S#>_<scenario>.md`.
Also keep a master `playtesting_prompts/findings/SUMMARY.md` you update as you go
and that becomes your final chat summary.

## Report template (fill in per run)

```markdown
# S<N> - <scenario name>

- CMD:        <exact command run>
- SEED:       <seed used>
- OUTPUTS:    <event log, app log paths>
- EXIT CODE:  <0 = clean / other>

## VERDICT
PASS | PASS-WITH-CAVEATS | FAIL   (one)

## Findings
### [Critical|Major|Minor|Nit] <short title>
- REPRO:     <copy-paste command / script>
- EVIDENCE:  <quote the exact log lines with t=... timestamps>
- ROOT CAUSE: <file_path:line and function> - or "unknown, needs code read"
- IMPACT:    <what a player experiences>
- SUGGESTED FIX: <one-paragraph, no code changes by you>
```

## Severity taxonomy

- **Critical** - crash, hang, memory blow-up, nondeterminism, data corruption.
- **Major** - game-breaking: waves stuck, kills not counted, unwinnable spiral,
  infinite loops in logic.
- **Minor** - balance / feel issues: TTK too slow, heal economy off, spawn ring
  violations, difficulty spike, permanent speed-stack stacking.
- **Nit** - log format nits, misleading payloads, cosmetic.

## Evidence rules

- Every finding MUST quote real log lines (with `t=...`) or console output.
- Every finding MUST have a single-copy REPRO command.
- If you cannot reproduce, say so explicitly instead of inventing evidence.

## Final summary (your last chat message + SUMMARY.md)

1. Overall VERDICT: PASS / PASS-WITH-CAVEATS / FAIL.
2. Top findings by severity (Critical first) with REPRO commands.
3. Metrics table: waves cleared, first wave time, survival time, DPS, TTK.
4. List of assumptions you made (e.g. seed choices, skipped S8).
5. "Replay pack": every copy-paste command and script an engineer needs to
   reproduce all findings deterministically.