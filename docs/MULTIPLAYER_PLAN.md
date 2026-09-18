# Multiplayer (LAN Co-op) — Requirements & Plan

Status: **Planning** (this is the requirements + design document, not implemented code)
Scope: local-area-network cooperative play (a handful of humans on the same network, no internet matchmaking).

---

## 1. Goal

Let 2-4 players survive the same open-world zombie waves together on a LAN:

- One shared, authoritative world; one shared wave counter.
- Every player sees every player (and all zombies) moving coherently.
- Each player keeps their **own** weapon inventory, shop points, and HUD.
- Runs on the existing engine: C11, SDL3, ECS, one codebase. The host's machine runs
  the full simulation exactly like today; remote machines send input and render.

---

## 2. Non-negotiable ground rules (from the codebase)

These shape every decision below:

- **One-directional inputs today.** `system_player_input` moves and fires the single
  `COMP_PLAYER_TAG`-tagged entity it finds (first match). Same assumption exists in
  `zombie_ai.c`, `sword.c`, `grenades.c`, `rockets.c`, `hud.c`, `event_bus.c`,
  `waves.c`. Multiplayer = generalize these to N players.
- **The AI input-injection path already exists and is the perfect hook for remote
  players.** `ai_apply_controls()` takes an `AIControls` struct and writes it into
  the real `InputState` via `input_inject_*` before systems run each frame. A remote
  player is just another "driver" that produces `AIControls` from the network
  instead of a bot/script. `src/ai/ai_driver.c`, `src/ai/bot.c`.
- **The ECS is a fixed-size SoA**: `ECS_MAX_ENTITIES=2048` entity slots, component
  arrays indexed by entity id (`positions[]`, `velocities[]`, `healths[]`, tag
  arrays...). Entity ids are stable array indices — a snapshot can address entities
  directly by index. `src/ecs/ecs.h`.
- **World is procedural, tile-fixed**: `world_init()` builds a deterministic 50x50
  tile map (3200x3200 px) with fixed spawn point(s). If it is deterministic across
  machines (to be verified, see §8), **we never send the world over the wire — only
  a seed/version**.
- **The entire game already runs headless** (`--headless`, fixed 120 Hz timestep,
  no window/renderer) — this is a free dedicated-server mode (§10).
- **The event bus** (`src/events/event_bus.h`, `GE_*` events, per-event monotonic
  serial) is a natural place to source the gameplay events (damage, kills, pickups,
  wave starts) we want to relay to clients for HUD/audio flavor.
- **Testing harness exists**: `zombie_tests` (77 assertions, green) and the
  headless AI/script machinery used by manual and bot playtests.

---

## 3. Requirements

### 3.1 Functional requirements

| ID | Requirement |
|----|-------------|
| R1 | Host a game from the main menu: choose name, port, max players (default 4). |
| R2 | Join a game from the main menu by host IP:port; enter your player name. |
| R3 | All connected players spawn into the same world and shared wave progression. |
| R4 | Each player controls only their own character (movement + aim + fire). |
| R5 | Remote players are visible (distinct color + name tag) and collide with the world/zombies. |
| R6 | Weapons fire server-side; remote clients see every shot and its effects. |
| R7 | Per-player inventory/points/shop: points from kills credit the **owner** of the killing bullet (`CBulletTag.owner`). |
| R8 | Items (health/ammo/speed) spawn server-side once and are pickable by any player. |
| R9 | Host pauses/unpauses for the whole group; shop is per-player but gameplay pauses for everyone (existing pause semantics). |
| R10 | Disconnect handling: a player dropping out despawns their character; host leaving ends the session for everyone. |
| R11 | In-game player list (names, colors, ping, HP) visible via pause menu or HUD. |
| R12 | Chat: basic text chat between players (LAN, nice-to-have). |
| R13 | Difficulty scales with player count (e.g., zombies per wave, spawn interval). |

### 3.2 Non-functional requirements

