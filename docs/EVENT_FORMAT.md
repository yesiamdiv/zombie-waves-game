# Gameplay Event Log Format

The game records a stream of gameplay events to a log file (default
`game_events.log`, override with `--events=<file>`). The event bus is
write-only during gameplay; buffered events are flushed to disk once per
frame, so timing in the log matches simulated time, not wall-clock time.

## File header

The first lines of each log are comments:

```
# open-world-zombie-waves gameplay event log
# format: t=<seconds> f=<frame> sid=<serial> EVT=<type> e=<entity> k=<kind> [payload]
# see docs/EVENT_FORMAT.md
```

## Line format

Every event occupies one line:

```
t=<seconds> f=<frame> sid=<serial> EVT=<TYPE> e=<entity> k=<kind> [x=<world_x> y=<world_y>] [a=<float> b=<float>] [ia=<int> ib=<int>]
```

Fields:

| Field    | Meaning                                               |
|----------|-------------------------------------------------------|
| `t`      | Simulated seconds since the start of the run          |
| `f`      | Frame counter (incremented once per game frame)       |
| `sid`    | Monotonic serial; unique across every line of a run (entity ids under `e=` are recycled by the ECS, so use `sid` for per-line identity) |
| `EVT`    | Event type (see below)                                |
| `e`      | Entity id; `4294967295` (`ECS_NULL_ENTITY`) when unused |
| `k`      | Entity kind: `- player zombie bullet item particle`   |
| `x`,`y`  | World-space position (only if non-zero)               |
| `a`,`b`  | Generic float payloads (type-specific)                |
| `ia`,`ib`| Generic int payloads (type-specific)                  |

## Event types

| Name              | `k`       | Payload                                |
|-------------------|-----------|----------------------------------------|
| `PLAYER_SHOT`     | `bullet`  | `x,y` world pos; `a,b` aim direction   |
| `DAMAGE`          | `zombie`/`player` | `a` damage dealt; `b` health after; `ia` source entity |
| `KILL`            | `zombie`/`player` | `x,y` death position; `ia` killer entity (`4294967295` if none) |
| `ENTITY_SPAWN`    | `zombie`/`bullet`/`item`/`particle` | `x,y` spawn pos |
| `ENTITY_DEATH`    | `-`       | `x,y` death pos                        |
| `WAVE_START`      | `-`       | `a` wave number; `b` zombies to spawn  |
| `WAVE_END`        | `-`       | `a` wave number; `b` total kills so far |
| `PLAYER_HEALTH`   | `player`  | `a` health after the change; `b` max health; `ia` health before the change. Emitted on zombie melee damage (`zombie_ai`) and on `ITEM_HEALTH` pickup heal (`items`) |
| `ITEM_PICKUP`     | `item`    | `a` item type (0=health 1=ammo 2=speed); `b` value |
| `POSITION_SAMPLE` | `player`/`zombie`/`bullet`/`item` | `x,y` position; sampled ~5x/sec |
| `INPUT`           | `-`       | `a` scancode or mouse button; `b` down(1)/up(0); `ia=1` when injected by AI |

## Notes

- `INPUT` lines carry scancode in `a` for keys (0 for keyboard events set
  via `SDL_SCANCODE_*`) and `button+1` in `a` for mouse clicks. The
  `x`,`y` fields hold the world-space mouse position for injected clicks.
- `POSITION_SAMPLE` throttles to ~5 samples/second per entity to keep the
  log compact.
- Item entities spawn from a 15s world rain and a ~40% drop at each zombie
  kill site, gated by a live-item cap (`MAX_ALIVE_ITEMS`, default 8) so long
  survival runs stay bounded.
- `ai=1` (`ia=1`) on `INPUT` lines distinguishes synthetic (AI/script)
  input from human input.
- The ring buffer holds 8192 events; if the buffer ever fills between
  flushes, the oldest buffered events are dropped and a warning is logged.