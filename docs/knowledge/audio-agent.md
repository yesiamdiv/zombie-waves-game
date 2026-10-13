# Audio agent — orientation

<!-- Project knowledge (Intercom-AI). Agents append entries; edit freely. -->

## 2026-10-13 14:20 UTC - note by assets: MIGRATED from docs/AUDIO_AGENT.md (owner D7). Start-here for the audio agent.

You are the **audio agent**. This topic is your orientation — read it top to
bottom before opening an editor. The plan of record is [audio-plan]; process
rules are [agent-rules].

**Mission, one line.** Give the game sound — weapons, hits, deaths, pickups,
waves, menu feedback — **in single-player, on the host, and on the client, from
one code path.** You start from nothing: no audio code, no sound files, no
library, no `vendor/` directory. You are building the system, not extending one.

**Phase (owner D4, 2026-10-13):** still in the PLANNING PHASE — nothing coded.

## Where you work

| | |
|---|---|
| Branch | `feature/audio` (not yet created; the owner cuts it) |
| Worktree | `worktrees/audio` (owner D5 — cut in-checkout from `main`, then rebased onto `feature/animation-assets` before landing) |
| Plan of record | [audio-plan] |
| Process rules | [agent-rules] |

Only ever work in your own worktree. Never edit `main`, `worktrees/assets`,
`worktrees/multiplayer`, or another agent's branch ([worktree-layout]).

## Read this first, in this order

1. `AGENTS.md` (root) — standing instructions, worktree discipline, the gate.
   Binding always.
2. `docs/SYSTEM_DESIGN_RULES.md` — mandatory. §1 (the host is the only authority
   for anything a player can see), §2 (classify every new value), §4 (**no gate
   here can hear anything**), §5 (no `rand()` in a replicated path), §8 (update
   order is a contract).
3. `docs/VERSIONING.md` — mandatory. §2, "one concern, one version" — the clause
   [audio-plan] §9 was argued from; **§9 is decided:** audio moves no number at
   all ([versioning-decisions]).
4. `docs/EVENT_FORMAT.md` — the event types you trigger from, and each payload.
5. `docs/I5_FIX.md` — skim §3-§4: the project's canonical example of a client
   inventing state the host already knew. Your design must not repeat it.
6. [audio-plan] — your plan. §2 verified ground truth, §4 **closed as B37**
   (gunfire is relayed — read it so you never touch the relay), §5 three wire
   limits to design around.
7. `docs/EXTENDING.md` — how systems are added and where they run.

## Ground truth you must not re-derive

- **There is no audio anywhere.** `grep -ri 'SDL_Audio\|miniaudio\|Mix_' src/`
  returns nothing. `assets/` has only `maps/` and `textures/`. The build links
  SDL3, SDL3_ttf, SDL3_image, enet — no audio library.
- **The trigger source is the event bus**, `g_events`, a process-global ring of
  `GameEvent` (`src/events/event_bus.h`, 8192 entries), drained to disk once per
  frame.
- **The client already receives the same events.** The host relays a curated
  subset (`NET_PKT_EVENTS`) and `net_client.c` re-emits them onto the same bus.
  One listener works on both peers — authority comes free from the existing
  relay.
- **Your reading window is fixed and narrow:**
  ```
  relay_net_events();        // main.c:1503
  AUDIO_TICK;                // <-- you go here
  event_bus_flush(g_events); // main.c:1504 — ring destroyed
  ```
  Earlier misses `GE_KILL`; later reads an empty ring. Copy the pattern
  `relay_net_events()` already uses: walk `g_events->ring` with a `sid` watermark.
- **`GE_PLAYER_SHOT` is now relayed — by `[net]`, not by you.** Landed as **B37**;
  reasoning and the four reasons it needs no wire bump are in [audio-plan] §4.
  **You consume the event; you never edit the relay list.** If it is missing when
  you reach Phase D, chase `[net]` over Intercom (`intercom_ask`) — do not add the
  line yourself.
- **`ia`/`ib` do not cross the relay**, and `x,y` is only present where the
  emitter passed it. [audio-plan] §5.
- Version numbers: read the current values from `src/net/net.h` and
  `VERSIONING.md` §2 — never from prose ([versioning-decisions]).

## Files

**Yours:** `src/audio/**` (new), `vendor/miniaudio.*` (new), `tools/gen_audio.py`
(new), `assets/audio/**` (new).

