# Asset verifier invariants (AT13)

`python3 tools/gen_assets.py --verify` regenerates the art and then enforces
three invariants, so the B2/B43 class fails loudly in the headless gate instead
of relying on a human noticing a box on the ground.

1. **Entity art is RGBA with transparent corners.** Every PNG under
   `assets/textures/entities/` must be `RGBA` and have alpha 0 at all four
   corners. An opaque corner means the sprite draws as an opaque card (B2/B43).
   The check is scoped to entity art: tiles are opaque by design and the UI
   vignette has coloured corners.
2. **Entity width matches `net_art` `base_px`.** The verifier parses
   `src/net/net_art.c` (`{ "textures/entities/x.png", N }` rows) and requires
   the real PNG width to equal `N`. `base_px` converts a replicated diameter
   into a draw scale, so a mismatch makes the client draw the wrong size. The C
   table is parsed rather than duplicated so the two cannot drift apart.
3. **Emitted texture count matches `docs/knowledge/gate-baseline.md`.** The
   count is read from the file, never hard-coded.

## Count rule (lane-local)

`gate-baseline.md` is a chronological log; the **last** `N textures`
declaration in the current tree is treated as current. This is deliberately
lane-local: the assets lane declares 34, animation's lane declares 41, and the
merged tree must declare the combined 42. Textures change only via a deliberate
re-baseline: update `gate-baseline.md` in the same commit that adds or removes
one. The failure message names the file, line, declared number and emitted
number.

Verified with negative tests: an opaque-corner RGBA, an RGB entity, a 31px
`player.png` against `base_px` 32, and a count mismatch each exit 1 with an
explicit message.
