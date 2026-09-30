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
| R13-I4 | Question | Host pause freezes client | **FIXED** (decision below) | `a555040` |
| R13-I5 | Major | Client draws dots, not sprites | **OPEN — needs a decision** | — |
| R13-I6 | Major | Remote spawns at host's feet | **OPEN** | — |

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

`R13-I1` (stutter), `R13-I2a`/`R13-I2b` (beacons) and `R13-I4` (pause overlay) are
render-path or interactive behaviours. `render()` returns immediately under
`--headless`, so these need an on-device two-instance run to confirm visually.
The I1 blend factor was proven by instrumenting the clock instead. R13-I4's
*relay* is proven headlessly (see its section), but its HUD notice, frozen
rendering and still guest ghost are not.

`playtesting_prompts/08_playtest_r13_fix_verification.md` is the consolidated
re-verification brief for the whole series, with each milestone tagged
`[HEADLESS]` or `[GUI]`.

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

**Fixed in `a555040`.** The client tracks `host_paused` from the relayed event, freezes
its own loop while set, shows a "Paused by host" / "Host resumed" HUD notice, and
streams a **neutral** input packet so the remote player does not appear to keep acting
in a world that is not advancing. The neutral packet keeps the normal 30 Hz cadence
rather than bypassing the rate limiter (an earlier draft returned before the
accumulator check and would have sent one packet per frame).

Because the event rides the existing curated `GE_*` relay, there is no new packet
kind and no wire-version bump. `GE_HOST_PAUSE` is appended to the enum, so the
existing type byte stays stable.

**Verified headlessly** with a temporary **host-only** auto-pause harness, since the
`--script` harness cannot drive this fix: it sets *held* keys (`input.keys[]`) while
`input_key_pressed()` reads the *press edge* array (`keys_pressed[]`), which only real
SDL events fill — so a scripted `key Escape down` cannot open the pause menu at all.
That harness was removed before the commit. With it in place:

- host stdout: `NET: host pause state -> PAUSED (relayed)` then `-> PLAYING (relayed)`
- client stdout: `NET: host PAUSED the session` then `host resumed the session`
- client `--events` log: `EVT=HOST_PAUSE` ×2
- the D1 relay is unaffected: `DAMAGE 185`, `ITEM_PICKUP 11`, `KILL 34`, `POINTS 34`,
  `WAVE_START 3` still arrive

An earlier draft of that harness was **wrong and its results were discarded**: the
auto-pause trigger was placed in `update()` without a host guard, so it also fired in
the *client* process. The client appeared to pause and resume in lockstep with the
host, which looked like a successful relay, but the client was pausing itself. The
false pass was caught only because the client decoded no `HOST_PAUSE` batch. Lesson:
instrumentation in a shared code path must be gated on `net_host_mode`.

**Still needs an on-device pass** for the parts `--headless` cannot reach: the
"Paused by host" / "Host resumed" HUD notice, the client's frozen rendering, and the
guest ghost holding still. Milestone M8 of `playtesting_prompts/08_playtest_r13_fix_verification.md`.


## R13-I5 — the client renders dots, not sprites (found on-device 2026-09-29)

**Reported by:** the user, watching two real windows — "the player shapes are
different for client and host".

**Root cause:** the host and the client do not share a renderer.

| | host | client |
|---|---|---|
| entry point | `system_render_entities` (`src/systems/render.c`) | `system_render_mirror` (`src/systems/render.c`) |
| player | the entity's own `CSprite`, built by `spawn_player_entity` as `sprite_rect(16, 16, color)` | hardcoded `sprite_circle(10.0f, net_slot_color(slot))` |
| zombie | real `CSprite` | `sprite_circle(11.0f, COLOR_GREEN)` |
| bullet / item | real `CSprite` | `sprite_circle(3.0f)` / `sprite_circle(7.0f)` |

`system_render_mirror` never reconstructs sprite geometry. It switches on
`NetEntitySnap.kind` and invents a circle size per kind, so every entity in a
joined client's world is a flat dot. Colour is correct — `net_slot_color(0)` is
`COLOR_BLUE`, the same constant the host's local player uses — so the mismatch
is purely shape and size.

**Why I2a/I2b did not catch this:** those fixes made the beacon render *beneath*
the player instead of over it, and both were only ever checked as "is the beacon
in front". On the client there was never a real player sprite underneath to
reveal, so ordering a beacon under a 10px circle looks like a successful fix
while the actual character is still missing. This is the case
`08_playtest_r13_fix_verification.md` M6 was written to catch, and the one class
of defect I could not check myself — see "Not re-verified headlessly".

**Decision: mirror the host's shape constructors client-side; do not put sprite
geometry on the wire.** `system_render_mirror` should call the same
`sprite_rect` / `sprite_circle` constructors that `spawn_player_entity` and the
zombie/item spawners use, keyed off `NetEntitySnap.kind`, so both processes agree
on shape without touching `NET_SNAP_ENTRY_BYTES` or the wire version.

**Rejected: send the sprite over the snapshot.** It would keep the two renderers
honest automatically, but it widens every snapshot entry, changes
`NET_SNAP_ENTRY_BYTES` (26) and the wire version, and pushes render concerns into
the netcode. Not worth it while a shared shape table is sufficient.

## R13-I6 — a mid-match joiner spawns at the host's feet (found on-device 2026-09-29)

**Reported by:** the user, watching two real windows — "when they are spawned for
one of them i can see that it looks like they spawned under the spawner area".

**Root cause:** three places establish a player position and they disagree on the
anchor. Only the mid-match path is wrong.

| site | anchor | |
|---|---|---|
| `main.c:150` local player, and `main.c:161` remote slots at match start | `world_get_spawn_point() + (s*70, s*30)` | correct |
| `main.c:205` `sync_remote_player_slots()` — the mid-match join path | **the host's live position** `+ (s*70, s*30)` | **wrong** |
| `main.c:261` beacon anchor for a roster slot | `world_get_spawn_point() + (s*70, s*30)` | correct |

`sync_remote_player_slots()` reads `players[0]`'s current position and offsets from
it, so a peer that joins while the host is loitering next to the spawner structure
is materialised inside it. It is also self-inconsistent: the same slot's beacon
anchor is set from the world spawn point, so on a mid-match join the body and its
beacon are at two different places.

**Decision:** use `world_get_spawn_point()` as the single anchor for all player
spawns, including the mid-match path. Drop the `players[0]`-relative branch
entirely rather than keeping it as a fallback, so there is exactly one place that
decides where a player starts.
