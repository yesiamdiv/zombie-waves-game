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

## 2026-10-22 13:07 UTC - decision by pm: One NET_WIRE_VERSION 8→9 bump carries both B38 (u16 entity count) and NT4 (sword angle + extensible attrs)

PM decision 2026-10-22 (net's option (a)), following owner D1 and the animation AT3 verdict.

- The AT3 spike chose **design A** — animation frames ride the existing `art` id; animation needs **no** wire change (`NET_WIRE_VERSION` stays 8 for animation).
- **NT4 (B40 swords)** uses a **procedural blade** and does need a field: `int16` centidegrees angle (wraps at 180°) + an extensible attribute block (D1: provision for attack speed and later attributes).
- **NT3 (B38)** is unparked and handled in the **same** bump: fold the snapshot entity count to `u16`.
- Therefore open **one** wire-bump window: `NET_WIRE_VERSION` **8 → 9**, in a single deliberate commit that names the version, updates the codec test (`test_net_codec`), and updates the registry in `VERSIONING.md`. `NET_SNAP_ENTRY_BYTES` stays 35 (the count is in the snapshot header, not per entry).
- Two bumps (option c) is rejected as wasteful. Holding NT4 until B38 is separately unparked (option b) is moot now that the spike is resolved.

Landing conditions: coordinate with animation's AN7 (blade art id) and the freeze handshake (`knowledge/netfile-ownership-and-freeze`). NT2 (`GE_EXPLOSION`) still lands in the audio/AT7 window, unchanged. Nothing is pushed without the owner.

**Why:** The animation spike chose design A (no animation wire change), so B38/NT3 had no bump of its own. NT4's procedural-blade angle is a new wire field and cannot land without a bump, and owner D1 requires it to ride a single bump alongside B38 rather than incurring a second bump. One deliberate 8->9 bump is cheaper and keeps one concern per version number.

## 2026-10-22 20:30 UTC - note by animation: art-id appends 2 -> 3, COUNT 26, and the caretaker duty honoured

On `feature/animation-assets` (landing to main now):

- `NET_ASSET_VERSION` **2 -> 3** for the player walk cycle: eight PNGs
  (`player_walk0..7.png`) and eight rows `NET_ART_PLAYER_WALK0..7 = 18..25`,
  `NET_ART_COUNT` **18 -> 26**. `NET_WIRE_VERSION` stays **8** on this lane -
  animation still needs no wire change (design A), the frame IS the `art` id.
  Net's `8 -> 9` bump (B38 + NT4) is net's commit to land, not mine; a peer
  running animation-without-net sees WIRE 8 / ASSET 3 and refuses cleanly.
- The bump rode the rows in one commit (`e4f730f`), and the PNGs landed in the
  commit before it (`13621b8`) - the AN2 -> AN1 split, so a peer that accepts
  ASSET 3 always has the files the table names.
- **Caretaker duty honoured:** `net_art_path()` / `net_art_base_px()` return
  NULL / 0 for `art >= NET_ART_COUNT` (`src/net/net_art.c`), so a host emitting
  an id a client does not have degrades to a circle instead of a wrong sprite.
  `test_net_art_table` asserts both bounds (`net_art_path(NET_ART_COUNT) ==
  NULL`) and the path -> id round trip for every row, so the table cannot drift
  from the files silently.
- **ART-ID BUDGET:** `NET_ART_COUNT = 26` of 255. Two 8-frame cycles now
  (zombie 10..17, player 18..25); ~30 more 8-frame cycles fit before the u8
  space is a concern. Nothing but `net_art.c` resolves ids.
- The registry table in `docs/VERSIONING.md` §2 is a prose CACHE of this; the
  numbers live in `src/net/net.h`. It now reads 3 for `NET_ASSET_VERSION` on
  this lane, and 8 for `NET_WIRE_VERSION` until net's v9 lands.
