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

## Backlog — deliberately deferred

- Snapshot packet size / MTU fragmentation. Pre-existing (the 1024-entity cap
  already produced a ~26 KB packet). N1 makes it ~31 KB. Out of scope here.
- Client-side prediction and reconciliation. The client is currently a pure
  mirror, which is correct but adds input latency. Only worth doing if the
  latency is measured and found bad.
- Per-entity animation phase over the wire. The host's `system_animation` is not
  replicated, so walking frames may not match exactly. Same class of issue as
  N1, smaller visible impact.

## Definition of done for the branch

N1, N2 and N3 all accepted; `AGENTS.md` gates green; `docs/BUGS.md` B19–B23
resolved with commit refs; `docs/PENDING_VERIFICATION.md` lists what still needs
a human two-window run; branch pushed; `main` untouched.