- LAN-first: target sub-10 ms round trips; snapshots at 20 Hz over UDP are more than
  enough; frame interpolation smooths the rest. No prediction required on a LAN
  (see §6 risks), but the architecture must not preclude it.
- Simplicity over throughput: this is a "we trust each other on the LAN" model.
  **No anti-cheat.** The host is authoritative by default because it already runs
  the only copy of the simulation.
- Low CPU/RAM cost: remote players are cheap (`AIControls` sized input + snapshot
  copies); no per-frame allocations (byte buffers reused).
- All netcode must be testable headless (loopback) so it fits the existing
  `zombie_tests` discipline; no manual-only verification.
- Deterministic world gen across machines (verify; else transfer tiles once at join).

### 3.3 Explicitly out of scope (v1)

- Internet / NAT traversal / matchmaking (no STUN/TURN).
- Cross-platform server binaries (dev on Linux only for now).
- Spectators, VoIP, save/replay of online sessions, anti-cheat, encrypted transport.
- Client-side prediction and rollback (added later only if LAN assumptions break).

---

## 4. Proposed architecture

### 4.1 Model: client-server with in-process host ("listen server")

```
┌─────────────────────────── PC A (HOST) ───────────────────────────┐
│  Game loop (existing)                                              │
│  step_frame() → update() → systems → render                       │
│                                                                     │
│  Local player 1      👤 ← local keyboard/mouse = InputState        │
│  Local player N (bot/script OR host's own)                         │
│  Remote players       👥 ← NetServer listens (ENet)                │
│     each remote = a "driver" producing AIControls                  │
│     fed through the SAME ai_apply_controls()/input_inject_* path   │
│                                                                     │
│  Simulation (authoritative, unchanged pipeline)                    │
│  waves / zombies / items / cleanup                                 │
│                                                                     │
│  NetServer: 20 Hz → serialize entity snapshot → ENet send → peers  │
└─────────────────────────────────────────────────────────────────────┘
                                      │ UDP
                                      ▼
┌─────────────────────────── PC B (CLIENT) ─────────────────────────┐
│  Render-only (no world simulation)                                 │
│  NetClient:                                                         │
│    → inputs out:  move/aim/fire/weapon/shop → AIControls wire fmt   │
│    ← snapshots in: entity state mirror (interpolated)               │
│    ← events in: damage/kills/wave/points → HUD + sounds             │
│  Local player rendered from own predicted/latest snapshot           │
│  Camera/HUD identical to single player (local)                      │
└─────────────────────────────────────────────────────────────────────┘
```

- The host plays with **zero network latency** (its input goes straight into
  `InputState`; the loop is the same as single-player).
- Clients are thin: they run **no** tick of `update()` for the shared world — they
  only render the snapshot mirror. This makes divergence impossible (one sim in the
  room) and keeps the v1 netcode small.
- The host also treats remote players through the existing AI-injection hook:
  **net input arrives → converted to `AIControls` → `ai_apply_controls`.** The bulk
  of gameplay code never learns that players 2-4 are human.

### 4.2 Why not P2P or lockstep?

- **P2P** complicates who owns the waves/zombies/items and doubles the code
  (every peer both sends inputs and receives them); a listen server is the standard
  co-op shape and matches the existing single-process simulation.
- **Deterministic lockstep** (all run the sim, exchange inputs) has stretch appeal
  (tiny bandwidth, perfect entity sync) but requires byte-exact determinism across
  compilers/FPUs for the *whole* pipeline (float math, RNG, system call order) and a
  synchronized tick, plus pause/herding. The codebase is not engineered for that, and
  the payoff on a LAN with one authoritative host is not worth the risk in v1.
  Revisit only if bandwidth/CPU ever matters.

---

## 5. Networking stack

**Recommendation: ENet** (C library, UDP). Rationale:

