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
- [ ] *next work items below*

## In progress / next

- [ ] P0.3 — `players[]` table: up to `MAX_PLAYERS(4)` slots each holding owned
      entity, name, color, `InputState` (posted), `PlayerInventory`, kill count,
      alive/respawn state — replace single `game.input` + `game.inventory` use in
      systems (invoke via a `player_index_of(entity)` helper); kill credit via
      `CBulletTag.owner` → the owning slot's inventory.
- [ ] P0.3 — `players[]` table (per-player `InputState`, `PlayerInventory`, entity,
      color, beacon, name) replacing the single `game.input`/`game.inventory` usage.
- [ ] P0.4 — Generalize player systems (`zombie_ai`, `sword`, `grenades`, `rockets`,
      `cleanup` kill-credit-by-owner) to N players.
- [ ] P0.5 — MP spawn beacons (visual component + per-player respawn anchor).
- [ ] P0.6 — Death/respawn: TDM fixed-timer respawn at beacon; HARDCORE permanent-out;
      game-over condition per mode.
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