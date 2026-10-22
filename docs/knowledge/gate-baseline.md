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

## 2026-10-22 18:41 UTC - note by animation: the texture count moves 33 -> 41 (the zombie walk cycle)

The zombie walk cycle added 8 frame textures, so `python3 tools/gen_assets.py
--verify` now reports **41 textures** instead of 33. Every other number in this
baseline is UNCHANGED and was re-verified in the same commit: `zombie_tests`
1186/0, ctest 3/3, the release build clean, and the two `--seed=42`
single-player runs still byte-identical at **125938 bytes / 1603 lines** with
wave 1 still `8 zombies, interval: 1.90s, difficulty: 1.00`.

Art does not touch the simulation, so the byte/line reference did not move.
The 33 in the 2026-10-09 entry above is the historical number; **41** is
current. The byte/line reference is untouched by this commit and stays a
deliberate re-baseline only if a line is added to the single-player events log.
