# 02 - Build and Harness

## Build

```bash
cd /home/divyam-redkar/development/personal/SDL3/open-world-zombie-waves
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
```

## Unit tests (must pass before playtesting)

```bash
ctest --test-dir build --output-on-failure
```

If the binary or tests fail to build/pass, stop and report - do not playtest.

## CLI reference

```
Usage: open_world_zombie_waves [options]
  --headless             run with no window/renderer (fast, deterministic)
  --ai=none|script|bot   AI control mode (default none)
  --script <file>        scripted playback (also --script-loop=N)
  --script-loop=N        repeat script N times (if it ends before the run)
  --events=<file>        gameplay event log path (default game_events.log)
  --seed=<n>             deterministic RNG seed
  --run-seconds=<s>      auto-exit after s simulated seconds
  --player-hp=<pct>      start player at pct% HP (0-100; debug/playtest)
  --player-damage-mult=<f>  zombie melee damage multiplier (debug/playtest)
  --zombie-speed-mult=<f>   zombie move-speed multiplier (debug/playtest)
```

Exit code 0 on a clean shutdown (including scripts that issue `quit`).

## Script format

Plain text, commented lines start with `#`. One action per line:

```
@<time_sec> key <name> down|up        # SDL scancode name, e.g. w, space
@<time_sec> click <button> down|up    # button 1-5 (1 = fire)
@<time_sec> aim <x> <y>               # world-space aim target
@<time_sec> quit                      # request application exit
```

## Outputs

- Gameplay event log (path via `--events`, default `game_events.log`): one
  human-readable event per line. Exact format + payload semantics:
  `docs/EVENT_FORMAT.md`. Timestamps are SIMULATED seconds (not wall clock).
- App log `game.log`: debug level logs of game systems (waves, spawns, moves).
- Event ring buffer holds 8192 events and is flushed per frame; all events
  from a run are in the file, in sim-time order.

## Key design facts for reading logs (verified source constants)

- Wave N spawns `5 + N*3` zombies, one per `max(0.3, 2.0 - N*0.1)`s, 3s
  cooldown between waves. Wave 1 = 8 zombies.
- Zombies spawn in a walkable annulus **400-600px around the player**
  (`SPAWN_RING_MIN/MAX`); check this from the log. Zombie detection range is
  700 (>= max spawn distance), so survivors hunt rather than idle.
- Zombie health `100 * difficulty`; difficulty = `1 + (N-1)*0.15`. Zombie speed
  `80 * difficulty`; player speed 200, player HP 200.
- Bullet damage = 25 (so 4 hits kill a wave-1 zombie). Zombie melee = 10.
- Bot: detection 600, standoff 260, flee 70, fire rate ~8.3 shots/s. It always
  engages the nearest zombie and seeks medkits when HP < 75% (item within 500).
- A wave force-ends after 60s (`WAVE_MAX_DURATION`) as a stuck-wave safety net.
- Items spawn from the 15s world rain AND a ~40% kill-drop at each zombie
  death site, capped at `MAX_ALIVE_ITEMS=8` live entities.
- Medkit heals 30 (`b` field on `ITEM_PICKUP`; a=0). `ITEM_SPEED_BOOST` (a=2)
  permanently adds +5 speed each pickup (unlimited ammo, no cap) - treat as a
  potential balance bug.
- Debug knobs for deterministic playtesting: start wounded (`--player-hp=40`)
  to exercise heal-seeking; raise `--zombie-speed-mult`/`--player-damage-mult`
  to make zombies overwhelm an elite bot and reach the death/game-over path.

## References

Read these before analyzing: `docs/EVENT_FORMAT.md`, `docs/DEV_PLAN.md`,
`docs/EXTENDING.md`.