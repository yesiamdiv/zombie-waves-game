# Multiplayer — Development Progress Log

> **DISCIPLINE (self-reminder, always read this):**
> 1. **Update this file after every work session** — what was technically done, what is
>    left, decisions, blockers. Keep it current so any continuation (or another
>    session) can resume instantly.
> 2. **Git-first**: commit every small, self-contained feature/change with a clear
>    message. Never accumulate uncommitted work across sessions.
> 3. Remember the pairing plan in `docs/MULTIPLAYER_PLAN.md` for the full design.

---

## Session status

| Item | Value |
|------|-------|
| Branch | `feature/multiplayer` |
| Worktree | `/home/divyam-redkar/development/personal/SDL3/open-world-zombie-waves-multiplayer` |
| Build dir | `<worktree>/build` (inside the worktree, separate from main's build) |
| Base | `395e3ae docs: add multiplayer requirements and design plan` |
| Mode of work | Autonomous; all work on the worktree branch, small commits |

## Confirmed feature requirements (from the user)

Multiplayer is a **distinct game mode** (not an add-on to the existing single-player
run). When a client hosts a multiplayer game, they pick one of two modes:

1. **TDM (respawn)**: each player has a player-colored **spawn beacon**; on death the
   player respawns at that beacon after a **fixed respawn timer**.
2. **HARDCORE**: if a player dies they are **permanently out**; there is no respawn.

Multiplayer specifics:

- Spawn beacons exist **only in multiplayer** mode (one beacon per player, visible,
  color-coded, at the player's spawn point; also the respawn anchor in TDM mode).
- Server/state model, snapshot, ENet stack, and pipeline exactly as designed in
  `docs/MULTIPLAYER_PLAN.md` (listen-server, render-only clients, `AIControls` reuse for
  remote input, 20 Hz snapshots over ENet channel 1, reliable events on channel 0).
- Single-player remains a separate, byte-identical run path (compiled-in flag / mode
  switch guards the multiplayer refactor).
- Dev constraint: build lives inside the worktree so it never collides with the main
  directory's dev.

## Development notes (technical)

### Worktree / git hygiene

- `git worktree add -b feature/multiplayer ../open-world-zombie-waves-multiplayer HEAD`
  done on **2026-09-18**.
- `main` (at commit `395e3ae`) is the branch base.
- **IMPORTANT:** the `main` working tree currently contains **uncommitted single-player
  gameplay WIP** the user is developing separately (sword sweep rework, player
  slow-on-damage, zombie AI retune, menu text, +test lines). We must NOT touch or
  commit those; keep multiplayer work isolated in this worktree. Expect a future
  merge/rebase to reconcile them.

## Done

- [x] `docs/MULTIPLAYER_PLAN.md` created + committed on `main` (`395e3ae`).
- [x] Worktree + `feature/multiplayer` branch created; this log added.
- [x] P0.1 — World determinism test (`test_world_determinism`, `tests/tests.c`,
      commit `30a1b84`): two `world_init()` worlds yield identical tiles,
      dimensions, and spawn point. Confirms `world.c` uses no `rand()` → **no world
      transfer needed on join** (D7 resolved in favour of sending only a version).
- [x] P0.2 — `GameMode` enum (`src/game_mode.h`, commit `a61e172`):
      `SINGLE | MULTI_TDM | MULTI_HARDCORE`, `MULTI_RESPAWN_TIME = 5.0f`,
      `--mode=` CLI flag (logs + warns on unknown), default `single` so solo
      behavior is bit-identical. No gameplay branches yet (that's P0.6).
- [x] P0.3 — Per-player slot table (`src/players.h/.c`, commit `f0073dd`):
      `Player players[MAX_PLAYERS]` (in_use/alive/eliminated/name/color/entity/
      input/inventory/kills/shoot_cd/respawn_timer). `system_player_input`,
      `system_sword`, `system_grenades`, `system_rockets` now take `Player *`
      and run per resident slot from main.c. Kill credit: bullet/sword/grenade/
      rocket owners now stamp `CZombieTag.last_hit_by`; `system_cleanup` resolves
      it via `players_find_index` → credits that slot's inventory + `kills`.
      Single-player = slot 0 (SDL input mirrored in each frame); pistol cooldown
      moved to the slot. Verified: 148 tests incl. new `test_kill_credit`
      (2-player kill attribution); 2 seeded headless bot runs → identical logs.
- [x] **Networking spike PROVEN before writing the real net layer**
      (`tests/net_spike.c` → `zombie_net_spike`; ENet `v1.3.18` via FetchContent;
      commit `ceb46b1`): standalone executable (no game code linked) spawns **1 server
      host + N client hosts each bound to its OWN local UDP port** (base
      `52001` → `52001..52004`), all on 127.0.0.1, connecting to one server port
      (default `50910`).
      - 16-byte packed `GamePacket` codec (`version/kind/seq/client_id/x(float)/
        y(float)/hp(u16)/weapon/checksum`), encode + decode verified byte-for-byte;
        checksum rejects damage.
      - Channels as planned: ch0 reliable/ordered = server→client `HELLO`
        handshake; ch1 unreliable/sequenced = client input up / server snapshot
        echo down.
      - Server identifies peers by **source socket port** (never trusts
        client-claimed id) — same rule the real listener will use. `peer->data`
        is only set on the connecting side (found when id arrived as 0).
      - Deterministic per-seq payload; client recomputes expected values from the
        echoed seq and requires exact match.
      - Result: **PASS at 2, 3, and 4 clients** (~800–1.6k round trips each,
        `lost=0`, `decode_err=0`, `echo_mismatch=0`), clean disconnect.
        `ctest` passes both `core_tests` and `net_spike`.
- [x] P0.4 — Nearest-alive-player targeting everywhere (`94ae2c4`). Killed the
      remaining "first player found in the ECS" assumptions now that up to 4
      slots exist:

      - `system_zombie_ai(ecs, players, player_count, dt)`: each zombie chases
        the **nearest ALIVE player** (slot table; falls back to the nearest
        player-tagged entity when `players==NULL` for tests/legacy callers),
        and its ATTACK damages exactly that target (previously hit "the first
        player in the scan" regardless of distance). No alive survivors → zombies
        go idle.
      - `waves_update(ws, ecs, world, players, player_count, dt)` + ring spawning:
        spawn circle centers on the nearest alive player (to world center) so
        hordes converge on the actual fight.
      - `hud_draw(renderer, hud, ecs, waves, const Player *local, ...)`: bound to
        one explicit slot (health = `local->entity`, crosshair = `local->input`,
        shop = `local->inventory`) — no ECS scan for "a" player.
      - `ai_build_view(view, ecs, waves, Entity local_player)`: anchored on the
        driven player entity so bots/AI reason about their own health/position.
      - `Player` struct given a tag (`typedef struct Player {...}`) so headers
        can forward-declare it (waves.h, hud.h) without heavy includes.
      - New `test_nearest_alive_target`: zombie targets nearest of two players,
        then retargets the survivor when the nearest dies (8 checks).
      Verified: **156 tests pass** (was 148), zero warnings; two seeded headless
      bot runs → byte-identical event logs (5 kills, 1351 lines) → determinism
      intact.
- [x] P0.5 — Color-coded spawn beacons (`579d904`): `Player.beacon_pos`
      (spawn/respawn anchor) set in `player_respawn()`; new
      `system_render_beacons()` draws a dark pad + player-colored core + bright
      center at each in-use slot's beacon (camera-culled), rendered **only in
      multiplayer modes** from main.c. HUD shows the slot's colored name tag
      when `multi`. Single-player visuals/determinism unchanged.
      `test_beacon_anchor` locks the anchor + slot color. 160 tests pass.
- [x] P0.6 — Death/respawn per mode: `players_match_update()` runs right after
      `system_cleanup` each frame. A slot whose entity the ECS just destroyed is
      marked dead, then the mode applies: **SINGLE** → just dead (game over when
      slot 0 drops, unchanged); **MULTI_TDM** → arms a `MULTI_RESPAWN_TIME`=5s
      countdown and `player_respawn_slot()` rebuilds the entity at the slot's OWN
      beacon, preserving inventory/kills/name/color (fresh `player_respawn()`
      still requires `!in_use` = join-only); **HARDCORE** → `eliminated=true`
      forever, no timer. Game over is host-side per mode (TDM never fires from
      deaths). HUD shows a red "Respawning at beacon in Xs" countdown /
      "ELIMINATED" overlay for a dead local slot in MP. Refactored a shared
      `spawn_player_entity()` helper; `players.h` now includes `game_mode.h`.
      New `test_tdm_respawn` + `test_hardcore_elimination` (192 tests pass, zero
      warnings). Verified live: forced 1-HP idle script re-loops the real TDM
      death→5s→respawn→death cycle at the spawn position; HARDCORE eliminates
      then ends the game; SP event log still byte-identical to the P0.5 baseline
      → determinism intact.
- [x] P1 — Real net layer (codec + host/join + lobby):
      - P1a — `src/net/net.h` + `net_codec.h/.c`: wire protocol. Fixed 7-byte
        header (version/kind/u16 seq/from_slot/flags/checksum, little-endian,
        covers bytes 0..5). `NET_WIRE_VERSION 2`, `NET_NAME_MAX 32`
        (`NET_NAME_CAP 33`), `NET_MAX_PLAYERS 4`, `NET_DEFAULT_PORT 5123`,
        `NET_WORLD_GEN_VERSION 1`. Channel 0 = reliable/ordered control
        (JOIN/HELLO/REJECT/PLAYER_LIST/LEAVE + reject reasons
        NONE/FULL/VERSION/WORLD/OTHER + leave reasons + mismatch flags),
        channel 1 reserved for P2 snapshots. `net_slot_color()` = fixed 4-color
        palette (blue/red/green/yellow). Codec is byte-exact round-trip +
        truncation-safe name decode (`get_name`). Unit tests in `tests/tests.c`
        → **240 tests pass**.
      - P1b — `net_server.h/.c` + `net_client.h/.c`: listen-server (slot 0 =
        local host, slots 1..3 in join order), JOIN→HELLO(slot, seed,
        world_gen)→PLAYER_LIST broadcast on join/leave, `peer->data` = slot
        (never trust client-claimed identity), client handles
        HELLO/REJECT/PLAYER_LIST/LEAVE, preserves `REJECTED` across transport
        teardown, `wire_version_override` seam for stale-client tests,
        `net_parse_host_port()` defaults port 5123.
      - ENet gotchas learned: `enet_peer_disconnect()` → `enet_peer_reset_queues()`
        races an un-acked reliable REJECT on loopback and drops it before client
        dispatch → `reject_and_drop()` uses **`enet_peer_disconnect_later`** so
        the coded reason is acked first. Client `while` loop re-checks
        `c->host` each iteration (DISCONNECT handler tears the host down).
      - P1c — `tests/net_test.c` loopback ctest (`zombie_net_test` /
        `net_loopback`): 1 host + Alice/Bob join (slots, roster), stale-build
        client (`wire_version_override=1`) is REJECTED with `VERSION` +
        `VERSION_MISMATCH` flag, Cara joins after Bob leaves → slot reuse.
        **net_test passes**.
      - P1d — main.c integration: `--host [--port=N]`, `--join ip[:port]`,
        `--name=N` (defaults name/port; forces `multi-tdm` if a MP mode wasn't
        given). New `GAME_STATE_CONNECTING`/`GAME_STATE_LOBBY`; caller pumps
        `net_server_update()`/`net_client_update()` each frame; `begin_net_session()`
        / `leave_net_session()` manage the session; lobby overlay renders
        roster slots + names + RTT (host reads `slot_peers[i]->roundTripTime`,
        client reads `net_client_rtt_ms()`), host's "ENTER starts the match".
        Net layer is also serviced during PLAYING (leaves/disconnects observed;
        real replication is P2). Local `shutdown()` renamed `shutdown_game()` to
        avoid colliding with POSIX `shutdown` pulled in by ENet headers.
      - P1e — menu entries: options are now `Solo | Host Co-op | Join Co-op |
        Quit`; "Join Co-op" opens a minimal scancode-based address field
        (default `127.0.0.1:5123`, `SDL3` dropped `SDL_SCANCODE_COLON` → ':' maps
        to `SDL_SCANCODE_SEMICOLON`); confirm hands the address to
        `begin_net_session()`.
      - P1f — verified two real processes over loopback: host logs "connection
        from ... joining" → "'Alice' joined -> slot 1" → "broadcast player list
        (2 players)"; client logs "JOIN sent (wire=2)" → "hello from 'Host' ->
        slot 1 (seed=...)" → "player list (2 in lobby) [0]'Host' [1]'Alice'(you)".
        Full ctest green (core_tests + net_spike + net_loopback), zero warnings;
        SP seeded bot runs still byte-identical → determinism intact.
- [x] P2 — snapshot replication, interpolation, net-input → `AIControls` path.
  - P2a — wire codec for ch1 game traffic: `NetInput`/`NetSnapshot`/`NetEntitySnap`
    encode+decode (little-endian f32/u16/u8, bytes-exact round-trips, truncation +
    max-entities guards). 492 tests. Commit `e323990`.
  - P2b — net input up: client streams local input ≥30 Hz (unreliable ch1,
    latest-wins); host folds each remote slot's input into that `InputState`
    through the same path bots use; `InputState.world_aim` drives aim in world
    space. net_test verifies the INPUT exchange; 492 tests. Commit `86e9d2b`.
  - P2c — snapshot replication: `net_snapshot_build()` (host: iterate the live
    ECS, only semantic entities + zombie/bullet/grenade/rocket/item owners +
    slot→entity map) → 20 Hz `net_server_broadcast_snapshot()` fan-out over ch1;
    client keeps the latest snapshot, feeds a two-snapshot `NetMirror`
    (interpolated `net_mirror_sample`), advances to PLAYING on the first
    snapshot, and renders the world from `system_render_mirror` (camera follows
    the interpolated own-slot entity). `--auto-start` lets a headless host start
    the match. Verified end-to-end with two real processes: host "snapshots
    sent=20/40/60… peers=1", client "snapshot seq=… count=1/2", "Match started
    (first snapshot from host)", mirror seq=20/40/… host_sim matches, wave+count
    replicate as zombies spawn. 546 tests; ctest 3/3; SP determinism byte-identical.
  - P2d — events relay (`NET_PKT_EVENTS=8`, ch0 reliable/ordered batches up to
    `NET_EVENTS_MAX_BATCH=48`). Host scans the event-bus ring BEFORE the flush
    drains it and relays a curated subset (GE_ENTITY_DEATH, GE_KILL — zombie
    deaths actually emit GE_KILL — GE_WAVE_START, GE_PLAYER_HEALTH) to every
    peer; client ch0 handler queues dead entity ids + wave start; `drain_net_events`
    applies `net_mirror_push_removing()` (strips dead ids from BOTH mirror bases)
    and shows a "Wave N - M zombies incoming!" HUD message. Codec
    `NET_EVENTS_MAX_BYTES = NET_HDR_SIZE + 1 + 48*20`. Two-process verified:
    client logs "NET: wave 1 starting - 8 zombies". **Debugging note:** the first
    relay scan used `idx = (head - count) % MAX` which read PREVIOUS-session stale
    slots (a count=1 frame scanned the prior frame's entry and skipped its real
    event); fixed to scan `ring[(head + i) % EV_MAX_EVENTS]` with `i < count`.
    560 tests; ctest 3/3; SP determinism byte-identical.
- [ ] *next work items below*
- [ ] P3 — shared waves/items/shop + per-player points + scaling.
- [ ] P4 — disconnect/pause broadcast/player list/chat; `--host --headless`.

## Decision log (D1-D8 from the plan)

- D1 Max players: **4**. — D2 Death/respawn: **user-specified modes** (TDM respawn at
  fixed timer; HARDCORE permanent out). — D3 Difficulty scaling: bump zombies-per-wave
  with player count (tune later). — D4 Authoritative host, no client prediction v1.
  — D5 Dedicated headless server: stretch, P4. — D6 Chat: nice-to-have, cut if it
  delays core parity. — D7 World transfer: verify determinism in P0.1, else send tiles
  at join. — D8 Replay of online sessions: out of scope v1.