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
|----|-----|------|--------|--------|
| R13-C1 | Critical | SP wave budget | **FIXED** | `bc6d87c` |
| R13-C2 | Critical | Remote player ghost | **FIXED** | `bc6d87c` |
| R13-C3 | Critical | Client event log empty | **FIXED** (partial → R13-D1) | `bc6d87c` |
| R13-M1 | Major | 2P scaling magnitude | **FIXED** (with C1) | `bc6d87c` |
| R13-D1 | Major | `POINTS` not relayed | **FIXED** | (this series) |
| R13-I3 | Major | Bullets die on items | **FIXED** | (this series) |
| R13-I2a | Major | Beacons missing on client | **FIXED** | (this series) |
| R13-I2b | Minor | Beacon hides player | **FIXED** | (this series) |
| R13-N1 | Minor | Client bot input dropped | **FIXED** | (this series) |
| R13-I1 | Major | Client stutter | **FIXED** | (this series) |
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
- **R13-N1** — `send_net_input()` read the raw `game.input`, so a headless
  `--ai=bot --join` client sent an all-zero input packet and the host saw a
  motionless remote player. It now reads the AI driver's `ai_controls` when the
  bot is driving.
- **R13-I1** — the mirror lerp computed `t` against the newest snapshot and
  clamped `t > 1` to `1`, which is true almost every frame at 20 Hz snapshots vs
  high-fps rendering. Entities therefore jumped a full 50 ms step each snapshot
  instead of gliding. The mirror now renders one snapshot behind the newest so
  `t` stays inside `(0,1)`.

## Not actionable

- **R13-N2** — the legacy `main` tree at `61a0b6b` does not compile
  (`src/ui/menu.c` `opt_colordessert` undeclared, `MapDef` missing
  `width`/`height`). That tree was merged into this branch and the resolution
  compiles clean, so there is no separate `main` binary to compare against. The
  `8 / 1.9 / 1.0` SP baseline is asserted in-repo by M1 instead.

## Open — needs a decision

- **R13-I4 — host pause freezes the joined client.** The host is authoritative
  and stops simulating while paused, but it keeps broadcasting frozen snapshots,
  so the client's mirror stops advancing. The client gets no "paused by host"
  indication and cannot unpause. Two reasonable behaviours:
  1. **Coop pause (current, implicit):** keep it, but relay a pause flag so the
     client shows "Host paused" and stops accepting input.
  2. **Per-player pause:** host keeps simulating for everyone else; only the
     pausing player freezes.
  Pick one and it is a small change on top of the existing `GE_*` relay.