- The user's guidance referenced "commonly used in C++ for networking." In the
  C/C++ game world that is, in practice, **ENet** (used by many C and C++ games),
  RakNet/​Slikenet (C++), or SDL_net (TCP-ish). For this **C** codebase:
  - **ENet** — C11, UDP with ordered/unordered reliable + unreliable channels,
    connection management, ping. Perfect fit: one lib, no C++ runtime, tiny. License
    is zlib-style. **Pick this.**
  - **SDL_net (SDL3)** — official SDL lib but TCP-centric; you'd hand-roll
    reliability/ordering for UDP. Not better than ENet for real-time.
  - **RakNet/Slikenet** — heavyweight C++, painful to link into a C build. No.
- Add via the existing `FetchContent` pattern in `CMakeLists.txt`
  (pin `GIT_TAG v1.3.18`, shallow clone, like SDL3):

```
FetchContent_Declare(enet GIT_REPOSITORY https://github.com/lsalzman/enet.git
                     GIT_TAG v1.3.18 GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(enet)
```

### Channels

| Channel | Traffic | Reliability |
|---------|---------|-------------|
| 0 | session control: hello/join/welcome, events, chat, pause, shop | reliable, ordered |
| 1 | snapshots, input acks | unreliable, sequenced (latest-wins) |

Input is small and frequent → put on channel 1 incoming to the host; the host reads
input at its own cadence regardless of packet order to stay in lockstep with the sim.

---

## 6. Replication strategy

### 6.1 Snapshots

20 Hz snapshot from host to all clients. Serialize only semantic (networked) state:

```
Header:  protocol_version, snapshot_seq, sim_time, wave state (number/active),
         kill count, per-client flags
Body:    for each alive entity that is networked:
           entity_id (u16), kind (u8), pos (f32x2),
           vel (f32x2, quantized), hp (f32), flags (u8), owner (u16, for bullets)
```

- Sprite/visual data is **not** sent; clients reconstruct the look from `kind`
  (deterministic and already single-source in the ECS: same factories both sides).
  This keeps snapshots tiny (`~30B * relevant entities`; bounded by 2048, typically
  a few hundred at most on LAN → comfortably under Ethernet MTU budgets).
- Entity ids are stable ECS array indices → client builds a `mirror[ECS_MAX_ENTITIES]`
  keyed by id with interpolation buffers (last two snapshots + 100-250 ms blend).
- Zombie kill cleanup uses `GE_ENTITY_DEATH` (channel 0) so clients can tidy the
  mirror instantly instead of waiting for the next snapshot.
- **No client prediction in v1** (LAN RTT < 10 ms ≈ one sim tick). Local player on a
  client is rendered from the incoming snapshot; on a LAN this is imperceptible. If
  wireless/wifi jitter shows up, add simple local input prediction for the own player
  only (scope guard).

### 6.2 Input

Client sends, at ≥30 Hz (and on change): move dir (u8 quantized), aim world pos or
screen pos (f32x2), and a button bitmask (fire held, weapon switch, shop buy
target). Host maps that to `AIControls` and, existing style, to each remote player's
`InputState`.

### 6.3 Events (channel 0)

The host relays a curated subset of `GE_*` for client UX: `GE_PLAYER_HEALTH`,
`GE_PLAYER_SHOT` (sound cue), `GE_DAMAGE`, `GE_KILL`, `GE_WAVE_START/END`,
`GE_ITEM_PICKUP`, `GE_POINTS`, `GE_SHOP_PURCHASE`, `GE_ENTITY_DEATH`. Clients only
read these; the host never trusts client-generated gameplay events.

---

## 7. Simulation changes required (host side)

