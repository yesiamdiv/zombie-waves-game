# Sprint Plan — net protocol: appearance, compatibility, host authority

Branch: `feature/net-protocol` (forked from `feature/multiplayer` @ `398f8d1`).
Design: `docs/NET_PROTOCOL_DESIGN.md`. Bugs: `docs/BUGS.md` (B19–B23).
Status of this document: **planning complete, execution pending** — recorded here
before any code was touched, per the PM gate.

## Standup — planning

**PM scope decision.** Three separable units of work, ordered by dependency:

1. **N1 appearance over the wire** — fixes the visible dots (R13-I5) *and*
   removes the client's need to guess anything. Largest win, touches the wire.
2. **N2 compatibility gating** — depends on N1's `NET_WIRE_VERSION` bump so the
   version bump is made once, deliberately, not twice by accident.
3. **N3 host-authoritative client state** — independent of N1/N2 at the code
   level; could run in parallel by a second agent, but it shares `main.c` and
   `hud.c` with N1's client render path, so it is sequenced last to avoid
   conflicts.

**PM → DEV orders.** Plan committed before execution. One commit per unit of
work; docs follow in their own commit. `main` is out of scope for this work.

## Why three sprints and not one

Each has a different risk profile and a different way of being wrong:

- N1 can silently desync if the version is not bumped (mechanical, testable).
- N2 can be "working" while still telling the player nothing (only visible
  on-device).
- N3 can regress single-player if client state leaks into the host path (caught
  by the byte-identity gate).

Splitting them keeps a failure in one diagnosable in that one.

---

## Sprint N1 — appearance over the wire (DEV)

**Goal.** The client draws what the host draws. No client-side invention.

**Scope.**

- Add `art`, `size_q`, `tint[3]` to `NetEntitySnap`; `NET_SNAP_ENTRY_BYTES`
  26 → 31.
- Shared art table `NET_ART_*` + `net_art_path()` in `src/net/`.
- Host: populate appearance from live `CSprite` in `net_snapshot.c`; derive
  `art` from `CItemTag.type` for pickups; `art = 0` when the host draws a circle.
- Client: `system_render_mirror()` consumes the transmitted appearance. Delete
  the hardcoded size table, the hardcoded colours, and any `rand()` in that path.
- Bump `NET_WIRE_VERSION` 3 → 4.
- Tests: `test_net_codec` appearance round-trip; snapshot builder emits
  non-zero `art` for a textured zombie; mirror decode of `size_q`/`tint`.

**Acceptance.**

- [ ] A client draws sprites, not circles, for all six entity kinds.
- [ ] Zombie size *and* variant tint match the host for the same entity.
- [ ] Pickups show the correct subtype icon (medkit / ammo / speed).
- [ ] Player tint still comes from the slot; unchanged from today.
- [ ] All standing gates in `AGENTS.md` pass, including SP byte-identity.

**Not proven by CI.** Everything above is visual. It needs a human two-window
run. Log as such in `docs/PENDING_VERIFICATION.md`.

---

## Sprint N2 — compatibility gating (DEV)

**Goal.** Two peers that cannot understand each other refuse to start, and both
humans are told why.

**Scope.**

- `NET_ASSET_VERSION` in the `HELLO` body, appended after `map_index`.
- New `NET_REJECT_ASSET` reason + `NET_FLAG_ASSET_MISMATCH`.
- Client-side rejection message naming host and client versions + remedy.
- Host-side feedback when a peer is refused.
- Bump `NET_WIRE_VERSION` 4 → 5.
- Tests: loopback rejects a stale wire peer, a stale world-gen peer, a stale
  asset peer, and an unknown map; each with the right reason code.

**Acceptance.**

- [ ] Mismatched asset version refuses the session instead of rendering
      different art on each side.
- [ ] The refused player sees a readable explanation, not a silent menu return.
- [ ] The host is told a peer was refused.
- [ ] All standing gates pass.

**Not proven by CI.** The *rejection decision* is testable headless; the
*message being legible to a human* is not.

---

## Sprint N3 — host-authoritative client state (DEV)

**Goal.** The client never displays its own state for anything the host owns.

**Scope.**

- Client HUD HP reads the mirror entity for the local slot, not the local ECS.
- Client HUD points / weapon / grenades / launcher ammo are driven by relayed
  `GE_POINTS`, `GE_ITEM_PICKUP`, `GE_KILL` — applied in `drain_net_events()`.
- Re-audit the client path for further self-derived state; record every
  instance, fixed or consciously accepted.

