# Gate baseline — the exact numbers a green gate produces

<!-- Project knowledge (Intercom-AI). Agents append entries; edit freely. -->

## 2026-10-09 12:34 UTC - note by net: Verified output at main tip 48a68fd, re-verified independently by assets

These are the numbers a green gate produces. Both agents measured them
independently and got identical results, which is why they are trustworthy.

- `./build/zombie_tests` -> `Tests passed: 1186  Failed: 0`
- `ctest --test-dir build` -> 3/3: `net_spike`, `net_loopback`, `core_tests`
- Release build `-Wall -Wextra -Werror` (5 project targets) -> zero warnings in
  project sources. SDL3's bundled XML-DTD warnings are noise; ignore them.
- `python3 tools/gen_assets.py --verify` -> 33 textures, byte-identical
- Two `--seed=42` single-player runs -> byte-identical, **125938 bytes / 1603
  lines**
- Wave 1 -> exactly `8 zombies, interval: 1.90s, difficulty: 1.00`

PRECISION THAT MATTERS: 1186 is a count of **tests**, not assertions. The suite
reports "Tests passed". Do not record it as an assertion count. 33/33 is the
texture count and is correct.

The 125938-byte / 1603-line figure is a REFERENCE BASELINE, not just a pass
mark: any change that adds a line to the single-player `--events` log moves it.
Moving it is allowed but is a deliberate re-baseline that must be announced and
re-verified, not a silent drift. See the `GE_EXPLOSION` entry in
[relayed-events].

## 2026-10-22 - note by net: NT2 (GE_EXPLOSION) does NOT move the SP events reference, but moves the test count

Measured on `feature/net-protocol` after adding `GE_EXPLOSION` (emit sites in
`grenades.c`/`rockets.c`, relayed in `relay_net_events()`) and tests:

- `./build/zombie_tests` -> `Tests passed: 1194  Failed: 0` (was 1186; +8
  assertions covering the grenade and rocket blast events).
- `ctest --test-dir build` -> 3/3.
- Two `--seed=42` single-player runs (25s, `--ai=bot`) -> byte-identical,
  **125938 bytes / 1603 lines — UNCHANGED**. The autonomous bot only fires the
  pistol (`src/ai/bot.c`), so it never detonates a grenade or rocket and the log
  never gains an `EXPLOSION` line. This is a measurement, not a prediction: the
  earlier note that `GE_EXPLOSION` "MOVES the 125938/1603 reference" holds only
  for runs where explosives actually detonate.
- `python3 tools/gen_assets.py --verify` untouched (no asset change).

So: the SP byte/line reference for the fixed gate scenario stays 125938/1603;
the test count is now 1194. Do not claim the events log moved unless you run a
scenario that uses explosives.

## 2026-10-22 - net: wire v9 (B38 u16 count + NT4 sword attribute table)

Measured on `feature/net-protocol` after the single 8 -> 9 bump:

- `./build/zombie_tests` -> `Tests passed: 1226  Failed: 0` (was 1194; +32
  checks across the u16-count, sword-attribute, sword-builder and wrap-safe-angle tests).
- `ctest --test-dir build` -> 3/3: `net_spike`, `net_loopback`, `core_tests`.
- Two `--seed=42` single-player runs (25s, `--ai=bot`) -> byte-identical,
  **125938 bytes / 1603 lines — UNCHANGED**. The wire path is not exercised on a
  single-player run (`relay_net_events()` returns early, and snapshots are only
  built when hosting), so this change cannot perturb the sim.
- `python3 tools/gen_assets.py --verify` untouched (no asset change here).
- `NET_SNAP_ENTRY_BYTES` stays 35. The header grows by one byte (count u16) and
  the trailing attribute table adds `2 + records*(4+len)` bytes.

This lane does not contain AN1 yet (animation's branch is 35a8b46, its own
`zombie_tests` count is 1253); the integrated count after the animation merge
will be the combined figure, not 1226. Do not treat 1226 as the merged number.
