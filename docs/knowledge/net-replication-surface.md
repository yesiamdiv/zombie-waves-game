# The replication surface — what crosses the wire today

<!-- Project knowledge (Intercom-AI). Agents append entries; edit freely. -->

## 2026-10-15 - note by assets: migrated from the retired agents/net-next-phase-protocol.md

FACTUAL / CODE-DERIVABLE. This is an inventory of the protocol's current state,
not a decision — verify it against the tree before relying on it, because it can
drift. The authoritative decisions about it live in [relayed-events] (which events
cross the wire, and what adding one costs) and [versioning-decisions] (which
version moves when); this entry restates neither.

SNAPSHOT ENTITIES (`NetEntitySnap`): players, zombies, bullets, grenades,
rockets, dropped items — each carrying `pos`, `vel`, `hp`, `hp_max`, `owner`,
`art`, `size_q`, `tint`. Zombie size/colour variants (B20) and item subtypes
(B21) are already on the wire.

PER-SLOT STATE (`NetPlayerState`): points, grenades, launcher ammo, current
hp/max, respawn countdown, selected weapon, unlocked-weapon mask, alive/
eliminated flags, beacon position. It has spare room (`NET_PLAYER_STATE_BYTES`
23) — see [versioning-decisions].

WAVE-PANEL STATE: wave number, active, total kills, zombies alive,
between-waves, cooldown.

RELAYED GAMEPLAY EVENTS (host -> client, re-emitted onto the client's own bus)
and CLIENT -> HOST messages (input, weapon selection, shop request/result): the
list and its costs are declared once in [relayed-events]; do not copy it here.

NOT REPLICATED (accepted gaps): particles / muzzle flashes / blood (B29), and —
until the animation work lands — sword blades (B40).

**Why:** Migrated from `agents/net-next-phase-protocol.md` when the retired
`agents/` directory was deleted (2026-10-15); it was the one non-decision item in
that file, kept so nothing was lost. It is code-derivable background, so treat it
as a starting map, not a spec — read the structs.