**Shared, keep to one line each:** `src/main.c` (init / update / shutdown /
`AUDIO_TICK`), `src/systems/systems.h`, `CMakeLists.txt`.

**Not yours — read-only:** `src/systems/render.c` contains `system_cleanup`,
which emits `GE_KILL` and `GE_ENTITY_DEATH` — your two most important triggers —
and calls `rand()`, making it part of the determinism gate. It belongs to the
animations agent and is read-only for *both* of you. If something there affects
your triggers, raise it in [audio-plan] rather than editing it.

**Never:** any of `src/net/**` — unlike the animations agent, you have no art ids
to append — and `playtesting_prompts/**`. If you think a wire change is needed,
read `I5_FIX.md` §7 and raise it in [audio-plan] instead of making one.

Full matrix: [agent-rules] §2.

## Your first task

**Spike, not code.** Fill in [audio-plan] §10:

1. **Vendored miniaudio, sound out of the speakers** — vendor the header, link
   it, play one sound, and prove it fails *soft*: no device, missing file, and
   `--headless` all run silently with no assert and no per-frame allocation.
   Decide which file vendoring happens in, the licence, and the build cost.
2. **Trigger audit** — one row per intended sound: emitted where, relayed?,
   carries `x,y`?, carries `ia/ib`?, works on a client? Read the emit sites;
   `EVENT_FORMAT.md` records intent, the code records what is actually passed.
3. **Double-fire audit** — count emissions per event type on a host and on a
   client for one 25-second run. A doubled kill sound is undetectable to every
   gate and much cheaper to rule out now ([client-cannot-double-fire-events]).
4. ~~Settle §4 and §9~~ — **both already decided** (2026-10-06): gunfire arrives
   via `[net]` relaying `GE_PLAYER_SHOT` ([audio-plan] §4), and audio does **not**
   move `NET_ASSET_VERSION` ([audio-plan] §9). Re-read both before you code, but
   do not re-open them.
5. **Is relayed gunfire latency audible?** The one question §4 deliberately left
   open — if it is, `[net]`'s option (c) becomes worth building, deduped by bullet
   entity id. Answer it while the two windows are already up.

Then commit the verdicts **before** writing implementation code ([agent-rules] §8).

## Definition of done

```
cmake --build build                    # zero warnings, -Werror
ctest --test-dir build                 # 3/3
./build/zombie_tests                   # green
./build/open_world_zombie_waves --headless --ai=bot --run-seconds=25 \
    --events=a.log --seed=42           # x2, diff-empty
```

Wave 1 must read exactly what [gate-baseline] records (the single declaration of
every gate number — cite it, never copy it), and a headless run must open **no**
audio device. Plus: `docs/FEATURES.md` **F16** written, and
`docs/PENDING_VERIFICATION.md` says **what a human must listen for, and on which
peer.**

## The ways this fails

- **The second window is silent.** Every gate passes, every test is green, and a
  client hears nothing — because triggers were only ever emitted host-side. This
  is your I5 equivalent, and [audio-plan] §4 is the known instance.
- **Sounds fire twice on a client** — once locally, once relayed. Only a human
  hears it.
- **A sound that needs `ia` works on the host and is silent on the client**,
  because the relay drops it.
- **The tick lands after `event_bus_flush()`** — reads an empty ring, plays
  nothing, passes every test.
- **Audio blocks or allocates when there is no device.** Headless and CI must
  stay allocation-free and side-effect-free.
- **`rand()` for sound variation** — forbidden anywhere, ever
  (`SYSTEM_DESIGN_RULES.md` §5).
- **You bump `NET_ASSET_VERSION` anyway.** Decided: audio does not, and no
  `NET_AUDIO_VERSION` exists ([audio-plan] §9, [versioning-decisions]). The
  animation agent bumps that number for art ids; your bump would collide for no
  reason.
- **You write `fixed` instead of `unproven`.** No gate can hear your work; a human
  listening to two windows is the proof.

## How to work

One commit per unit. Keep the todo list current. Record findings in
`docs/BUGS.md` / `docs/PROGRESS.md` **after** the commit exists — chat is not the
record. Rebase onto `main` at the end of every phase, and onto
`feature/animation-assets` after it merges (you are second; [audio-plan] §12,
[agent-rules] §9). Report in PM/standup form: planned / done / next / blocked.
