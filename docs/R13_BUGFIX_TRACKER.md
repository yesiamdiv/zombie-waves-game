# R13/D3 Bugfix Tracker

Single source of truth for every bug the playtester agent reported against the
`feature/multiplayer` worktree, and where each one stands.

- **Worktree:** `open-world-zombie-waves-multiplayer`
- **Branch:** `feature/multiplayer`
- **Playtest reports:** `playtesting_prompts/findings/`
  - `DEV_FIX_REPORT_R13_2026-09-25.md` — headless M1–M4 run
  - `07_R13_interactive_device_test_2026-09-25.md` — manual 2-instance on-device run
- **Rule:** SP-visible behaviour must stay byte-identical. Every fix below is
  gated on M1 (two same-seed SP runs `diff` empty, wave 1 = `8z / 1.90s / 1.00`)
  plus `ctest` 3/3.

## Status

| ID | Sev | Area | Status | Commit |
|-----|-----|------|--------|--------|
| R13-C1 | Critical | SP wave budget | **FIXED** | `bc6d87c` |
| R13-C2 | Critical | Remote player ghost | **FIXED** | `bc6d87c` |
| R13-C3 | Critical | Client event log empty | **FIXED** (partial → R13-D1) | `bc6d87c` |
| R13-M1 | Major | 2P scaling magnitude | **FIXED** (with C1) | `bc6d87c` |
| R13-D1 | Major | `POINTS` not relayed | **FIXED** | `56738c4` |
| R13-I3 | Major | Bullets die on items | **FIXED** | `c2686a1` |
| R13-I2a | Major | Beacons missing on client | **FIXED** | `c50a62a` |
| R13-I2b | Minor | Beacon hides player | **FIXED** | `92a2ac2` |
| R13-N1 | Minor | Client bot input dropped | **FIXED** | `26634b0` |
| R13-I1 | Major | Client stutter | **FIXED** | `60033a1` |
| R13-N2 | Minor | Legacy `main` won't compile | NOT ACTIONABLE — see below | — |
| R13-I4 | Question | Host pause freezes client | **OPEN — needs a decision** | — |

## Fixed in `bc6d87c` (round 1)

- **R13-C1** — `main.c` passed the constant `MAX_PLAYERS` (4) as the
  `player_count` argument to `waves_update()`, so the `player_count > 1` scaling
  branch always fired and SP ran the 4-player budget (`14z / 1.75s`).
  Added `players_active_count()` (`src/players.c`) and pass the real occupied-slot
  count. SP is byte-identical again; 2P = `10z / 1.85s`.
- **R13-C2** — `reset_game()` ran before any peer connected, so
  `net_server.slot_used[]` was all-false and no remote slot ever got an entity.
  Added `sync_remote_player_slots()` (`src/main.c`), called after every
  `net_server_update()`; spawns on join, releases on leave.
- **R13-C3** — the client decoded relayed host events into transient queues that
  were drained and dropped, so it wrote no gameplay events to its own log. The
  client now re-emits the host's authoritative events into its local bus.

## Fixed in round 2 (this series)

- **R13-D1** — `relay_net_events()` only relayed
  `GE_ENTITY_DEATH / GE_WAVE_START / GE_PLAYER_HEALTH / GE_KILL`. `GE_POINTS`
  (plus `GE_DAMAGE`, `GE_ITEM_PICKUP`) never crossed the wire, so a client could
  not evidence scoring at all (`POINTS x0`). Added to the allow-list.
- **R13-I3** — `system_collision()` destroyed a bullet on overlap with *any*
  collider. Dropped items carry a collider but no health, so every bullet that
  crossed a dropped item silently died. Not MP-specific — hit single-player too.
  Bullets now skip `COMP_ITEM_TAG` targets.
- **R13-I2a** — `system_render_beacons()` was only called in the non-client
  branch, so a joined player saw no spawn beacons. A render-only client runs no
  simulation, so `sync_client_beacon_slots()` now fills roster slots with
  beacon metadata only (NULL entity) and beacons draw in both branches.
- **R13-I2b** — beacons drew *after* the entities, and the opaque 48x48 core
  covered a player standing on their own spawn. Draw order moved: beacons first,
  then entities.
