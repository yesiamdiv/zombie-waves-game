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
