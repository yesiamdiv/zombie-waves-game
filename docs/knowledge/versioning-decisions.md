# Version-number decisions and the trap in them

<!-- Project knowledge (Intercom-AI). Agents append entries; edit freely. -->

## 2026-10-09 12:35 UTC - decision by assets: Audio moves no version; art-id appends DO bump NET_ASSET_VERSION; NET_SNAP_ENTRY_BYTES is 35

Registry lives in `docs/VERSIONING.md` §2; the numbers themselves live in
`src/net/net.h`. Read net.h, never a prose number - that is exactly how
VERSIONING.md's own table went stale.

CURRENT VALUES (at main tip 8e7e08e): `NET_WIRE_VERSION = 8`,
`NET_WORLD_GEN_VERSION = 2`, `NET_ASSET_VERSION = 1`, `NET_SNAP_ENTRY_BYTES = 35`.

**`NET_SNAP_ENTRY_BYTES` is 35, not 31.** Four places in the animation docs said
31; all four were corrected. If you read 31 anywhere, that doc is stale.

DECIDED — AUDIO MOVES NO VERSION AT ALL:
Audio must not bump `NET_ASSET_VERSION`: that number guards "two peers ship the
same *art*", and sound is not a picture - mismatched audio cannot desync a
simulation that never reads it. And audio does NOT get its own
`NET_AUDIO_VERSION` either, for the reason in this entry's rationale:
`NET_ASSET_VERSION` is consulted in the HELLO handshake and produces a
`NET_REJECT_ASSET` refusal with a human-readable reason (`net_client.c:258`,
`net_server.c:342`). A new number nothing checks would be a lie.
Revisit only if audio later ships assets read off the wire (sample rate, bank
layout) or gains a handshake field. Recorded in `knowledge/audio-plan` §9 and
`knowledge/agent-rules` §3.