**Acceptance.**

- [ ] Client HUD health tracks the host's, including damage and death.
- [ ] Client HUD points and ammo track the host's after scoring and pickups.
- [ ] No `render_only_client()` path reads un-simulated local ECS/inventory
      state for a host-owned value.
- [ ] SP byte-identity intact.

**Not proven by CI.** HUD text is visual.

---

## Sprint N4 — divergence audit: everything else that can drift (DEV)

**Goal.** Build a complete inventory of *every* value the two peers do not share,
and decide — per item — replicate it, derive it deterministically, or accept the
difference on purpose. Nothing stays "not looked at".

**Why this sprint exists, and why it is last.** N1–N3 fix divergences that were
already found. N4 is the sweep for the ones nobody has looked for yet, because
looking is what finds them. It is deliberately sequenced last: each earlier
sprint changes the replication surface, so an audit run before them would be
partly invalidated by its own fixes. It is the last thing to do precisely so it
is done once, on the final shape of the protocol.

**Method.** For each subsystem, ask: *where does this value live, and who decides
it?* Then classify every value into exactly one bucket:

| Bucket | Meaning | Action |
|---|---|---|
| **R** replicated | Host decides, travels the wire | verify it actually round-trips |
| **D** deterministic | Either peer may compute it, identical result everywhere | prove it, and say why it is safe |
| **A** accepted | Genuinely per-peer, difference is intended and harmless | record the reason |
| **F** fix | A real divergence | file a bug, replicate it, commit |

A value that fits none of the four is a finding in itself.

**Candidate inventory — to be confirmed, not assumed.** Each of these is a
question to answer, not a claim:

- **Player lifecycle** — `eliminated`, `respawn_timer`. The HUD already reads
  both (`hud.c:121-127`) on the client, from local state that never simulates.
  Strong R-candidate.
- **Player combat state** — sword swing timing, attack cooldown, hit timers.
  Cosmetic or not?
- **Wave state** — `wave_number` and `wave_active` are in the snapshot header,
  but the HUD message path uses a *relayed event* instead. Two mechanisms for
  one value is itself a smell: which one wins, and can they disagree?
- **Scoring** — `total_kills` in the snapshot header *and* `GE_KILL`/`GE_POINTS`
  relayed events. Same duplication question as wave state.
- **Items** — does the client show pickups that the host has already consumed?
  Does it linger after collection?
- **Particles and effects** — the client does not simulate, so it presumably
  draws no explosion particles at all. Deliberate or overlooked?
- **Audio** — does the client play anything? Are event sounds aligned with host
  events, or invented client-side?
- **Zombie AI state** — target selection, attack windup. Likely **A**, but must
  be recorded rather than assumed.
- **Camera** — each peer follows its own view. **D** by design.
- **Pause** — relayed (R13-I4). Verify it cannot desync.
- **Anything that calls `rand()` on the client** — the mirror path must contain
  none (ADR-3); the wider client path needs a sweep too.
- **Time bases** — `sim_time` vs local clock; anything comparing them.

**Acceptance.**

- [ ] Every value in every subsystem is classified R / D / A / F, in a table
      committed to `docs/`.
- [ ] Every **F** has a bug number and is fixed or explicitly deferred.
- [ ] Every **D** has a one-line justification for why it is client-safe.
- [ ] No `rand()` in any replicated visual path.
- [ ] All standing gates pass; SP byte-identity intact.

**Not proven by CI.** As always: anything visual or audible is on-device only.
This sprint is mostly a *reasoning* deliverable, and its value is in the writing
down, not in the passing of tests.

---

## Backlog — deliberately deferred

- Snapshot packet size / MTU fragmentation. Pre-existing (the 1024-entity cap
  already produced a ~26 KB packet). N1 makes it ~31 KB. Out of scope here.
- Client-side prediction and reconciliation. The client is currently a pure
  mirror, which is correct but adds input latency. Only worth doing if the
  latency is measured and found bad.
- Per-entity animation phase over the wire. The host's `system_animation` is not
  replicated, so walking frames may not match exactly. Same class of issue as
  N1, smaller visible impact — **N4 will classify it properly**.

## Definition of done for the branch

N1, N2, N3 and N4 all accepted; `AGENTS.md` gates green; `docs/BUGS.md` B19–B23
resolved with commit refs plus any N4 findings; `docs/PENDING_VERIFICATION.md`
lists what still needs a human two-window run; branch pushed; `main` untouched.