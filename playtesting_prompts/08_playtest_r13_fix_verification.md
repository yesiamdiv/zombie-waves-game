# 08 - Playtest: R13 fix series verification (C1-C3, D1, I1-I4, N1)

Date: 2026-09-29 · Worktree: `open-world-zombie-waves-multiplayer`
Commit under test: `a555040` (feature/multiplayer — the R13 fix series; SP gate
re-verified: 8 zombies / 1.90s interval / 1.00 difficulty, byte-identical).

This milestone **re-verifies the fixes** landed since `f8891ec`. It does not
re-test R13/D3 itself (prompt 06 already did that). What it must prove is that
the ten fixes hold, and — just as important — that they did **not** move
single-player.

Each milestone is marked **[HEADLESS]** or **[GUI]**. `--headless` returns
immediately from `render()`, so anything visual (beacons, bullets, mirror
interpolation, HUD, pause overlay) **cannot** be verified headlessly and needs
two real windows. Do not report a GUI milestone PASS from a headless run.

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

For the **[GUI]** milestones, two instances must be detached or the command
hangs waiting on a window:

```bash
IP=$(hostname -I | awk '{print $1}')
setsid $BIN --host --port=35200 --name=Host \
  >/tmp/opencode/r13_host.log 2>&1 </dev/null &
sleep 2
setsid $BIN --join=$IP:35200 --name=Guest \
  >/tmp/opencode/r13_guest.log 2>&1 </dev/null &
```

Shut down with explicit PIDs (`pgrep -f open_world_zombie_waves | xargs -r kill`).
Do **not** use `pkill -f open_world_zombie_waves` — the pattern matches the
shell running the command and kills your own tool call.

## M1 - Single-player must not have moved [HEADLESS] (regression guard)

Every R13 fix is multiplayer-only. If SP changed, the whole series is void.

```bash
$BIN --headless --ai=bot --run-seconds=25 --events=/tmp/opencode/r13x_a.log --seed=42
$BIN --headless --ai=bot --run-seconds=25 --events=/tmp/opencode/r13x_b.log --seed=42
diff -q /tmp/opencode/r13x_a.log /tmp/opencode/r13x_b.log && echo DIFF_EMPTY
```

- `diff` must be **EMPTY** (byte-identical).
- Wave-1 must be exactly `(8 zombies, interval: 1.90s, difficulty: 1.00)`.

REGRESSION GUARD: anything other than 8z / 1.90s / 1.00, or a non-empty diff
→ **FAIL** immediately and stop; the rest of the series is untrustworthy.

## M2 - C1: wave budget scales with real player count [HEADLESS]

Host + one join, same seed, long enough to clear waves 1→3:

```bash
$BIN --headless --ai=bot --host --port=35201 --auto-start --run-seconds=90 \
     --events=/tmp/opencode/r13x_host.log --seed=7 >/tmp/opencode/r13x_host_stdout.log 2>&1 &
HOST_PID=$!
sleep 1
$BIN --headless --ai=bot --join=127.0.0.1:35201 --run-seconds=85 \
     --events=/tmp/opencode/r13x_join.log --seed=7 >/tmp/opencode/r13x_join_stdout.log 2>&1
JOIN_PID=$!
wait $HOST_PID
```

Pinned literal values — the 2P wave budget must be **exactly**:

| wave | zombies | interval | difficulty |
|------|---------|----------|------------|
| 1    | 10      | 1.85s    | 1.00       |
| 2    | 13      | 1.75s    | 1.15       |
| 3    | 16      | 1.65s    | 1.30       |

C1's bug was scaling off a stale/always-1 player count. The 2P values must
differ from SP (8/1.90/1.00) and match the table above. A 2P wave 1 of
`8 zombies, interval: 1.90s` means the count never engaged → **FAIL**.

## M3 - C2: remote player entity appears and is reconciled [HEADLESS]

C2's bug was the host never spawning (or never removing) the joined peer's
entity, so a client saw an empty world.

```bash
grep "Spawned remote player entity for slot" /tmp/opencode/r13x_host_stdout.log
```

- Slot 1 must spawn once, within a few seconds of the join.
- Client mirror line must report `ents` **> 0** and must track the host's
  entity count as waves spawn zombies:

```bash
grep "mirror seq" /tmp/opencode/r13x_join_stdout.log | tail -n 1
```

- Expected shape: `CLIENT mirror seq=<N> host_sim=<t> wave=<w> ents=<E>` with
  `E` growing as wave `w` spawns. `ents=0` while zombies are alive → **FAIL**.

Leave/rejoin check: kill the client, rejoin, and confirm the host does not
leak a stale entity (host stdout must not show a second
`Spawned remote player entity for slot 1` for the same live peer, and a
second joiner must land on a fresh slot).

## M4 - C3 + D1: authoritative gameplay events reach the client [HEADLESS]

C3's bug was the render-only client logging no gameplay events at all; D1's was
that points/damage/pickups were never relayed even once events flowed.

```bash
grep -oE 'EVT=[A-Z_]+' /tmp/opencode/r13x_join.log | sort | uniq -c
```

The client log must contain all of:

