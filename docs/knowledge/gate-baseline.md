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

## 2026-10-22 18:47 UTC - note by animation: the test count moves 1186 -> 1253 (the walk-frame art table checks)

The art table gained eight rows (the zombie walk cycle), and `test_net_art_table`
now checks each of them: the id resolves to the expected file name, the base
size is right, and the file is really shipped in the asset tree. That is 67
extra checks, so `./build/zombie_tests` now reports **1253 passed, 0 failed**
instead of 1186.

Everything else in this baseline is UNCHANGED and was re-verified in the same
commit: ctest 3/3, `gen_assets.py --verify` 41 textures, the release build
clean, and the two `--seed=42` single-player runs still byte-identical at
**125938 bytes / 1603 lines** with wave 1 still `8 zombies, interval: 1.90s,
difficulty: 1.00`.

The 1186 in the 2026-10-09 entry above is the historical number; **1253** is
current. It is still a count of TESTS, not assertions.

## 2026-10-22 18:58 UTC - note by animation: the test count moves 1347 -> 1382 (the rate-rule test)

`test_animation_rate_rule` (AN4) simulates the client's view of the animation
and checks the <= 15 fps rule, so the suite gained 35 checks:
`./build/zombie_tests` now reports **1382 passed, 0 failed** instead of 1347.

Everything else in this baseline is UNCHANGED and was re-verified in the same
commit: ctest 3/3, `gen_assets.py --verify` 41 textures, the release build
clean, and the two `--seed=42` single-player runs still byte-identical at
**125938 bytes / 1603 lines** with wave 1 still `8 zombies, interval: 1.90s,
difficulty: 1.00`.

The 1347 in the 2026-10-22 18:47 UTC entry above is the historical number;
**1382** is current.

## 2026-10-22 19:30 UTC - note by animation: the test count moves 1382 -> 1654 (AN5)

AN5 added `test_animation_states` (the pose follows `CZombieTag.state`),
`test_animation_missing_texture` (the flat-shape fallback, the frame index
still advancing) and `test_item_bob` (the bob moves only the displayed
position; the pickup range and payload read `CItemTag.base_pos`). The bob test
samples 240 ticks, which is why the suite jumps by 272 checks:
`./build/zombie_tests` now reports **1654 passed, 0 failed**.

Everything else is unchanged and was re-verified in the same commit: ctest 3/3,
`gen_assets.py --verify` 41 textures, the release build clean, and the two
`--seed=42` single-player runs still byte-identical at **125938 bytes / 1603
lines** - which is the point of the base_pos split; the bob does not move the
sim by a tick. Wave 1 still `WAVE_START a=1.00 b=8.00`.

## 2026-10-22 19:45 UTC - note by animation: the texture count moves 41 -> 49 (the player walk cycle)

AN8's art landed: eight individual player-walk PNGs
(`player_walk0..7.png`, 32x32, frame 0 pixel-identical to `player.png` so the
player does not pop when it starts moving). `gen_assets.py --verify` now
reports **49 textures**, and because `--verify` counts files, this is declared
here with the art (the art ids and `NET_ASSET_VERSION 3` follow in their own
commit, the same split as AN2 -> AN1).

Lane-local count. Once assets' B45 vignette merges the integrated count is 50.
Everything else is unchanged: ctest 3/3, `zombie_tests` 1654/0, two `--seed=42`
runs byte-identical at 125938 bytes / 1603 lines.

## 2026-10-22 19:50 UTC - note by animation: the test count moves 1654 -> 1721 (the player walk ids)

The player walk rows added 67 checks to `test_net_art_table` (eight ids, each
with its path, `base_px`, file-on-disk and round trip, plus the contiguity and
idle-size checks): `./build/zombie_tests` now reports **1721 passed, 0
failed**. `NET_ASSET_VERSION` is **3**; `NET_WIRE_VERSION` stays 8 and
`NET_SNAP_ENTRY_BYTES` stays 35 - the frame still travels as the art id.

Everything else unchanged and re-verified in the same commit: ctest 3/3,
`gen_assets.py --verify` 49 textures, SP runs byte-identical at 125938 bytes /
1603 lines.

## 2026-10-22 20:10 UTC - note by animation: the test count moves 1721 -> 1759 (the player wiring)

AN8's wiring (player spawn carries `COMP_ANIMATION`, `system_animation()` holds
frame 0 until the player's velocity is non-zero, the frame set is chosen by
`COMP_PLAYER_TAG`) plus `test_animation_player_walk` (ids/paths/wrap, the two
cycles never sharing an id, standing holds 0, moving advances, wrap, stop
returns to 0) takes `zombie_tests` to **1759 passed, 0 failed**. Teeth proven:
forcing `moving = true` fails four of the checks, including the standing one.

