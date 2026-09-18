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
- [ ] *next work items below*

## In progress / next

- [ ] P0.5 — Spawn beacons (MP only): colored beacon per player at spawn +
      respawn anchor (TDM); render + HUD color-coded.
- [ ] P0.6 — Death/respawn per mode: `MULTI_RESPAWN_TIME` timer in TDM,
      `eliminated` in HARDCORE; game-over only when all players gone.
- [ ] P1 — ENet fetch (`FetchContent v1.3.18`) + codec + host/join UI + handshake.
- [ ] P2 — snapshot replication, interpolation, net-input → `AIControls` path.
- [ ] P3 — shared waves/items/shop + per-player points + scaling.
- [ ] P4 — disconnect/pause broadcast/player list/chat; `--host --headless`.

## Decision log (D1-D8 from the plan)

- D1 Max players: **4**. — D2 Death/respawn: **user-specified modes** (TDM respawn at
  fixed timer; HARDCORE permanent out). — D3 Difficulty scaling: bump zombies-per-wave
  with player count (tune later). — D4 Authoritative host, no client prediction v1.
  — D5 Dedicated headless server: stretch, P4. — D6 Chat: nice-to-have, cut if it
  delays core parity. — D7 World transfer: verify determinism in P0.1, else send tiles
  at join. — D8 Replay of online sessions: out of scope v1.