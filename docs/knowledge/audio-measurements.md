# Audio — spike measurements (raw numbers)

<!-- Project knowledge (Intercom-AI). Agents append entries; edit freely. -->

## 2026-10-22 18:30 UTC - note by audio: Spike A/B/C/D measured on `feature/audio`

**Status: MEASURED.** The decisions live in [audio-plan] §10; this file is the
raw data and the method, so a verdict can be re-checked instead of trusted.
Owner: the audio agent ([audio-agent]). Branch `feature/audio` in
`worktrees/audio`.

Everything below was measured on the machine the gate runs on, with the
project's own compiler flags (`-O3 -DNDEBUG -Wall -Wextra -Werror`). Scratch
probes lived in `/tmp/opencode/audiospike/` and are **not** part of the repo;
the co-op logs live under `worktrees/audio/build/logs/` (ignored by git).

### Method — proving a sound reached the speakers

The machine has real audio: PipeWire sink
`alsa_output.pci-0000_04_00.6.analog-stereo` (monitor serial `58`), PulseAudio
socket `/run/user/1000/pulse/native`. SDL3 opened driver `pulseaudio`; miniaudio
opened backend `PulseAudio` at 44100 Hz / 2 ch.

**Raw RMS is useless here** — the desktop was playing its own audio, so the
silence floor is high and non-stationary. The probe therefore plays a
**12 kHz tone**, which the ambient desktop audio does not contain, and the sink
is captured with:

```sh
pw-record --target 58 --rate 48000 --channels 2 --format s16 out.wav   # positional filename, NOT -o
```

and the tone is detected in the capture with a Goertzel filter. Measured peak
levels of the 12 kHz partial: **silence control -46.6 dBFS, SDL3 -34.0 dBFS,
miniaudio -29.3 dBFS**, each peak inside its own playback window. That is the
proof that both backends put sound on the speakers — not that they opened a
device, which is a much weaker claim.

## Spike A — backend: SDL3 audio API vs vendored miniaudio 0.11.25

miniaudio was fetched as the single-header amalgamation
(`https://raw.githubusercontent.com/mackron/miniaudio/master/miniaudio.h`,
**4,108,168 bytes**, `MA_VERSION 0.11.25`).

| Measure | SDL3 audio | miniaudio (vendored) |
|---|---|---|
| Source added to the repo | none | one 4,108,168 B header |
| New translation units | 0 | 1 (amalgamated) |
| Compile time — probe TU | **0.11 s** (maxrss 36,616 KB) | 13.91 s (maxrss 346,640 KB) |
| Compile time — project TU, `-O3 -DNDEBUG -Wall -Wextra -Werror` | 0 s (no new TU) | **19.27 s** (maxrss 362,780 KB), warning-clean |
| Same TU at `-O1` / `-O2` | – | 6.64 s / 13.89 s |
| Binary size delta vs empty main | **0 B** (SDL3 is already linked) | 884,008 B untrimmed; **646,128 B** with the Linux/WAV-only trim |
| New link dependencies | none | `-ldl -lpthread` (`-lm`) |
| Licence to vendor and track | none (SDL3 already in the tree) | miniaudio's, must ship with the file |

The SDL3 API used by the probe is all already exported by `libSDL3.so`:
`SDL_OpenAudioDeviceStream(devid, spec, callback, userdata)`,
`SDL_ResumeAudioStreamDevice`, `SDL_LoadWAV` (core, not `SDL3_image`).

### Spike A's three questions, answered

- **Which file vendoring happens in** — `vendor/` does not exist yet. **With the
  SDL3 decision, nothing is vendored.** If the decision is ever reversed, the
  file is `vendor/miniaudio.h` (single header, no CMake `FetchContent` entry, no
  build-system change).
- **How it is licensed** — miniaudio ships public-domain / MIT-0 dual
  licensed. **Not needed with the SDL3 decision** (SDL3 is already vendored
  and its licence already tracked).
- **Is the amalgamated build time acceptable** — **no.** 19.27 s for one
  translation unit at `-O3`, against a project whose whole build is about a
  second and whose gate is run many times a day. `-O1` still costs 6.64 s.

### Soft-fail, both backends (probe level)

