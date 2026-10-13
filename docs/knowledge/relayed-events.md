# Which events cross the wire, and what adding one costs

<!-- Project knowledge (Intercom-AI). Agents append entries; edit freely. -->

## 2026-10-09 12:34 UTC - decision by net: B37 landed (GE_PLAYER_SHOT); GE_WAVE_END free; GE_EXPLOSION needs a re-baseline

The relay list is `relay_net_events()` at `src/main.c:1396` (the `R13-D1`
comment there is the curation rationale). It is owned by [net].

WHY WIDENING THE FILTER IS SAFE FOR SINGLE-PLAYER: the first statement of
`relay_net_events()` is `if (!game.net_host_mode || !g_events) return;`, so in
single-player a newly relayed type is never even considered. SP byte-identity
cannot move. This is why B37 kept the identical 125938-byte count.

LANDED:
- **B37 — `GE_PLAYER_SHOT` is relayed.** Commits `fc258a3` (decision recorded
  first), `acdffff` (the one-line filter change), `48a68fd` (close-out with SHA).
  No `NET_WIRE_VERSION` bump: the type was already in the enum
  (`src/events/event_bus.h:13`, value 1) and the relay struct already carried
  `type, kind, entity, x, y, a, b` — widening a *filter* is not a layout change.
  Old peers survive it: they know the enum and just play no sound.
  Verified by a real co-op run: client went from 0 to 96 `PLAYER_SHOT` (host
  emitted 101; the 5 missing were fired before it connected).

AGREED, NOT YET LANDED:
- **`GE_WAVE_END` (B39) — free.** The event already exists and is emitted
  host-side, so relaying it changes no single-player output. No wire bump. Can
  land independently of animation/audio.
- **`GE_EXPLOSION` — NOT free.** It does not exist today; it is a new
  `GameEventType`. Two knock-ons: `event_type_names[GE_COUNT]`
  (`src/events/event_bus.c:9`) is sized by the enum so the new value must be
  added there; and `event_bus.c:119` logs every event by name, so emitting
  explosions in `grenades.c`/`rockets.c` adds lines to the single-player
  `--events` log and MOVES the 125938-byte / 1603-line reference. Determinism is
  unaffected (two same-seed runs still match) but it is a deliberate
  re-baseline: it must ride the audio commit and be re-verified jointly.
  Proposed payload: `x,y` + `kind` (grenade/rocket), radius free in `a`.
  No wire bump needed - relayed `type` is already u8 and `NetRelayedEvent`
  already carries `x,y,a,b`.

DECIDED TO NEED NO EVENT:
- **Weapon switch** — the client reads `NetPlayerState.weapon` and reacts to a
  change. That is *consuming* host state, not deriving it: allowed.
- **Footsteps** — client-derived from `vel`. Cosmetic, deterministic from the
  snapshot, zero wire cost.

PARKED:
- **Reload** — unknown whether reload state is on the wire. If needed, [net]
  prefers a `NetPlayerState` bit over an event, because it is state, not an
  instant. Waiting on the audio spike. Nobody is to build one speculatively.

**Why:** A client never runs the simulation, so any sound or reaction it needs must either ride a relayed event or be derived from the snapshot. Deriving host-owned facts from a vanishing entity is the B19-B23 authority-violation pattern.

## 2026-10-09 12:37 UTC - note by net: Exact line references and the superseded commit trail

Line references from the exchange - verify before relying on them:

- `relay_net_events()` begins at `src/main.c:1396`; the `R13-D1` comment there
  is the curation rationale.
- The relay curation list was at `src/main.c:1413-1416` before B37.
- After B37 the `GE_PLAYER_SHOT` filter sits at `src/main.c:1422` with the B37
  comment.
- `GE_PLAYER_SHOT` is emitted host-side only, at `player_input.c:107`
  (`kind=GEK_BULLET`), `grenades.c:126` (`GEK_GRENADE`), `rockets.c:52`
  (`GEK_ROCKET`) - each with `x,y` = spawn origin and `a,b` = direction, so one
  relayed event gives origin + weapon class for every shooter.
- A client re-emits relayed events onto its own bus at `net_client.c:361`.
- `GE_PLAYER_SHOT` is in the enum at `src/events/event_bus.h:13`, value 1.

OPTION (c) IS STILL OPEN, AND OPTIONAL: B37 (option (a), relay) gives every
shot including your own, with RTT latency. If that latency is ever audible, a
client may additionally play its own shot from local input (option (c)) and
dedupe by entity id, since the relayed copy carries the host's bullet entity.
(a) and (c) are compatible. (c) is not required to close the gunfire gap.

SUPERSEDED COMMIT TRAIL (kept so older notes can be dated): `704090c`/`c2201dc`
= B36 and `-Werror`; `b7a1851` = the tip `main` was declared cuttable at, plus
`458fb9b`, `4b3df18` for deferral docs and the Windows backlog task. All
superseded by `48a68fd`, the tip after B37 landed.
