# Audio — plan

<!-- Project knowledge (Intercom-AI). Agents append entries; edit freely. -->

## 2026-10-13 14:20 UTC - note by assets: MIGRATED from docs/AUDIO_PLAN.md (owner D7). Spike-first plan; nothing implemented.

Status: PLANNED — spike-first. Nothing here is implemented. Owner: the audio
agent ([audio-agent]). Branch `feature/audio` in `worktrees/audio` (owner D5);
the owner cuts it, once `main` is clean and the gate is green. Process rules:
[agent-rules].

**Scope (owner D4, 2026-10-13):** still planned, in the PLANNING PHASE — nothing
coded; starts only after the other blockers clear and net+assets finish current
work; further changes may still come. **`GE_EXPLOSION` is GO (D3)** and its
single-player re-baseline rides this work (§5.3).

## 1. Goal and non-goals

**Goal.** The game has sound — weapons, hits, deaths, pickups, wave starts, menu
feedback — in single-player, on the host, and on the client, from one code path.

**Non-goals.** No music system. No voice chat. No mixing UI beyond a master
volume. No audio that affects gameplay, and none that blocks, allocates per
frame, or asserts with no device.

**The hard requirement:** no automated gate in this repo can hear anything
(`SYSTEM_DESIGN_RULES.md` §4). A passing suite is not evidence a sound plays.
Everything ships as `unproven` until a human has heard it; the listen-for list
lives in `PENDING_VERIFICATION.md`.

## 2. Ground truth — verified, not assumed

- **There is no audio at all.** No sound code, no audio files, no `vendor/`
  directory, nothing referencing `SDL_Audio`, `SDL_mixer` or `miniaudio` in
  `src/`. Greenfield.
- Build links `SDL3::SDL3` (SDL3 ships an audio API), `SDL3_ttf`, `SDL3_image`
  and `enet`. `CMakeLists.txt` has no audio library today.
- `assets/` has `maps/` and `textures/` only — no `assets/audio/`.
- **The event bus is the trigger source and already works identically on both
  peers.** `g_events` is a process-global ring of `GameEvent`
  (`src/events/event_bus.h`, 8192 entries), written by the simulation and drained
  to disk once per frame.
- **The host already consumes that ring mid-frame** — the template to copy:
  `relay_net_events()` walks `g_events->ring` with a `sid` watermark and
  broadcasts a curated subset, and runs *before* the drain (`relay_net_events()`
  → `event_bus_flush(g_events)`, `main.c:1503/1504`).
- **The client re-emits relayed events onto the same bus** (`net_client.c`,
  packet `NET_PKT_EVENTS`). So audio does not need to know host from client: it
  reads one ring either way, and authority comes free from the existing relay.

That is the whole architecture, and it satisfies `SYSTEM_DESIGN_RULES.md` §1 with
no new wire field: on a host the events originate locally, on a client they were
relayed from the host. Audio consumes host state on both.

## 3. Where audio runs

The window is fixed and it is **not inside `update()`** ([agent-rules] §4):

```
relay_net_events();        // main.c:1503
AUDIO_TICK;                // <-- insert here
event_bus_flush(g_events); // main.c:1504
```

Later reads an emptied ring; earlier misses `GE_KILL`, emitted from
`system_cleanup` in `src/systems/render.c` — a file audio does not own ([agent-rules]
§2). Audio also inits/updates/shuts down in `src/main.c` (shared) — keep those
insertions to a stub.

## 4. Gunfire has no trigger — DECIDED and LANDED as B37

The host relays a **curated** subset (`relay_net_events()`):

| Relayed | **Not** relayed |
|---|---|
| `GE_ENTITY_DEATH`, `GE_WAVE_START`, `GE_PLAYER_HEALTH`, `GE_KILL`, `GE_POINTS`, `GE_DAMAGE`, `GE_ITEM_PICKUP`, `GE_HOST_PAUSE` | `GE_PLAYER_SHOT`, `GE_ENTITY_SPAWN`, `GE_WAVE_END`, `GE_POSITION_SAMPLE`, `GE_INPUT`, `GE_SHOP_PURCHASE` |

`GE_PLAYER_SHOT` is emitted only host-side (`player_input.c` bullets,
`grenades.c`, `rockets.c`) and was **not in the relay list** — so on the second
window no gunshot sound could exist: absent, for every player. The audio
equivalent of I5, invisible to every gate.

**Decided and landed 2026-10-06 as B37 — option (a), by `[net]`.** Decision-
before-code: `docs(bugs): record B37` → `fix(net): relay GE_PLAYER_SHOT so a
client can hear gunshots (B37)` → SHA backfilled. `docs/BUGS.md` B37 carries the
row. `[net]` owns `relay_net_events()`; **audio is not to touch the relay list**
(§8).