Textures stay 49; ctest 3/3; two `--seed=42` SP runs byte-identical at 125938
bytes / 1603 lines (the player's frame is display-only).
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

- `./build/zombie_tests` -> `Tests passed: 1235  Failed: 0` (was 1194; +41
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
will be the combined figure, not 1235. Do not treat 1235 as the merged number.

## 2026-10-23 11:52 UTC - note by net: the merged main tree measures 1808/0 (animation + net); SP reference unchanged

Measured on `main` at `6801d7c` (merge of `feature/net-protocol` into animation's
`3648856`), on the **merged** tree rather than either lane:

- `./build/zombie_tests` -> `Tests passed: 1808  Failed: 0`. That is animation's
  1759 plus net's 49 net-only checks (the 1235 lane figure predates
  AN1/AN4/AN5/AN8; 1759 is animation's lane-only figure).
- `ctest --test-dir build` -> 3/3: `net_spike`, `net_loopback`, `core_tests`.
- `python3 tools/gen_assets.py --verify` -> 49 textures.
- Two `--seed=42` single-player runs (25s, `--ai=bot`) -> byte-identical,
  **125938 bytes / 1603 lines — UNCHANGED**; wave 1 still `8 zombies,
  interval: 1.90s, difficulty: 1.00`.
- Headless two-instance: host spawned the remote slot and started wave 1
  (10 zombies, 1.85s for 2 players); the client mirror reached `wave=1 ents=2`;
  no rejects and no errors.

**1808 is the merged number.** The 1235 above is net's lane figure before
animation's tests and 1759 is animation's lane-only figure; neither is the gate
number for this tree. Version registry here: `NET_WIRE_VERSION` **9**,
`NET_ASSET_VERSION` **3**, `NET_ART_COUNT` **26**, `NET_SNAP_ENTRY_BYTES` **35**.
## 2026-10-22 - note by assets: B45 adds the baked vignette texture

B45 replaces the banded damage vignette with one baked texture,
`assets/textures/ui/vignette.png` (`knowledge/ui-damage-vignette`). It is
UI-only art - not a net art id - so `NET_ASSET_VERSION` is untouched. It is
still a new texture, so the count moves **33 -> 34**:

- `python3 tools/gen_assets.py --verify` -> **34 textures**, byte-identical

Everything else is unchanged at this tip: `zombie_tests` 1186/0, `ctest` 3/3,
and the two `--seed=42` single-player runs are still byte-identical **125938
bytes / 1603 lines** - the vignette is a HUD overlay and touches no simulation
or event. Measured on `feature/assets-maps` after the B45 fix.

## 2026-10-23 12:00 UTC - note by pm: the merged main tree measures 1808/0 and 50 textures (animation + net + assets)

Measured on `main` after merging `feature/assets-maps` into the animation+net
tip (`6801d7c`), on the **merged** tree:

- `./build/zombie_tests` -> `Tests passed: 1808  Failed: 0` - unchanged from the
  animation+net merge. The assets lane adds verifier checks in
  `tools/gen_assets.py` (AT13), not C test cases, so the suite count does not move.
- `ctest --test-dir build` -> 3/3: `net_spike`, `net_loopback`, `core_tests`.
- `python3 tools/gen_assets.py --verify` -> **50 textures**: animation's 49
  (33 + 8 zombie walk + 8 player walk) plus assets' B45
  `ui/vignette.png`. This is the integrated count; the lane-local 41/34/49
  figures above are historical.
- Two `--seed=42` single-player runs (25s, `--ai=bot`) -> byte-identical,
  **125938 bytes / 1603 lines - UNCHANGED**; wave 1 still `8 zombies,
  interval: 1.90s, difficulty: 1.00`. The vignette and loot art are display-only.

**50 is the merged texture count.** Version registry here: `NET_WIRE_VERSION`
**9**, `NET_ASSET_VERSION` **3**, `NET_ART_COUNT` **26**,
`NET_SNAP_ENTRY_BYTES` **35**.
>>>>>>>

## 2026-10-23 - note by audio: the test count moves (lane figure); texture count and SP reference unchanged

Measured on `feature/audio` rebased onto `38ee6e8` (main with animation + NT2 +
wire v9), before and after Phase D's trigger table:

- `./build/zombie_tests` -> **1829 passed / 0 failed** on the rebased lane, then
  **1862** with Phase D (+33: the event -> slot table, the 2D predicate and
  hurt-vs-heal through the hp cache).
- `ctest --test-dir build` -> 3/3: `net_spike`, `net_loopback`, `core_tests`.
- `python3 tools/gen_assets.py --verify` -> **50 textures**, the merged count
  declared in the note above. Audio ships WAVs, not textures, so this count does
  not move (`tools/gen_audio.py --verify` reports 13 sounds separately).
- Two **sequential** `--seed=42` single-player runs (25 s, `--ai=bot`) ->
  byte-identical, **125938 bytes / 1603 lines - UNCHANGED**; wave 1 still
  `8 zombies, interval: 1.90s, difficulty: 1.00`. Audio is inert headless
  (`Audio: headless - device never opened`), so it cannot perturb the sim.
- No wire field, no relay-list edit, no event type, no version bump:
  `NET_WIRE_VERSION` 9, `NET_ASSET_VERSION` 3 untouched.

**1862 is a LANE figure**, like net's 1235: the assets lane's tests are not in
it. Do not treat it as the merged number.
