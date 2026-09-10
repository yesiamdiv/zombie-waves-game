# Sprint 2 Development Plan — Events, AI Driver & Playtest Fixes

Author: DEV | Reviewed: QA, MANAGER | Status: Pre-implementation

---

## 1. Goals

1. Fix the three playtest bugs (kill counter, dead waves, far zombies, mouse/crosshair).
2. Capture a **structured gameplay event stream** (separate from the engine log) for debugging, replay, and AI training.
3. Let an **AI driver play the game** through a public input/control API — two modes:
   - **Scripted**: a timed list of predetermined actions (automated tests / replay).
   - **Autonomous bot**: decides actions from live game state each frame.
4. Support **headless** execution (no window/renderer) so QA can run the game logic at full speed.

---

## 2. Root Causes (confirmed in code)

| # | Report | Root cause | Fix |
|---|--------|-----------|-----|
| B1 | Kill count never increases | `waves_on_zombie_killed()` defined (`waves.c:52`) but never called | Call it when a zombie dies in `system_cleanup()` |
| B2 | No new zombies spawn after the wave is cleared | B1's root cause — `zombies_alive` never decrements, so the `waves_spawned >= to_spawn && alive <= 0` completion check (`waves.c:147`) never passes | Same fix as B1 restores wave completion |
| B3 | Zombies too far to find | Spawn points are edge constants, 1400px from player | Spawn zombies in a ring 400–600px around the player, clamped to walkable tiles |
| B4 | Mouse "doesn't work" with keyboard | Crosshair is hardcoded at screen center (`hud.c:158`); aim indicator never tracks the cursor | Draw crosshair at screen-space mouse position |

---

## 3. New Module: Gameplay Event Bus

### 3.1 Concept
A ring buffer of game *events* with a human-readable (and parseable) file dump.
Distinct from the engine log (`LOG_*` in `src/core/log.*`) — this is the **game
domain** log, exactly what a player/AI acting on the game would care about.

### 3.2 Files
```
src/events/event_bus.h/c
docs/EVENT_FORMAT.md        (schema + example)
```

### 3.3 Event types (initial set, extendable)
| Event | Payload |
|-------|---------|
| `EV_PLAYER_SHOT` | bullet id, aim dir |
| `EV_DAMAGE` | target id, target type, amount, source, hp remaining |
| `EV_KILL` | victim id, victim type, killer source |
| `EV_ENTITY_SPAWN` | entity id, type, pos |
| `EV_ENTITY_DEATH` | entity id, type, pos (non-damage deaths) |
| `EV_WAVE_START` / `EV_WAVE_END` | wave number, zombie count |
| `EV_PLAYER_HEALTH` | hp, max (on change) |
| `EV_ITEM_PICKUP` | item type, value |
| `EV_POSITION_SAMPLE` | entity id, type, pos — **sampled ~5x/sec**, not per-frame |
| `EV_INPUT` | raw user input (key/mouse) forwarded from the input layer |

### 3.4 Forwards vs. caught events
Systems **emit** events (push to bus). Nothing polls the bus during normal play —
it is write-only for gameplay. A recorder flushes the bus to `game_events.log`
each frame (or per N ms). An optional non-blocking reader exists for tools/tests.

### 3.5 Recording
- File: `game_events.log`, line-oriented, timestamped (`t`, frame `f`), type, entity, x/y, payload.
- Rotation: cap file size; restart on new session.
- Frequency: position samples throttled to interval (default 200 ms). Everything
  else logs immediately.

---

## 4. New Module: Input Injector + AI Driver

### 4.1 Decouple input sources
Current systems read `InputState` (`src/core/input.h`), filled by
`input_process_event()`. Add explicit injector functions that write into the
*same* struct so the whole game stack works unchanged:

```c
void input_inject_key(InputState*, SDL_Scancode, bool down);   // press/release
void input_inject_aim(InputState*, float wx, float wy);        // world-space aim
void input_inject_mouse(InputState*, float sx, float sy);      // screen-space cursor
void input_inject_click(InputState*, int button, bool down);
```

Real and injected input are OR-combined per frame; `input_update()`
still resets edge flags. Flags on `InputState` mark a frame as
`source_human` / `source_ai` for logging.