Verified, not assumed: the client event log went 0 → 96 `PLAYER_SHOT` (host
emitted 101; the gap = shots before connect), each carrying origin `x,y`,
direction `a,b`, `kind=bullet`; SP runs stayed byte-identical (the count in
[gate-baseline]), proving the widened filter cannot reach single-player.
Re-check in Phase D — an accepted claim is still a claim.

Why (a) is safe (so audio does not re-litigate it):

- **No `NET_WIRE_VERSION` bump.** `GE_PLAYER_SHOT` already exists in the enum and
  the relay struct already carries `type, kind, entity, x, y, a, b`. Widening a
  *filter* is not a layout change, and the wire version guards layout only.
- **Old peers survive it** — a client re-emits a known type harmlessly, just
  plays no sound.
- **The payload already suits a sound** — the three emitters pass `x,y` (spawn
  origin), `a,b` (direction), `kind` (weapon class). One relayed event covers
  every shooter with origin and weapon.
- **SP is untouched by construction** — `relay_net_events()` opens with
  `if (!game.net_host_mode || !g_events) return;`, so in single-player the new
  type is never considered.

**Option (c) — deferred, not rejected.** Relay gives everyone's shots including
your own, at RTT latency. Build (c) only if that latency proves audible, and
dedupe by bullet entity id (the relayed copy carries the host's bullet entity).
Make "is relayed latency audible" a spike question, not a plan assumption.
**Option (b) rejected** — re-deriving a shot from the mirror is the
second-opinion pattern (`I5_FIX.md` §3, `SYSTEM_DESIGN_RULES.md` §1).

**Phase D job: consume the relayed event, never change the relay.** If
`GE_PLAYER_SHOT` is missing when you get there, chase `[net]` over Intercom
(`intercom_ask`); do not add the line yourself.

## 5. Three data limits the wire and the gate impose

Facts about the existing relay; design around them, do not discover them
mid-sprint. All three are spike-check items (§6), none justifies a packet-format
change.

1. **`ia`/`ib` do not cross the relay.** `net_encode_events` writes
   `type, kind, id, x, y, a, b` — the two integer payload fields are dropped.
   `GE_DAMAGE` carries its source entity in `ia`; `GE_ITEM_PICKUP`/
   `GE_SHOP_PURCHASE` use integer payloads. A sound that wants "who did this"
   must work from `id`/`x,y`/`a,b`, or it works on the host and is silent on the
   client.
2. **Position is present only where the emitter passed it.** `EVENT_FORMAT.md`
   documents `x,y` present "only if non-zero". `GE_KILL` passes the death
   position; other types may not. Every spatialised sound needs a position, so
   the spike must check each candidate trigger for a real `x,y` on *both* peers.
3. **A brand-new event type re-baselines the single-player gate.**
   `GE_EXPLOSION` (owner D3, GO) does not exist today; emitting it grows
   `event_type_names[GE_COUNT]`, and `event_bus.c` logs *every* event, so an SP
   run's events log changes — and that log is the byte/line reference the whole
   gate rests on ([gate-baseline]). So the audio work must **re-baseline the gate
   deliberately**, recorded in [gate-baseline] *before* the commit, never folded
   in silently, and **jointly re-verified with `[net]`**. `GE_WAVE_END` (B39) is
   the opposite — type exists, relaying is free and changes no SP output. A new
   event costs a gate re-baseline; a relayed existing one does not.

## 6. Spikes

### Spike A — vendored miniaudio, sound out of the speakers

Vendor one header, prove it links here, play one sound from a tiny test path,
and prove it fails *soft*: no device (headless/CI/container) → init fails, the
game runs, zero audio calls, no assert, no per-frame allocation; missing file →
logged, skipped, no crash; `--headless` never opens a device. Decide and write
down: **which file vendoring happens in** (`vendor/` does not exist yet), **how
it is licensed**, **whether the amalgamated build time is acceptable**.

### Spike B — the trigger audit

One row per intended sound; read the emit sites, not `EVENT_FORMAT.md` (the doc
records intent, the code records what is passed):

| Trigger | Emitted where | Relayed? | Carries `x,y`? | Carries `ia/ib`? | Works on a client? |
|---|---|---|---|---|---|

Row one is `GE_PLAYER_SHOT` — it comes out `No`, which is §4.

### Spike C — the double-fire audit

On a client, are events **arriving twice** (locally generated and relayed)? A
doubled kill sound is the kind of defect no gate catches. Count emissions per
type on a host and on a client for one 25-second run and record both here.

## 7. Phases

| Phase | Deliverable | Acceptance |
|---|---|---|
| **A. Spike** | §10 verdicts filled in | miniaudio linked; trigger audit complete; double-fire ruled out; relayed-latency answered; verdict written before implementation. **§4 already decided — do not re-open.** |
| **B. Foundations** | `src/audio/`, vendor header, init/update/shutdown in `main.c`, `AUDIO_TICK` in the §3 window | headless path allocation-free and silent; build clean; gate green |
| **C. Content** | `tools/gen_audio.py`, `assets/audio/**`, the sounds | generator deterministic; regenerating leaves `git status` clean |
| **D. Multiplayer** | triggers work from the relayed bus; §4 option (a) consumed | client hears kills, damage, pickups, wave start **and gunfire** (relayed by `[net]`); audio adds **no** wire field and edits **no** relay list |
| **E. Verification** | human listens, two windows | `PENDING_VERIFICATION.md` lists what to listen for, on which peer |

## 8. Cross-dependencies

- **`system_cleanup` in `src/systems/render.c` emits `GE_KILL` and
  `GE_ENTITY_DEATH`** — audio's two most important triggers, in someone else's
  file, read-only for both agents because it consumes the seeded RNG
  ([agent-rules] §2). If the animation plan changes when deaths are detected,
  audio inherits it silently.
- **`src/main.c` is shared** — init, shutdown, the `AUDIO_TICK` insertion. Keep
  each insertion to a line or two. The §4 relay edit is closed as B37 — not
  yours.
- **Double-fire was proven impossible, not merely unlikely** (`[net]`): a
  render-only client breaks out of `update()` before the systems loop, so every
  relayed type is emitted from a path the client never reaches, while the only
  types a client writes to its own bus (`GE_INPUT`, `GE_POSITION_SAMPLE`) are not
  relayed — disjoint sets. **Still close it with a counting log in the spike:** a
  log reading `1` beats an argument, and a `2` is a netcode bug, not an audio one.
  Detail: [client-cannot-double-fire-events].
- **`NET_ASSET_VERSION`** — audio does not move it (§9). If it moves, that is the
  animation agent's, not a conflict for you.
- Merge-order argument in [animation-plan] §9; audio rebases onto animation.

## 9. Versioning — DECIDED 2026-10-06

**Audio does not move `NET_ASSET_VERSION`, and no `NET_AUDIO_VERSION` exists.**
Agreed with `[net]`; recorded in [versioning-decisions] and [agent-rules] §3.

Reasoning, so it survives the channel being ephemeral:

- `VERSIONING.md` §2, *"one concern, one version"* — the concern
  `NET_ASSET_VERSION` guards is *"two peers ship the same art"*. Sound is not a
  picture: two peers with different audio agree on the world, cannot desync, and
  render identically.
- `[net]`'s sharper version: `NET_ASSET_VERSION` **is** read — in the HELLO
  handshake, producing `NET_REJECT_ASSET`. A number no code path consults is
  worse than no number, because it implies a check that is not happening.
- So audio gets **no bump and no new number**. If it later ships something read
  off the wire (sample rate, bank layout, a handshake field), it earns its own
  number then. Do not pre-create it.

**Asymmetry with animation**, which *does* bump `NET_ASSET_VERSION`: an art id a
peer cannot resolve silently skips a frame; an audio file a peer lacks merely
plays nothing, and only one of those is a defect ([versioning-decisions]).

## 10. Spike verdicts

*Empty until the spikes run. Do not start implementation with this blank.*

- **§4 — gunfire reaches a client:** DECIDED 2026-10-06, option (a), by `[net]`.
  Not a spike verdict; do not re-open.
- **Trigger audit:** (will be filled in)
- **Double-fire audit (host / client counts):** (will be filled in)
- **Vendoring: file path, licence, build cost:** (will be filled in)
- **Versioning — does audio move `NET_ASSET_VERSION`:** DECIDED 2026-10-06 — no
  bump, no `NET_AUDIO_VERSION`. Not a spike verdict; do not re-open.

## 11. Acceptance for the feature as a whole

- [ ] Gate green: build clean under `-Werror`, `ctest` 3/3, `zombie_tests` green
- [ ] Two same-seed SP runs `diff`-empty; wave 1 exactly the string in [gate-baseline]
- [ ] `--headless` opens no device, allocates nothing per frame, logs no error
- [ ] Missing audio file and missing device both degrade to silence, not a crash or assert
- [ ] Version decision (§9) applied and recorded
- [ ] `docs/FEATURES.md` **F16** written; `PENDING_VERIFICATION.md` updated with
      **what a human must listen for, on which peer**
- [ ] **A human has heard it, including on a client.** Until then the status line
      reads `unproven`, never `done`

## 12. Why audio merges second

Argument in [animation-plan] §9. Animation owns the render path, the texture
generator and a probable `net_art.c` append — the wide blast radius. Audio owns
new files almost entirely, plus three one-line insertions in shared files.
Merging audio first would not remove those insertions; it would put a second set
of `main.c` edits on top of a tree about to move. Animation merges, audio rebases
onto the settled render path, then audio merges. Reversing merge order requires
arguing down both this section and [animation-plan] §9 — it is not a preference.