| Area | File(s) | Change |
|------|---------|--------|
| Multiple players | `main.c` `create_player` | Track all player entities in a `players[]` table (id, name, color, inv, connection). |
| Per-player inventory | `main.c`, `update()` | `PlayerInventory` per player (already a struct); create per player. |
| Player systems iterate all players | `player_input.c`, `sword.c`, `grenades.c`, `rockets.c` | Replace `find_player()`/first-match with per-driver handling or a loop over `players[]` with that player's `InputState`. |
| Kill credit by owner | `systems/cleanup` + `bullets.c` | When a zombie dies, attribute to `CBulletTag.owner`'s inventory (map entity→inv). Today `system_cleanup` credits one `inv`. |
| Zombie targeting | `zombie_ai.c` | Nearest living player, weighted toward the attacker; keep horde pressure spread. |
| Item pickup | `items.c` `items_check_pickup` | Trigger per player entity (already parameterized by entity; call for each player). |
| Spawns | `waves.c` | Spawn zombies around players' positions; server-side only. Multi-player scaling knobs. |
| Respawn / game over | `main.c` | Decide: per-player death screen + respawn after N seconds, or team game-over when all remain dead. (Open decision D2.) |
| Pause | `main.c` | Host pauses; broadcast state to pause all clients (clients are render-only so they just show the overlay). |
| Shop | `menu.c`/`shop` | Per-player; host validates and applies purchases; broadcast only the resulting calls to HUD. |
| Net hooks | `main.c` `step_frame()` | After AI driver step, socket-driver step: tx inputs, rx snapshot/events, rebuild mirror/inputs. |

Remote "drivers" slot in next to `AIDriver` as a peer component:
`NetServer` (host) and `NetClient` consume/produce `AIControls`; both reuse
`ai_apply_controls`.

---

## 8. World sync

`world_init()` is procedural and tile-fixed. Two sub-requirements:

1. **Verify determinism.** Confirm `world_init()` produces identical tile layouts +
   spawn points for the same inputs across processes/machines (it currently doesn't
   consume `rand()` — needs a quick test harness assertion). If not deterministic,
   the host sends a compact tile-format string once during the join handshake.
2. **Version it.** Bundle a `WORLD_GEN_VERSION` constant in the handshake so a
   mismatch = clean "version mismatch" instead of desync.

No mid-game world mutation is planned → nothing further to send.

---

## 9. Build / menu / UI changes

- `CMakeLists.txt`: add `enet` FetchContent + link to `zombie_core`; keep everything
  else untouched.
- `src/ui/menu.c`: main menu gains "Host Game" and "Join Game" entries →
  `GAME_STATE_CONNECTING`/`GAME_STATE_LOBBY` additions to the state machine in
  `main.c`. Simple text-field entry for name/IP:port (reuse existing font/text
  drawing; no textbox widget exists yet — implement a minimal one).
- Pause menu: player list (name/color/ping/HP) + "Restart host" + "Disconnect".
- HUD: remote player name tags + color-coded health bars (render.c already draws
  generic health bars; just gate on remote vs self).
- Overlay for connection state / "waiting for host...".

---

## 10. Headless dedicated server (stretch, low cost)

`--headless` already runs the entire sim without a renderer. Expose:

```
--host --headless            # dedicated LAN server, no window
--host [--port=5123] [--max-players=4]
```

Because the host loop is the game loop, a headless host is literally the same binary
with `game.headless=true`. The only additions: skip the AI/local-input driving for
slots not occupied by a local player, and skip `render()`. Cheap and useful for
testing/CI and for people without the release build.

---

## 11. Testing & QA plan

Reuse the existing headless discipline:

1. **Unit (zombie_tests additions, `tests/tests.c`):**
   - Serialization/codec round-trips (snapshot, input, event frames) — byte-exact.
   - World-gen determinism assertion (same seed → identical tiles + spawns).
   - Kill-credit attribution (owner inventory gets points; map lookup for N players).
   - Multi-player player-system iteration (2+ players move/shoot independently).
2. **Loopback integration:** start two `Game` instances in one process (or a host +
   a bot-driven client via `--ai=bot`/script) over `127.0.0.1`; assert: join, spawn
   both, one shoots → other sees the shot, kill credits correct, disconnect cleans
   up. Run this as a normal headless test case.