| Failure | Result |
|---|---|
| SDL3, no device (`SDL_AUDIODRIVER=nosuchdriver`) | `PROBE sdl_init_failed=Audio target 'nosuchdriver' not available` + `RESULT soft_fail_no_device=1`, **exit 0** |
| SDL3, missing file | `PROBE load_failed=Couldn't open …: No such file or directory` + `RESULT soft_fail_missing_file=1`, **exit 0** |
| miniaudio, missing file | `PROBE load_failed=-7 path=…` + `RESULT soft_fail_missing_file=1`, **exit 0** |
| explicit "no device" mode | `RESULT soft_fail_ok=1`, **exit 0** |

No assert, no abort, no crash, and the process exit code stays 0 in every case.
**Game level is not yet proven**: `--headless` must never open a device, and
that is a Phase B acceptance item, not a spike result.

## Spike B — the trigger audit

The audit table itself lives in [audio-plan] §6 (one row per intended sound).
The findings that changed a design decision:

1. **`GE_DAMAGE` for the player carries the attacker id in `ib` — and `ib` does
   not cross the relay.** `src/systems/zombie_ai.c:74` emits
   `event_emit(g_events, GE_DAMAGE, target, GEK_PLAYER, x, y, dmg, hp_after,
   prev_hp, zombie)`. On the client the attacker id arrives as 0. The client
   therefore knows *that* it was hit and *where*, but not *by what*.
2. **`GE_PLAYER_HEALTH` cannot say hurt-or-heal on the client.** `ia` is
   "hp before" and is dropped by the relay, so `a` (hp after) alone cannot
   distinguish a hit from a medkit. `a` (hp after) and `b` (hp max) do cross
   the wire — and so does the *previous* `GE_PLAYER_HEALTH` event, so a client
   can keep the last hp it saw and derive the delta locally. That is a
   deterministic derivation from host state, not a second opinion.
3. **There is no reload.** `grep -ri reload src/` returns nothing; the pistol's
   ammo is unlimited (`src/items/items.c`, "Pistol ammo is unlimited") and
   `NetPlayerState` (`src/net/net.h`, `NET_PLAYER_STATE_BYTES 23`,
   `NET_WIRE_VERSION 8`) has no reload field. There is no reload mechanic to
   sound, and no reload state to put on the wire.
4. **A rocket's impact has no event.** `src/systems/rockets.c:120` emits
   `GE_ENTITY_DEATH` with `kind=GEK_ROCKET` and `a=0, b=0` when the rocket
   *expires or leaves the world* — it is not the impact, and it is
   indistinguishable from a despawn. Only the **grenade** explosion
   (`src/systems/grenades.c:19`) carries a radius (`a`) and damage (`b`). A
   rocket-explosion sound on a client needs `GE_EXPLOSION` — net's NT2
   (`task_01a120dfba93258199e2c29a`).
5. **Every spatial sound has a position; every non-spatial one does not.**
   `GE_WAVE_START`, `GE_WAVE_END`, `GE_POINTS`, `GE_SHOP_PURCHASE` and
   `GE_HOST_PAUSE` all emit `x=0, y=0`. Those sounds must be 2D.
6. **`GE_SHOP_PURCHASE` and every `GE_ENTITY_SPAWN` are not relayed** — shop
   feedback and spawn sounds are host-only unless the relay list grows, which
   is net's file ([audio-plan] §4, §8).

## Spike C — double-fire audit

Two independent co-op runs, host and client both `--ai=bot`, events counted per
type from the `--events` logs. A doubled sound would show a client/host ratio
of **2.00**.

**Run 1 — headless, 25 s, seed 42** (`build/logs/coop_host.log`,
`build/logs/coop_client.log`):

| Event | Host | Client | Ratio |
|---|---|---|---|
| `PLAYER_SHOT` | 102 | 102 | **1.00** |
| `DAMAGE` | 42 | 42 | **1.00** |
| `KILL` | 10 | 10 | **1.00** |
| `POINTS` | 10 | 10 | **1.00** |
| `WAVE_START` | 1 | 1 | **1.00** |
| `WAVE_END` | 1 | 1 | **1.00** |
| `ENTITY_SPAWN` (not relayed) | 96 | 0 | 0.00 |
| `INPUT` (client-local) | 702 | 334 | – |
| `POSITION_SAMPLE` (client-local) | 1386 | 125 | – |

