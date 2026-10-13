# Why a client cannot double-fire a relayed event (structural proof)

<!-- Project knowledge (Intercom-AI). Agents append entries; edit freely. -->

## 2026-10-09 12:35 UTC - note by net: The two sets are disjoint: a client never runs the simulation paths that emit relayed types.

QUESTION: a client re-emits relayed events onto its own `g_events` bus
(`net_client.c:361`). If it also generated any of those types locally, audio
would play twice - undetectable to every gate here. Does any type arrive by
both paths?

ANSWER: **No.** Not "none known" - here is the structural reason.

A render-only client never runs the simulation. `update()` hits
`if (render_only_client()) { ... break; }` at `src/main.c:794-802` **before** the
systems loop, so on a client none of these ever execute:
`system_player_input`, `system_sword`, `system_grenades`, `system_rockets`,
`system_zombie_ai`, `system_movement`, `system_collision`, `system_bullets`,
`system_animation`, `system_particles` - nor the wave, item and weapon paths
behind them.

The eight types `relay_net_events()` forwards - `GE_ENTITY_DEATH`,
`GE_WAVE_START`, `GE_PLAYER_HEALTH`, `GE_KILL`, `GE_POINTS`, `GE_DAMAGE`,
`GE_ITEM_PICKUP`, `GE_HOST_PAUSE` - are **every one** emitted from a path a
client never reaches. Belt and braces: `broadcast_host_pause()`
(`main.c:1391`) early-returns `if (!game.net_host_mode)`.

The only types a client writes to its own bus are `GE_INPUT` (its own per-frame
input) and `GE_POSITION_SAMPLE` (`event_bus_samples()`, called outside the
client break). Neither is in the relay list. **The two sets are disjoint.**

CORROBORATED by a real co-op run: client logged `KILL 16`, `POINTS 15`,
`DAMAGE 82`, `ITEM_PICKUP 1` - exactly the host's counts, not double - and
client `ENTITY_SPAWN 0` vs host 147, because the client has no local spawner.

STILL WORTH A COUNTING LOG: a count that reads 1 beats an argument from
structure. If it ever reads 2, that is a bug.

SEPARATE FINDING FOUND WHILE CHECKING THIS (code-read only, not tested):
a client pressing ESC still executes `game.state = GAME_STATE_PAUSED` at
`main.c:740-745`, while `broadcast_host_pause()` no-ops for it - so **a client
can pause its own view while the host keeps simulating, and neither side is
told.** No event is emitted on either side, so it is not a double-fire, but it
is a real asymmetry and is recorded here so it does not wash away.
