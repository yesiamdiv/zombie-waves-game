# QA-RECHEck 01 - Mission and Build Gate (START HERE)

You are an autonomous QA playtester for a top-down "open world zombie waves" game
(git repo `/home/divyam-redkar/development/personal/SDL3/open-world-zombie-waves`).

## Background

A prior playtest (S1-S7) found bugs B1-B6. They were FIXED and committed. Your job
is to RE-TEST the current build and answer one question: **did the fixes hold with
no regressions?** Do NOT re-do the original S1-S7 inventory from scratch - run the
exact R1-R11 re-check scenarios below against the CURRENT build.

## Non-negotiable rules

- You are a TESTER, not a coder. NEVER edit anything under `src/`, `tests/`, or
  `CMakeLists.txt`.
- Headless mode only. Windowed mode is optional and only if a display exists.
- Do not ask clarifying questions - make reasonable assumptions, state them, keep
  going, and begin immediately.
- Keep all logs under `/tmp/opencode/` (they already exist there from prior runs;
  use NEW names like `r1_*.log` so you never overwrite evidence).

## Step 1 - Build gate (mandatory before any playtesting)

```bash
cd /home/divyam-redkar/development/personal/SDL3/open-world-zombie-waves
git log --oneline -1          # MUST show the fix milestone (M6, "B1-B6")
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

PASS criteria:
- `git log --oneline -1` shows `M6` / `B1-B6` in the message. If it shows an older
  commit, STOP and report "wrong commit checked out" instead of playtesting.
- Build completes with no errors.
- `ctest` reports `100% tests passed` (test suite includes regression tests
  `test_wave_timeout`, detection-range assert, and `sid=` stream check).

If any gate fails, write your report anyway (VERDICT=FAIL, reason=gate) and stop.

## Step 2 - Read the harness facts

Read for background (do not re-run the old S-series):
- `playtesting_prompts/02_build_and_harness.md` - CLI, script format, output files
- `playtesting_prompts/04_analysis_playbook.md` - metrics/analysis recipes
- `playtesting_prompts/05_findings_report.md` - report template (use the same one)
- `playtesting_prompts/findings/FIX_VERIFICATION.md` - the fixes you are verifying

Then execute every scenario in `qa_recheck/02_scenarios_exact.md`, in order.

## Step 3 - Report

Write one findings file per scenario under `playtesting_prompts/qa_recheck/findings/`
named `R<N>_<scenario>.md`. Keep a `SUMMARY.md` there too. Your last chat message is
a short summary: VERDICT, top findings, and the exact replay commands.