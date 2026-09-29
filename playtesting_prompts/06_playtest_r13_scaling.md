# 06 - Playtest: R13/D3 difficulty scaling with player count

Date: 2026-09-25 · Worktree: `open-world-zombie-waves-multiplayer`
Commit under test: `f8891ec` (feature/multiplayer — merge of `main` +
`feature/multiplayer`, union resolution. R13/D3 wave budget scales with
`player_count`; SP wave-1 remains byte-identical to main: 8 zombies /
1.90s interval / 1.00 difficulty).

> **TREE GUARD (read first):** you are testing the **`-multiplayer`** worktree.
> If your shell lands you in `.../open-world-zombie-waves` (no `-multiplayer`
> suffix) you are in the WRONG tree — that is the legacy single-player forked
> base and has no `feature/multiplayer`. Abort and re-anchor to the worktree
> whose path ends in `-multiplayer`.

## Setup

```bash
W=/home/divyam-redkar/development/personal/SDL3/open-world-zombie-waves-multiplayer
cd "$W"
BIN="$W/build/open_world_zombie_waves"
mkdir -p /tmp/opencode
```

## M1 - Single-player byte-identical determinism (R13 MUST NOT move SP)

Two runs, same seed, single local bot. Diff the FULL event logs byte-for-byte:

```bash
$BIN --headless --ai=bot --run-seconds=60 --events=/tmp/opencode/r13_sp_a.log --seed=42
$BIN --headless --ai=bot --run-seconds=60 --events=/tmp/opencode/r13_sp_b.log --seed=42
```

- `diff` must be EMPTY (byte-identical, D1/D5 preserved exactly).
- **Wave-1 budget must be 8** (`5 + 1*3`), interval `2.0 - 1*0.1 = 1.9s`, difficulty
  `1.0 + 0*0.15 = 1.0` — the R13 scaling block (only applies when
  `player_count > 1`) must NOT have touched single-player at all.
- Grep wave-1 start line: `WAVE 1 STARTED` → `(8 zombies, interval: 1.90s, difficulty: 1.00)`.

REGRESSION GUARD: if wave-1 shows anything other than 8z / 1.9s / 1.00 →
**FAIL** (R13 must be SP-transparent).

## M2 - Two-player host+join reaches wave 3 with correct individual points (P3 exit)

**Host** (local slot 0) + **one join bot** (slot 1) both headless, same seed.
Server is authoritative-host; run long enough to clear waves 1→3:

```bash
$BIN --headless --ai=bot --host --port=34901 --run-seconds=180 \
     --events=/tmp/opencode/r13_mp_host.log --seed=7 >/tmp/opencode/r13_mp_host_stdout.log 2>&1 &
HOST_PID=$!

sleep 1
$BIN --headless --ai=bot --join=127.0.0.1:34901 --run-seconds=180 \
     --events=/tmp/opencode/r13_mp_join.log --seed=7 >/tmp/opencode/r13_mp_join_stdout.log 2>&1 &
JOIN_PID=$!

wait $HOST_PID; echo "host exit=$?"
wait $JOIN_PID; echo "join exit=$?"
```

Verify the P3 exit criterion — **wave 3 on BOTH, per-player points, credit owner**:

```bash
grep -c "WAVE 3 STARTED" /tmp/opencode/r13_mp_host.log       # expect >= 1 (host reaches wave 3)
grep -c "WAVE 3 STARTED" /tmp/opencode/r13_mp_join.log       # expect >= 1 (join relays wave 3 too)
grep -c "GE_KILL_CREDIT\|credit.*slot\|\.points += \|points" \
     /tmp/opencode/r13_mp_host.log /tmp/opencode/r13_mp_join.log  # kills credited to owner
```

- Both processes exit 0 (clean shutdown, no crash).
- Wave-3 reached on the host AND mirrored to the join client (P2d relay).
- Points accrue **per player** (each player's inventory), and each zombie's
  kill credit goes to the player who landed the last/hit or kill — not merged.

## M3 - R13 scaling actually engages for >1 player (the whole point of this test)

Compare host wave-1 vs the single-player wave-1 from M1 — SAME wave number, so
the ONLY differing input is `player_count`:

- Host (2 players) wave-1 zombies must be **> 8** (M1's single-player 8).
  Per D3: `zombies_per_wave += (players-1) * per_player_bonus` = `8 + 1*BONUS`.
- Host wave-1 spawn interval must be **faster than 1.90s** (S3: slightly faster
  interval per extra player).

```bash
grep "WAVE 1 STARTED" /tmp/opencode/r13_mp_host.log   # compare numbers vs M1
```

FAIL if host wave-1 == 8z / 1.90s (identical to SP) — that means R13 did not
scale and difficulty scaling is not actually working.

## M4 - Repeatability across runs (same player count → same budget)

Two MORE separate two-player runs with a different seed but same 2-player config;
wave-1 budget must be identical between them (scaling is deterministic per
player count, not random):

```bash
$BIN --headless --ai=bot --host --port=34902 --run-seconds=120 \
     --events=/tmp/opencode/r13_mp2_a.log --seed=99  &
# ... join client, run same as M2 with seed=99 ...
$BIN --headless --ai=bot --host --port=34903 --run-seconds=120 \
     --events=/tmp/opencode/r13_mp2_b.log --seed=99  &
# ... join again ...

grep "WAVE 1 STARTED" /tmp/opencode/r13_mp2_a.log /tmp/opencode/r13_mp2_b.log
```

Wave-1 budget must MATCH between a and b (byte-equal wave-1 start lines) —
scaling is a pure function of (wave_number, player_count), not RNG.

## Exit criteria

- **M1**: 2x SP diff empty + wave-1 == 8z/1.90s/1.00 on both runs.
- **M2**: host & join both clean-exit 0, both reach wave 3, per-player points
  credited to kill owner.
- **M3**: host 2-player wave-1 budget > SP 8 AND faster interval (scaling engaged).
- **M4**: two 2-player runs same-seed → identical wave-1 budgets (deterministic).

Any M1-M4 failure = **FAIL** (report which milestone + paste the failing grep
line + the `diff`/exit-code evidence). All four green = **PASS** R13/D3.

## Report

`/tmp/opencode/06_findings.md` with one section per milestone (M1-M4): PASS/FAIL,
the exact awk'd wave-1 line for host vs SP, exit codes, and the byte-level diff
result for M1. Append to `playtesting_prompts/findings/` per the house convention.