3. **Scripted remote:** reuse `--ai=script`/bot as stand-ins for the missing humans —
   the bot emits `AIControls`, exactly the shape netinput uses. This gives an
   automated "second player" for waves/pickup/shop scenarios.
4. **Manual playtest prompts** (matching the existing `playtesting_prompts/`
   format): host+join on two machines, latency/touchpad regression, disconnects,
   4-player pressure, shop/pause interactions.

Definition of done for each milestone includes "green `ctest` + a scripted co-op
scenario logged in `game_events.log`."

---

## 12. Milestones

| Milestone | Scope | Exit criteria |
|-----------|-------|---------------|
| P0 | World determinism check; refactor plumbing for multiple players (per-player inventory, kill credit, iterate-all systems); add `players[]` table. | `ctest` green; 2 players move/shoot correctly in a scripted headless run; existing single-player behavior byte-identical. |
| P1 | Fetch ENet; `NetHeader`/codec; `NetClient`/`NetServer` skeletons; host/join menu + `CONNECTING/LOBBY` states; handshake (version, name, seed). | Two processes join over loopback; names/ping shown; version mismatch handled. |
| P2 | Snapshot replication (host→client mirror, interpolation, death events) + remote player visibility; input path → `AIControls`. | Remote player visibly renders/aims/fires from scripts on both loopback and a second machine. |
| P3 | Full gameplay parity: shared waves, items, per-player shop/points, kills credit owners, difficulty scaling, respawn policy. | Two-player scripted scenario reaches wave 3 with correct individual points; events relay correctly. |
| P4 | Polish: hostile disconnect handling, pause broadcast, chat (R12), player list, game-over/team resolution, dedicated `--host --headless`. | Manual playtest prompts pass on two real machines; headless host + scripted client passes in CI. |

Estimated rough scope: P0-P1 as "foundation" (~biggest unseen risk: multiple-player
refactor and world determinism), P2-P3 as "core netcode + gameplay parity", P4 as
"hardening". Each milestone is mergeable to `main` behind a `NET_*` compile/runtime
flag so single-player regressions are caught early.

---

## 13. Open decisions (need your input)

| ID | Decision | Default proposal |
|----|----------|------------------|
| D1 | Max players | 4 (configurable 1-4). |
| D2 | Death/respawn | Individual death → spectate/respawn after 5 s; team loses only if all players are dead simultaneously. |
| D3 | Difficulty scaling | `zombies_per_wave += (players-1) * per_player_bonus`; slightly faster spawn interval. Tune later. |
| D4 | Authoritativeness | Host authoritative (trusted LAN). No client prediction in v1. |
| D5 | Dedicated headless server | Include as stretch within P4; not required for v1 release. |
| D6 | Chat (R12) | Include as nice-to-have; cut if it delays core gameplay parity. |
| D7 | World transfer | Verify determinism; else send tiles at join. Decide during P0. |
| D8 | Save/replay of online sessions (event bus already logs) | Out of scope v1; powerful given `game_events.log`, revisit later. |

---

## 14. Risks

| Risk | Impact | Mitigation |
|------|--------|------------|
| Multi-player refactor touches every player system | High regression risk for single-player | Land P0 behind a compile flag; keep single-player path byte-identical; extend `zombie_tests`. |
| World-gen nondeterminism / version drift | Desyncs visible client-side | P0 determinism test; handshake version constant; tile transfer fallback. |
| Wi-fi jitter on laptop (the dev box) | Interpolation artifacts | 20 Hz + generous interpolation; low-cost own-player prediction as follow-up. |
| Pause/shop interplay (freeze-all vs per-player) | Confusing UX | Follow existing pause semantics: host pause freezes everyone; mirror it on clients. |
| ENet fetch/build friction in CI | Build breaks | Pin tag, shallow clone, same pattern as SDL3; document in runbook. |