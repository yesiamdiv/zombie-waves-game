# Animation transport: design A (art-id frames) vs design B (phase field)

<!-- Project knowledge (Intercom-AI). Agents append entries; edit freely. -->

## 2026-10-09 12:35 UTC - decision by assets: PARKED pending a spike. Lean A. Nobody may build B speculatively.

STATUS: PARKED. This is not 'silence to be defaulted on' - it is pending a
spike verdict and must not be acted on either way. `ANIMATION_PLAN.md` §3 makes
this the spike's first verdict.

DESIGN A — the frame travels as an art id. No wire change.
DESIGN B — a phase field is added. Bumps `NET_WIRE_VERSION` 8 -> 9 and requires
a `test_net_codec` update in the same commit.

TWO CHECKS THAT DECIDE IT, both measured not argued:
1. **Sheet constraint.** If a frame lives *inside a sprite sheet*,
   `asset_manager_name_of()` cannot name it, so design A is simply unavailable
   for sheets. Spike A settles this in about an hour.
2. **20 Hz aliasing.** If frames are separate textures, A is available - but
   sampling an 8-frame walk cycle at 20 Hz can stutter or run backwards on the
   client. Whether that is acceptable must be measured.

IF B WINS, its shape: append at the end of `NetEntitySnap` - `anim_phase u8`
(host-authoritative; the client may interpolate between snapshots
deterministically), plus `anim_state u8` (idle/walk/attack) and `facing u8`
**only if** the spike shows they are needed. Per **entity**, not per slot -
zombies are not slots and they are most of the animation.

ID-REUSE RISK: under B, a recycled ECS index can briefly wear a dead entity's
phase unless the design resets phase on a changed `kind`/`art`. Design A is
stateless per snapshot and does not care. A mild point for A, a real duty for B.
Logged as a risk in `ANIMATION_PLAN.md` §7.

LOAD-BEARING CONSTRAINT EITHER WAY: animation stays strictly downstream of the
snapshot - no RNG, no feedback into the sim, no reordering around
`system_cleanup`, and the client computes only interpolation between two host
frames. `SYSTEM_DESIGN_RULES.md` §1 holds regardless of which design wins.

PARTICLES (B29): stay accepted as their own item. Animation does not fold them
in; effects would be a separate proposal with the bandwidth math first.

**Why:** Design A gives maximum host authority - the client draws literally what the host drew, which is the property the I5 fix exists to protect - and needs no wire change. But two measurable constraints can force B, and the spike decides, not the argument.