### 4.2 AI Driver (`src/ai/`)
```
src/ai/ai_driver.h/c     — controller interface + modes (none/script/bot)
src/ai/script.h/c        — script format, parser, player
src/ai/bot.h/c           — autonomous behavior
```

**Interface** — one abstraction: `AIDriver` with `update(AIDriver *,
float dt, const GameView *, AIControls *)`.
`GameView` = read-only snapshot systems use to let AI see the world
(player pos/hp, nearest zombie, colliders). `AIControls` = the output the
driver writes each frame (input injector calls).

### 4.3 Scripted mode
- Format: timestamped action lines, comment support.
  ```
  @00.000 aim 400 300
  @00.100 click 1 down
  @00.400 key W down
  @01.200 key W up
  @02.000 click 1 up
  @05.000 quit
  ```
- Runner maps each timestamp to playback against a game clock; skipped-load
  if time behind (fast-forward). Used by CI tests (ctest) and manual replay.

### 4.4 Autonomous bot
- Frames per second: finds nearest zombie, aims at it, kites backwards when
  zombies are close, fires on cooldown, walks to nearest medkit when low.
- Small easily-tuned constants (aggression, standoff range, flee threshold).
- Emits the same `EV_INPUT` events as a human (so the log can't tell).

### 4.5 Headless mode
- `--headless`: skip `SDL_CreateWindowViewer`/render loop; run update pipeline
  only (fixed timestep), bot (or script) drives input. Fast, deterministic.
- Used by QA stress runs (e.g. "survive 60s at wave 5").

### 4.6 CLI surface
```
--headless              no window, run as fast as possible
--ai=none|script|bot    AI mode (default none)
--script=FILE.script    scripted playback
--script-loop=N         repeat script N times (fuzz)
--events=FILE           gameplay event log path (default game_events.log)
--seed=N                deterministic RNG seed
--run-seconds=S         auto-quit after S seconds (headless friendly)
```

---

## 5. Integration Points (wiring)

| Site | Change |
|------|--------|
| `src/systems/render.c` `system_cleanup` | emit kill/death; call `waves_on_zombie_killed`; emit particles |
| `src/systems/collision.c` | emit damage events |
| `src/systems/zombie_ai.c` | emit zombie attack / damage-to-player events |
| `src/systems/player_input.c` | emit `EV_PLAYER_SHOT`, forward injected input |
| `src/world/waves.c` | emit spawn / wave start / wave end; **ring spawn around player** |
| `src/ui/hud.c` | crosshair at mouse screen pos; health-change events |
| `src/items/items.c` | emit pickup events |
| `src/main.c` | init event bus + AI driver; CLI parse; headless path; per-frame position sampler; flush recorder |
| `src/core/input.c` | injector API + source flag |

---

## 6. Testing Plan (QA)

| Test | What it proves |
|------|----------------|
| `test_wave_completion` | Kill an entire wave; assert next wave starts and `total_kills` increments |
| `test_script_aim_shot` | Script: aim at zombie + click; assert a bullet spawns toward it |
| `test_bot_wave_survive` | Headless bot survives N waves; assert ≥1 kill, player alive |
| `test_event_stream` | Drive 2s of play; assert `game_events.log` contains damage + kill + sample events |
| `test_deterministic_seed` | Same seed → same zombie spawns (spawn ring reproducibility) |

All wired into `ctest` via the existing `tests/` harness.

---

## 7. Milestones

1. **M1 — Fixes**: B1/B2 (wave fix), B3 (ring spawns), B4 (crosshair) + regression tests.
2. **M2 — Event bus**: `event_bus`, `EVENT_FORMAT.md`, emit from all systems, recording.
3. **M3 — API & driver**: input injector, AI driver interface, scripted mode.
4. **M4 — Bot + headless**: autonomous bot, `--headless`, CLI, stress test.
5. **M5 — QA pass**: full ctest suite, docs updated to v1.1.

---

## 8. Open Items
- Zombie spawn ring radius tuned during playtest (400–600px starting guess).
- Whether bot should Pathfind around walls (M4: no — straight-line is fine v1).
- Event file size: log rotation thresholds (default 5 MB).