THE TRAP — "no wire bump" IS NOT "no version bump":
This one nearly shipped wrong and is the most important entry here.
Appending art ids to `src/net/net_art.c` needs **no `NET_WIRE_VERSION` bump** -
but it **does** need `NET_ASSET_VERSION 1 -> 2 in the same commit**, because
VERSIONING.md §2's registry says that number bumps when "any file in `assets/`
is added, removed, resized, or replaced, **including `src/net/net_art.c`**".

What happens if you skip it: a peer running the old 10-entry table receives an
id >= 10, `net_art_spec()` returns NULL (`net_art.c:37`), and the frame is
skipped or worse. Silent. **No gate catches it.** Bumping instead makes old and
new peers refuse each other with a clear reason, which is the designed
behaviour.

`I5_FIX.md` §7 originally read as "no version at all" and was the source of the
confusion; it has been corrected to say art-id appends need no *wire* bump but
do need `NET_ASSET_VERSION` in the same commit.

CARETAKER DUTY UNDER DESIGN A: `net_art_spec()` returns NULL for an id >=
`NET_ART_COUNT`, so the host must never emit an art id past the *client's*
table. The handshake rejects the mismatch, so this is belt-and-braces, but the
assert belongs wherever the appends land.

ART-ID BUDGET: `NET_ART_COUNT = 10` today (`src/net/net_art.h:27`) in a u8
field. Design A means ~60-80 appends -> ~70-90 of 255. Nothing else consumes
that id space; nothing but `net_art.c` resolves ids; appending at the end
renumbers nothing in use.

**Why:** A version number exists to be checked. A number no code path consults is worse than no number, because it implies a guarantee that is not enforced.

## 2026-10-09 12:37 UTC - note by net: Exact locations of the constants, and spare room in NetPlayerState

Line references as of the 2026-10-06/09 exchange - verify against the tree, do
not trust these numbers if the file has moved:

- `NET_SNAP_ENTRY_BYTES` (= 35) is at `src/net/net.h:229`.
- `NET_ART_COUNT` (= 10) is at `src/net/net_art.h:27`; `net_art_spec()` returns
  NULL for an out-of-range id at `src/net/net_art.c:37`.
- `NET_ASSET_VERSION` is handshake-checked at `net_client.c:258` and
  `net_server.c:342`, producing `NET_REJECT_ASSET`.
- **`NetPlayerState` has spare room: `NET_PLAYER_STATE_BYTES 23`.** This is the
  fact behind preferring a `NetPlayerState` bit over a new event for reload
  state (it is state, not an instant) - there is room for it without growing the
  struct.

## 2026-10-22 - decision by net: NT4 (B40 swords) needs the ONE wire bump; animation's AT3 needs none

INPUT FROM [animation]'s AT3 verdict (commit `7ee1e16`, `knowledge/animation-transport`):
**Design A, individual PNG frames at >= 67 ms.** The frame is the art id the
snapshot already carries, so animation itself adds **no** snapshot field:
`NET_WIRE_VERSION` stays 8 and `NET_SNAP_ENTRY_BYTES` stays 35 for that work.

NT4 IS NOT FREE, and is unrelated to animation's wire cost:
- New kind `NET_ENT_SWORD` (append after `NET_ENT_ITEM`; the snapshot loop at
  `net_snapshot.c:111` currently skips swords as "visuals").
- It must carry: `kind`, `owner`, `art` (blade art), `size_q` (blade length),
  `tint`, and **`angle` as `int16` centidegrees** (0..18000, wrapping at
  180 deg). The blade is drawn through the same `sprite_draw_blade()` path the
  host uses; the client interpolates angle wrap-safe between the two host
  frames. Frame-based sword art was rejected in AT3 (would need >= 16 frames/rev
  and would change the SP picture).
- This adds a field, so it **requires a `NET_WIRE_VERSION` bump**. D1 says the
  angle rides the ONE bump that also fixes B38 (u16 entity count) - not two
  separate bumps. **Open dependency: B38/NT3 is parked by pm, so no bump window
  is open yet; NT4 cannot land until pm opens it.**

EXTENSIBLE ATTRIBUTE BLOCK (D1's "no rework" requirement): keep the fixed
`NetEntitySnap` stride and put kind-specific extras in a versioned side table
after the entity array, not in every entry:
`u16 attr_count`, then `{u16 id, u8 kind, u8 len, u8 bytes[len]}` records.
Sword's record starts with the angle; future attributes (attack speed, etc.)
append bytes and the client reads `min(len, known_len)`, so adding one is
additive rather than a redesign. A per-entry variable blob was rejected because
it destroys the fixed `NET_SNAP_ENTRY_BYTES` stride the codec and B38 depend on.
Payload contents are finalised only after [animation]'s AT4 art append.

**Why:** One bump is cheaper and safer than two, and a side table keeps the
hot-path entity entry fixed while leaving room for attributes D1 already knows
are coming. Recording this now means the codec work is a plan, not a guess.

## 2026-10-22 - net: v9 implemented (single 8 -> 9 commit)

B38 and NT4 landed as one bump, as decided:

- `NetSnapshot.count` is `uint16_t`; encode/decode use `put_u16`/`get_u16`;
  `NET_SNAP_HEADER_BYTES` count term 1 -> 2. `NET_SNAP_MAX_ENTITIES` stays 1024
  and the encoder rejects anything larger. Test: 300 entities round-trip.
- `NetSnapshot` gained `attr_count` + `attrs[NET_SNAP_MAX_ATTRS]`
  (`NetEntityAttr`: `id`, `kind`, `len`, `bytes[8]`), encoded AFTER the entity
  array. Unknown kinds are preserved and ignored by readers.
- `NET_ENT_SWORD = 7`. `net_snapshot.c` emits swords with `owner`, art/blade
  length (`size_q` = blade length in half units) and one attr record
  `{int16 angle_cdeg, u8 width_q}` (len 3). **The angle is SIGNED centidegrees**
  in (-18000, 18000], not 0..18000: the blade is drawn from a pivot, and an
  unsigned half-turn would draw one half of the spin backwards.
- Client: `net_mirror_sample_attr()` blends the sword angle the short way
  around the circle; `system_render_mirror()` draws it with
  `sprite_draw_blade()` at the host's length/width.
- `NET_SNAP_ENTRY_BYTES` stays 35; no existing field offset moved.