- `EVT=WAVE_START` — one per wave the session reached (3 if it reached wave 3)
- `EVT=KILL`, `EVT=POINTS` — points must be credited to the **kill owner**,
  so the two counts should line up per kill, and the *joining* client's log
  must show the host's kills, not an empty session
- `EVT=DAMAGE` — non-zero count
- `EVT=ITEM_PICKUP` — non-zero count (drops are reachable in an ordinary run)

Any of these missing entirely → **FAIL**. Note `EVT=INPUT` / `EVT=POSITION_SAMPLE`
on the client are *local* instrumentation (the client sends input and samples
its mirror) and are **not** part of the relay evidence — do not count them.

## M5 - I1 + N1: mirror clock advances; client bot view is mirror-driven [HEADLESS + GUI]

- **I1** [HEADLESS] — the render clock must not pin. In the same
  `grep "mirror seq"` output as M3, `host_sim=` must increase monotonically
  across samples and must roughly equal the host's own elapsed time. The I1
  symptom was the mirror `t` sticking at `1.000`; a `host_sim=` that never
  grows → **FAIL**.
- **N1** [GUI] — with the client running `--ai=bot`, the guest window's bot
  must visibly aim and shoot at zombies **using the mirror**, not a stale local
  world. If the guest bot is frozen, empty, or shooting at nothing while the
  host is clearly being attacked → **FAIL**. (N1 built the client's AI view
  from the snapshot mirror; previously it was built from the local ECS, which
  is empty for a render-only client.)

## M6 - I2a + I2b: remote player is visible on the client [GUI] (not headless)

I2a spawned a beacon for the remote player; I2b fixed beacons being drawn
**above** the player so the body was hidden underneath.

Run the two-instance GUI setup. On the **guest** window:

- The host's player must be visible and clearly distinguishable.
- The host's avatar must be **readable as a character** — you must see the
  sprite/body, not only a floating marker. If the only thing you see is a
  marker pinned over an invisible body → **FAIL** (that is exactly the I2b
  regression).
- Beacon markers must sit **underneath** the sprite, not on top of it, and
  must not occlude the player.

## M7 - I3: dropped-item bullets are visible [GUI] (not headless)

I3's bug: bullets from dropped items were not rendered at all.

In the host or guest window, when the player drops an item (or items drop on
death), the bullet/marker for the dropped item must be **visible** and must
**disappear when picked up**. A dropped item that is invisible, or a bullet
that never clears after pickup → **FAIL**.

## M8 - I4: host pause is global and reaches the client [GUI] (headless CANNOT drive it)

I4's bug: ESC changed only the host's local state. A client kept sending input
and kept animating while the host was frozen, and had no way to learn the pause
had been lifted.

**Why this milestone is GUI-only:** the `--script` harness sets *held* keys
(`input.keys[]`) but `input_key_pressed()` reads the *press edge* array
(`keys_pressed[]`), which only real SDL events fill. A scripted `key Escape
down` therefore **cannot** trigger the pause, and a headless run cannot produce
a pause at all. Use two real windows and press ESC.

With the two-instance GUI setup running:

1. Press **ESC in the host window**.
   - Host: pause menu opens, world freezes.
   - **Guest: must also freeze** — the guest's world must stop advancing, its
     remote-player ghosts must stop moving, and a **"Paused by host"** notice
     must appear. A guest that keeps running → **FAIL**.
2. Press **ESC in the host window** to resume.
   - Guest must resume and show a **"Host resumed"** notice. A guest stuck
     frozen after the host resumed → **FAIL** (this was the original trap: the
     client could never learn the pause was lifted).
3. Confirm the guest's player does not "keep acting" while frozen — the ghost
   must hold still, not walk away from where it was.

## Exit criteria

- **M1**: SP diff empty AND wave 1 == `8z / 1.90s / 1.00`.
- **M2**: 2P waves 1-3 == `10/1.85/1.00`, `13/1.75/1.15`, `16/1.65/1.30`.
- **M3**: host spawns slot 1; client `ents` > 0 and tracks; no leaked entity
  on rejoin.
- **M4**: client log has `WAVE_START`, `KILL`, `POINTS`, `DAMAGE`, `ITEM_PICKUP`.
- **M5**: `host_sim` grows (no `t=1` pin); guest bot acts on the mirror.
- **M6**: guest sees the host's player as a readable sprite, beacon beneath.
- **M7**: dropped-item bullet visible, and clears on pickup.
- **M8**: ESC on host freezes the guest with a "Paused by host" notice; second
  ESC resumes the guest with a "Host resumed" notice.

Any milestone failure = **FAIL**. Report which milestone, the exact failing
grep/stdout line, and the observed vs expected literal values. All eight green
= **PASS** for the R13 fix series.

**Partial-credit rule:** M1 is a hard gate. If M1 fails, report that and stop
— do not continue to M2-M8, because every later result is then meaningless.

## Report

Write `/tmp/opencode/08_findings.md` with one section per milestone (M1-M8):
PASS/FAIL, the exact grep'd line(s) for the pinned values (host vs SP wave-1),
exit codes, and for the GUI milestones a one-line description of what was
actually seen on screen. Append to `playtesting_prompts/findings/` per the house
convention.

For each GUI milestone, record **which instance** you observed it on (host or
guest) — several of these bugs were only visible from one side, and "it looked
fine" without naming the window is not evidence.