**Run 2 — windowed (two real windows, vsync), 18 s, seed 42**
(`build/logs/win_host.log`, `build/logs/win_client.log`):

| Event | Host | Client | Ratio |
|---|---|---|---|
| `PLAYER_SHOT` | 63 | 63 | **1.00** |
| `DAMAGE` | 25 | 25 | **1.00** |
| `KILL` | 5 | 5 | **1.00** |
| `POINTS` | 5 | 5 | **1.00** |
| `WAVE_START` | 1 | 1 | **1.00** |
| `ENTITY_SPAWN` (not relayed) | 50 | 0 | 0.00 |

**Verdict: double-fire is ruled out empirically**, in addition to the
structural argument in [client-cannot-double-fire-events] (a render-only client
breaks out of `update()` before the systems loop, and the two types a client
writes locally — `GE_INPUT`, `GE_POSITION_SAMPLE` — are not relayed: disjoint
sets). Every relayed type arrives **exactly once** on the client, with matching
entity ids.

## Spike D — relayed latency (gunfire on the wire)

Method: temporary instrumentation (reverted after the measurement, tree
verified clean) printing `CLOCK_MONOTONIC` nanoseconds when the host relays an
event in `relay_net_events()` (`src/main.c`) and when the client receives it in
`NET_PKT_EVENTS` (`src/net/net_client.c:361`). `CLOCK_MONOTONIC` is a
system-wide clock, so the two processes share an epoch; events were matched
one-for-one in order, and the bullet entity ids were checked to match.

**Unthrottled (both peers `--headless`, running as fast as the fixed timestep
allows), 102 matched `GE_PLAYER_SHOT` events:**

- entity-id mismatches: **0** (no drops, no duplicates, no reordering)
- residual after removing the constant clock offset: **min -1.0 ms, median
  0.0 ms, max 0.0 ms**

So the **network + encode + decode cost of the relay is ~1 ms** on loopback.
Everything else is frame quantization.

**Windowed (two real windows, vsync), 63 matched `GE_PLAYER_SHOT` events:**

| Type | n | min | median | p90 | max |
|---|---|---|---|---|---|
| `GE_PLAYER_SHOT` | 63 | 52.6 ms | **89.6 ms** | 105.4 ms | 106.1 ms |
| `GE_DAMAGE` | 25 | 52.9 ms | 90.0 ms | 103.9 ms | 105.7 ms |
| `GE_KILL` | 5 | 52.9 ms | 90.0 ms | 92.2 ms | 102.1 ms |

**Decomposition: delay = one host frame + one client frame + ~1 ms network.**
This machine ran the windowed game at **~25 FPS (40 ms frames)** — software
rendering on a 1920x1080 panel at 59.96 Hz — so 40 + 40 + 1 ≈ 81 ms, matching
the measured ~90 ms median. On a machine that holds 60 FPS the same path costs
**~33 ms** (16.7 + 16.7 + 1).

### What this means for the design

- **The delay is frame-bound, not network-bound.** No amount of netcode tuning
  removes it; a faster network would change ~1 ms of ~90 ms.
- **A client hears its own gunfire late by up to two frames.** At 60 FPS that is
  ~33 ms, which is at the edge of the ~20-45 ms window in which audio is
  normally read as simultaneous with the action that caused it. Below 60 FPS it
  is plainly late.
- **[audio-plan] §4 option (c) — "build (c) only if that latency proves
  audible" — is therefore a live question, not a formality.** The mitigation
  (play locally, dedupe the relayed copy by bullet entity id) removes the two
  frames for the *local* player's own shots, and costs a mirror-side
  derivation, which is the pattern §4 rejected as option (b) for the other
  sounds. **This is a PM decision, not an audio decision** — §4 is closed and
  is not re-opened here; the measurement is the input it was waiting for.
- **Every other sound is unaffected.** Kills, damage, pickups, wave starts and
  explosions are events *about* the world, not about the player's own hands, so
  one relay frame of delay is invisible to the listener.