- **R13-N1** — a headless `--ai=bot --join` client sent an all-zero input packet
  and the host saw a motionless remote player. `send_net_input()` was reading the
  right struct; the real cause was upstream: `ai_build_view()` derives the bot's
  world view from `game.ecs`, and a render-only client has an empty local ECS
  (the world lives only in the snapshot mirror), so the bot saw no zombies. The
  client now builds its `GameView` from the newest mirrored snapshot, which
  carries everything the bot reasons about. The host's own path is untouched.
- **R13-I1** — the client mirror lerp was fed the client's own sim clock, which
  runs ahead of the newest 20 Hz snapshot, so `t` clamped to `1.0` on nearly every
  frame and every entity jumped a full 50 ms step per snapshot. Replaced with a
  render clock driven along the **host's** snapshot timeline and held inside the
  mirror's `[older, newer]` window, so `t` sweeps `0..1` and motion is
  continuous. `client_local_pos()` shared the same broken clock for camera
  follow, so the camera micro-jumped too; it now uses the corrected clock.
  `NET_SNAP_HZ` deliberately stays at 20 — raising it trades bandwidth, and the
  pinned-blend root cause is fixed without it.

## Verification (all fixes applied)

Run on the fixes series, `feature/multiplayer`:

| Gate | Result |
|---|---|
| Clean Release `-Werror` build | passes, **0 warnings in project sources** |
| `ctest` | **3/3** (`net_spike`, `net_loopback`, `core_tests`) |
| M1 SP determinism (seed 42, two runs) | logs **byte-identical** |
| M1 SP wave 1 | `8 zombies, interval: 1.90s, difficulty: 1.00` |
| M2 remote entity | `Spawned remote player entity for slot 1` |
| M3 2P scaling | wave 1/2/3 = `10z/1.85s`, `13z/1.75s`, `16z/1.65s` |
| M2 client log | `POINTS`, `KILL`, `DAMAGE`, `WAVE_START`, `ITEM_PICKUP` all present |
| M4 2P determinism (seed 99) | two 2P hosts produce identical wave starts |
| R13-I1 blend factor | `t` spans `0.000 … 0.833`; **0 of 1905** frames clamped at `1.000` |

The SP byte-identity gate is the important one: every fix above was required to
leave two same-seed single-player runs `diff`-empty with wave 1 at `8 / 1.90 /
1.00`.

### Not re-verified headlessly

`R13-I1` (stutter), `R13-I2a`/`R13-I2b` (beacons) and `R13-I4` (pause) are
render-path or interactive behaviours. `render()` returns immediately under
`--headless`, so these need an on-device two-instance run to confirm visually.
The I1 blend factor was proven by instrumenting the clock instead.

## Not actionable

- **R13-N2** — the legacy `main` tree at `61a0b6b` does not compile
  (`src/ui/menu.c` `opt_colordessert` undeclared, `MapDef` missing
  `width`/`height`). That tree was merged into this branch and the resolution
  compiles clean, so there is no separate `main` binary to compare against. The
  `8 / 1.9 / 1.0` SP baseline is asserted in-repo by M1 instead.

## R13-I4 — host pause freezes the client (decision)

**Observed:** when the host presses ESC to pause, the joined client's game freezes
too. The host is authoritative, so while paused the host's `update()` only
services the pause menu and the simulation clock stops — but the main loop still
calls `step_frame()`, which still calls `broadcast_snapshots()`. The host therefore
keeps streaming *frozen* snapshots, the client's mirror stops advancing, and the
client gets no indication that anything happened. It also cannot unpause itself.

The real defect is not that the client freezes; it is that the client freezes
**silently and with no way out**.

**Decision: global coop pause, with the pause state relayed to clients.**

1. The session pauses for everyone. The simulation genuinely stops, so no host can
   use pause to stall wave progression or otherwise alter shared state for
   everyone else.
2. The host relays its pause state, and the client raises its own pause overlay
   showing who paused, so the freeze is explained and visible.

**Rejected: per-player pause** (host keeps simulating, only the pauser freezes).
It removes the freeze entirely, but it touches the simulation path directly next to
the single-player byte-identity guard, and it would let a host pause mid-wave to
manipulate shared state. It is a reasonable alternative if the user prefers it
later — it is a small change on top of this one, since the relay plumbing is the
same.

**Implementation:** a new append-only `GameEventType`, `GE_HOST_PAUSE`, emitted by
the host whenever its paused state changes and relayed on the existing `GE_*`
channel-0 path. The client already re-emits every relayed event into its local bus,
so the client half needed no new networking — only a HUD message and input
suppression.

