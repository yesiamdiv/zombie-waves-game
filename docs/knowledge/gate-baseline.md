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

## 2026-10-23 - note by audio: the merged main (5997c86) measures 1862/0

Landing audio on the assets+animation+net main does not change any merged
figure except the C test count, which carries audio's 33 trigger checks:

- `./build/zombie_tests` -> **1862 passed / 0 failed** (was 1808 on the merged
  main before audio). Unlike the lane note above, **1862 is now the merged
  count**; the next lane to land should treat it as the baseline.
- `ctest --test-dir build` -> 3/3; `gen_audio.py --verify` -> 13 sounds;
  `gen_assets.py --verify` -> **50 textures**, unchanged.
- Two sequential `--seed=42` SP runs still byte-identical at **125938 bytes /
  1603 lines**; wave 1 unchanged. Headless opens no audio device.

## 2026-11-10 - note by assets: deliberate re-baseline for limited pistol ammo (gameplay)

Sprint item "Gameplay: ammo limits + knife starter + toggle" makes **limited
pistol ammo the default**. This is a deliberate re-baseline, announced by PM to
the owner in parallel (decision record:
`docs/knowledge/gameplay-ammo-knife.md`).

- **Canonical invariance reference is UNCHANGED** at **125938 bytes / 1603
  lines**, measured with `--unlimited-ammo` (two sequential `--seed=42` 25 s bot
  runs, byte-identical). That is now the command the gate uses: the flag turns
  the new drain off, so the reference still proves net/effects/HUD did not
  perturb the sim.
- **Default (limited) gameplay reference: 129504 bytes / 1648 lines**, two
  sequential `--seed=42` 25 s bot runs byte-identical. This is the new
  default-gameplay baseline while `PISTOL_AMMO_START` is 60 and
  `PISTOL_AMMO_PICKUP` is 20; changing either number re-baselines this figure
  (not the canonical one).
- Wave 1 is unchanged in both modes: `8 zombies, interval: 1.90s,
  difficulty: 1.00`.
- `zombie_tests` -> **1889 passed / 0 failed** on the assets lane (pre-merge
  with net's v10); re-measure after merging main. `ctest` 3/3; 
  `gen_assets.py --verify` -> **50 textures** (knife reuses `sword_blade.png`,
  no new art; `NET_ASSET_VERSION` stays 3).

### 2026-11-10 follow-up by assets: merged main (0fa9b2d, wire v10) into the lane

After merging net's v10, the same run re-measures:

- `./build/zombie_tests` -> **1895 passed / 0 failed** (merged count; was 1862
  before audio/v10 additions plus this gameplay work). This is now the merged
  baseline.
- `ctest --test-dir build` -> 3/3; `gen_assets.py --verify` -> **50 textures**.
- Canonical unlimited SP reference still **125938 bytes / 1603 lines**;
  default limited reference still **129504 bytes / 1648 lines**; both
  byte-identical across two `--seed=42` runs, wave 1 unchanged.

## 2026-11-11 - verified by pm on merged main (assets lane landed)

Merged `feature/assets-maps` (H1-H4 HUD + gameplay knife/ammo) into main at
`c0bc960`. Independently re-ran the whole gate on merged main:

- `./build/zombie_tests` -> **1895 passed / 0 failed**; `ctest` -> 3/3.
- `gen_assets.py --verify` -> **50 textures**; `gen_audio.py --verify` -> **13 sounds**.
- Canonical unlimited SP: **125938 bytes / 1603 lines**, two `--seed=42` runs byte-identical.
- Default limited SP: **129504 bytes / 1648 lines**, two `--seed=42` runs byte-identical.
- Wave 1 unchanged in both modes.

Numbers above stand as current. Remaining after this: net's `net_snapshot.c`
one-line `pistol_ammo` fill, then assets flips the client HUD from `--` to the
replicated value.

## 2026-11-11 - note by net: merged main (wire v10) measures 1896/0 after the pistol_ammo fill

`net_snapshot.c` now copies the host's authoritative `inventory.pistol_ammo`
into `NetPlayerState.pistol_ammo` (the v10 field was already on the wire; no
version, enum, or layout change in this commit). The new host-side assertion
adds one check, so the merged count moves **1895 -> 1896**:

- `./build/zombie_tests` -> **1896 passed / 0 failed** (merged count).
- `ctest --test-dir build` -> 3/3; `gen_assets.py --verify` -> **50 textures**;
  `gen_audio.py --verify` -> **13 sounds**.
- Canonical unlimited SP reference **125938 bytes / 1603 lines**, two sequential
  `--seed=42` 25 s bot runs byte-identical.
- Default limited SP reference **129504 bytes / 1648 lines**, two sequential
  `--seed=42` 25 s bot runs byte-identical.
- Headless two-instance `tools/net_coop_check.py` -> PASS (remote slot spawned,
  8 client mirror lines, one WAVE_START, no client/host errors).
## 2026-11-12 - re-baseline by animation: 1895 -> 2003 after merging main into the polish lane

Merged main (through `2cbaf05`, which includes the assets gameplay merge
`c0bc960` and net's v10 `pistol_ammo`) into `feature/animation-assets`
(`e824a53`), then added the AN6/Phase D transport test. Re-measured on the
merged lane:

- `./build/zombie_tests` -> **2003 passed / 0 failed** (1895 merged baseline +
  84 polish checks: item bob phase, walk stagger, +29 for the Phase D test).
- `ctest --test-dir build` -> 3/3; `gen_assets.py --verify` -> **50 textures**.
- Canonical unlimited SP: **125938 bytes / 1603 lines**, two `--seed=42` runs
  byte-identical.
- Default limited SP: **129504 bytes / 1648 lines**, two `--seed=42` runs
  byte-identical. Wave 1 unchanged in both modes.

This is now the current lane count; the merged-main figure (1895) is history.

## 2026-11-12 - merge by pm: animation lane integrated into main; merged-main count is 2003/0

Merged `feature/animation-assets` (P1-P5 polish + AN6/Phase D `fc5baca`) into
`main` (`4580b61`). Re-verified on the merged `main` tree, not on the lane:

- `./build/zombie_tests` -> **2003 passed / 0 failed** (merged main count).
- `ctest --test-dir build` -> 3/3; `gen_assets.py --verify` -> **50 textures**.
- Canonical unlimited SP: **125938 bytes / 1603 lines**, two sequential
  `--seed=42` 25 s bot runs byte-identical.
- Default limited SP: **129504 bytes / 1648 lines**, two sequential `--seed=42`
  25 s bot runs byte-identical. Wave 1 unchanged in both modes.

**2003 is the current main count.** The animation lane's own entry above said
"current lane count"; this note supersedes it for `main` since the lane is now
merged. Same total because the lane had already absorbed the net fill.

## 2026-11-16 - merge by pm: effects E1 integrated; merged-main count is 2011/0

Merged `feature/effects` (E1 shared world-space text popup `89a4034`, on top of
its merge of main `3c8b427`) into `main` (`7fb6242`). Re-verified on merged main:

- `./build/zombie_tests` -> **2011 passed / 0 failed** (2003 + 8 popup checks).
- `ctest --test-dir build` -> 3/3.
- Canonical unlimited SP: **125938 bytes / 1603 lines**, two `--seed=42` runs
  byte-identical.
- Default limited SP: **129504 bytes / 1648 lines**, two `--seed=42` runs
  byte-identical. Wave 1 unchanged.
- `gen_assets.py --verify` could **not** run in this environment: Pillow 12.1.1
  on Python 3.14.4 fails only `Image.NEAREST` ("ImagingCore.resize returned
  NULL without setting an exception"); BILINEAR/BICUBIC/LANCZOS/BOX all work.
  Environment bug, not the repo; `assets/` is byte-identical to the last green
  verify (`git diff 03b0e6e HEAD -- assets/` empty), so the 50-texture result
  stands. See knowledge/pillow-nearest-env-bug.

## 2026-11-16 - merge by pm: effects E2+E3 integrated; merged-main count is 2032/0

Merged `feature/effects` E2 (damage numbers `4d26b65`) + E3 (points/pickup
popups `9fcc309`) into `main` (`2632ec4`). Re-verified on merged main:

- `./build/zombie_tests` -> **2032 passed / 0 failed** (2011 + 21 E2/E3 checks).
- `ctest --test-dir build` -> 3/3.
- Canonical unlimited SP: **125938 bytes / 1603 lines**, two `--seed=42` runs
  byte-identical.
- Default limited SP: **129504 bytes / 1648 lines**, two `--seed=42` runs
  byte-identical. Wave 1 unchanged.
- `gen_assets.py --verify` still blocked by the Pillow NEAREST env bug
  (knowledge/pillow-nearest-env-bug); `assets/` unchanged since the last green
  verify, so 50 textures stands.

## 2026-11-16 - merge by pm: effects E4+E5 integrated; merged-main count is 2059/0

Merged `feature/effects` E4 (muzzle flash `d6fe142`) + E5 (hit flash `fabff2b`)
into `main` (`85eb0a8`). Re-verified on merged main:

- `./build/zombie_tests` -> **2059 passed / 0 failed** (2032 + 27 E4/E5 checks).
- `ctest --test-dir build` -> 3/3.
- Canonical unlimited SP: **125938 bytes / 1603 lines**, two `--seed=42` runs
  byte-identical.
- Default limited SP: **129504 bytes / 1648 lines**, two `--seed=42` runs
  byte-identical. Wave 1 unchanged.
- `gen_assets.py --verify` still blocked by the Pillow NEAREST env bug; assets
  unchanged since the last green verify.

## 2026-11-16 - merge by pm: effects E6 integrated (E1-E6 complete); merged-main count is 2075/0

Merged `feature/effects` E6 (wave banner `a7947da`) + its docs (`b9d85f5`) into
`main` (`cfd0049`). This is the first SP-invariance proof for E6 (the effects
agent's session could not launch the binary). Re-verified on merged main:

- `./build/zombie_tests` -> **2075 passed / 0 failed** (2059 + 16 wave-banner checks).
- `ctest --test-dir build` -> 3/3.
- Canonical unlimited SP: **125938 bytes / 1603 lines**, two `--seed=42` runs
  byte-identical — E6 did NOT move it.
- Default limited SP: **129504 bytes / 1648 lines**, two `--seed=42` runs
  byte-identical. Wave 1 unchanged.
- `gen_assets.py --verify` still blocked by the Pillow NEAREST env bug; assets
  unchanged since the last green verify (50 textures).

Effects E1-E6 are now all on main. Same numbers hold: 50 textures, 13 sounds.

## 2026-11-18 - merge by pm: S6-1 H5 themed menu backdrop integrated; count stays 2075/0

Merged `feature/assets-maps` H5 (`5fb55b2`, procedural themed menu backdrop,
render-only, no texture/wire/sim change) into `main` (`9c2fdae`). Re-verified on
merged main:

- `./build/zombie_tests` -> **2075 passed / 0 failed** (no test count change).
- `ctest --test-dir build` -> 3/3.
- `python3 tools/gen_assets.py --verify` -> **50 textures** (Pillow NEAREST bug
  did NOT reproduce on this run; see the 2026-11-18 addendum in
  knowledge/pillow-nearest-env-bug).
- Unlimited SP **125938 / 1603**, limited SP **129504 / 1648**, both two-run
  byte-identical. Wave 1 unchanged.

## 2026-11-18 - S6-2 N3 phase 1 landed on main; gate stays 2075/0

Net landed relay-overflow visibility (`49f1efe`) on `main` (`0a11d2f`, after
merging H5 in `f521182`). Local counting/warnings + host HUD notice only; no wire
field, no `NET_WIRE_VERSION` bump, `last_relay_sid` retry untouched. PM re-gate
on merged main:

- `./build/zombie_tests` -> **2075 passed / 0 failed** (no count change).
- `ctest --test-dir build` -> 3/3; `gen_assets.py --verify` -> **50 textures**.
- Unlimited SP **125938 / 1603**, limited SP **129504 / 1648**, both two-run
  byte-identical. Wave 1 unchanged.
- Headless two-instance `tools/net_coop_check.py` -> PASS (remote slot spawned,
  8 client mirror lines, one WAVE_START, host/client errors 0).

HUD notice / warning are unproven on screen until a real overflow is observed.

## 2026-11-18 - S6-4 B47 shop wiring fix on main; count 2075 -> 2091/0

Assets fixed the local/host shop-copy regression (`30c2a57`, merged `c2c2dd3`
and docs `f4a64ed`). PM re-gate on merged main:

- `cmake --build build` -> clean; `./build/zombie_tests` -> **2091 passed /
  0 failed** (16 new `shop_update_wired()` wiring assertions).
- `ctest --test-dir build` -> 3/3; `python3 tools/gen_assets.py --verify` ->
  **50 textures**.
- Unlimited SP **125938 / 1603**, limited SP **129504 / 1648**, both two-run
  byte-identical. Wave 1 unchanged — the shop fix does not touch the gate path.
- Headless shop repro (`build/logs/qa_b47_shop_repro.script`, 6 s, seed 42,
  `--points=1000`) now emits `SHOP_PURCHASE a=2 b=75` followed by
  `PLAYER_SHOT k=grenade` (pre-fix QA saw `k=bullet`).

On-screen shop appearance remains UNPROVEN until a human looks.

## 2026-11-18 - S6-5 forced relay overflow proven (no code change)

QA re-ran the grenade-burst probe on the B47-fixed tree (S6-4). No source or
gate numbers changed (2091/0, ctest 3/3, 50 textures, SP 125938/1603 +
129504/1648 byte-identical). Forced path now proven in a real headless run:
one host warning `NET: relay overflow - 10 event(s) dropped at this frame's
flush (max 48/frame, 10 total)` at wave 5, with `--events` frame 35039 holding
58 relayable events (18 kills) => 48 kept, 10 dropped, no assert, run continued
into wave 6. Measured economics: 3 relayable events per kill + 2 for the blast,
so the cap needs >=16 kills in one frame. HUD text remains unproven on a real
display (same branch as the proven log line, `src/main.c:1536-1547`).

## 2026-11-20 - note by winport: FIRST WINDOWS PLATFORM SECTION (Sprint 7, W5/W7)

**This is a separate platform, not a re-baseline.** Every number above is the
Linux reference and is untouched. Nothing in this section replaces anything in
it. Measured on `feature/windows-port` after W1-W4 and the B52/B61/B62/B64
fixes, on Windows 11, MSVC Visual Studio 17 2022 Community, x64.

Measured in **both** configurations, because `build/Debug/` and
`build/Release/` are separate trees on a multi-config generator and the Linux
gate has only ever had one:

- `cmake --build build --config Debug` and `--config Release` -> **zero
  warnings in project sources**, all five targets. MSVC is now gated at
  `/W4 /WX` (W1); the only warnings anywhere in a build log come from
  third-party sources (ENet's `win32.c` deprecation warnings, harfbuzz), the
  same way SDL3's noise is ignored on Linux.
- `ctest --test-dir build -C Debug` -> **3/3**, and `-C Release` -> **3/3**:
  `net_spike`, `net_loopback`, `core_tests`.
- `build/Debug/zombie_tests.exe` -> **2091 passed / 0 failed**, and
  `build/Release/zombie_tests.exe` -> **2091 passed / 0 failed**. This matches
  the merged-`main` Linux count of 2091 exactly, which is the useful signal:
  the W lane's portability fixes changed no test outcome.
- `python tools/gen_assets.py --verify` -> clean, exit 0. **Texture count: as
  declared in the entries above; this section deliberately does not restate
  the number.** `declared_texture_count()` reads the *last* declaration in
  this file, so a count restated here would silently override a later lane's
  re-baseline on the merged tree - a conflict git does not report. One number,
  declared once (`docs/VERSIONING.md`).
- Note `python`, not `python3`, on this machine.

### Windows paths differ from every path written in the docs

Binaries land in **`build/Debug/`** or **`build/Release/`**, not `build/`.
Every `./build/zombie_tests` in `AGENTS.md` and the docs is a Linux path and
does not exist here. `ctest` needs `-C <config>` or it finds no tests at all.

### Windows single-player determinism (W7)

Command, from `build/Debug/` or `build/Release/`:

```
open_world_zombie_waves.exe --headless --ai=bot --run-seconds=25     --events=a.log --seed=42 --unlimited-ammo
```

- **Same-platform determinism: PASS.** Two sequential same-seed runs are
  byte-identical at **142341 bytes / 1829 lines**.
- **Debug and Release are byte-identical to each other**, which is worth more
  than it looks: it rules out optimisation level and float codegen as a source
  of divergence on this platform.
- **Cross-platform byte-identity with the Linux reference: FAILS, and cannot
  currently succeed.** Decomposed honestly, against Linux's 125938 bytes /
  1603 lines:
  - **1829 bytes** were line-ending translation. The recorder opened the log
    in text mode, so Windows wrote CRLF into a byte-compared artifact. Fixed
    (B52, `fopen(path, "wb")`); the pre-fix figure was 144170 bytes / 1829
    lines and 144170 - 1829 = 142341 accounts for it exactly.
  - **The residual +16403 bytes / +226 lines is a real simulation
    divergence**, and it is not fixable by a platform `#ifdef`. The sim draws
    from libc `rand()` at 34 sites across 13 files, seeded by `srand()`.
    `rand()` is implementation-defined: MSVC's `RAND_MAX` is **32767** against
    glibc's **2147483647**, and the two generators produce entirely different
    sequences from the same seed. The simulations diverge at the first zombie
    spawn's `rand() % 360`. Full analysis and the measured sequences are in
    **B63** (`docs/BUGS.md`).

So the Linux byte/line references stay **exactly as declared above** and the
Windows figure is a *parallel platform measurement*, not a competing one.
Closing the gap means the project owning its generator, which would re-baseline
both Linux references and is therefore an owner/PM decision, not a lane fix.

## 2026-11-20 - RE-BASELINE: 50 -> 68 textures (Sprint 7 lane A, zombie variants)

Deliberate texture re-baseline, declared in the same commit as the art that
moves it, per `docs/knowledge/asset-verify-invariants.md` and
`docs/VERSIONING.md`.

`python tools/gen_assets.py --verify` -> **68 textures**.

The 18 new files are two zombie variant builds, each an idle plus a full
8-frame walk cycle: `zombie_brute.png` + `zombie_brute_walk0..7.png` and
`zombie_runner.png` + `zombie_runner_walk0..7.png`. They exist so a horde
stops reading as one repeated silhouette (B50, ticket A2).

They are **not referenced by `src/net/net_art.c` yet** and nothing draws them:
every zombie still uses `zombie_walk0..7`. The wiring - appended art ids, a
per-entity variant stored on the ECS, one (variant, frame) -> art id mapping,
and a `NET_ASSET_VERSION` bump with the codec test - is outside the art lane
and is specified in `docs/knowledge/art-overhaul-plan.md`. If the integrator
chooses not to take the variant commit, dropping it restores **50**.

Nothing else moves: no `.c`/`.h` file is touched by this lane, no existing PNG
changes size, every `base_px` still matches, so the single-player seed-42
references (unlimited **125938 / 1603**, limited **129504 / 1648**) and the
test counts are unaffected.

## 2026-11-20 - note by G: DELIBERATE RE-BASELINE - the simulation's PRNG is now project-owned (B63)

The owner approved replacing libc `rand()` in the simulation on 2026-11-20.
The sim now draws from the project's own PCG32 stream (`src/core/rng.c`), so
the spawn angles, ring radii, zombie variants and sizes, item types and bot
strafe directions are all different numbers for the same seed. **This moves
the byte/line reference, deliberately and announced**, per the AGENTS.md rule.

Measured on `sprint7/windows-port-and-determinism` at 96ef7b2, Windows,
MSVC x64 Debug:

- `zombie_tests.exe` -> `Tests passed: 2123  Failed: 0` (2114 + 9 new CHECKs
  in `test_rng_stream_pinned`)
- `ctest -C Debug` -> 3/3: `net_spike`, `net_loopback`, `core_tests`
- Build clean: 0 errors, 0 warnings in project sources under `/W4 /WX`
- Two `--seed=42` single-player runs (25 s, `--ai=bot`, `--unlimited-ammo`)
  -> byte-identical, **111750 bytes / 1423 lines** (was 125938 / 1603)
- Two `--seed=42` runs with default limited ammo -> byte-identical,
  **111750 bytes / 1423 lines** (was 129504 / 1648)
- Wave 1 -> still exactly `8 zombies, interval: 1.90s, difficulty: 1.00`
  (wave composition is arithmetic, not a random draw)

**The two references are now the same number, and that is not a mistake.**
Measured, not assumed: the bot fires 68 shots in the 25 s window and collects
one ammo pickup at t=10.825, and `PISTOL_AMMO_START` 60 + `PISTOL_AMMO_PICKUP`
20 = 80 > 68, so the ammo cap never binds inside the gate window. The new
strafe draws changed the bot's firing cadence enough to drop it under the cap.
If the gate's duration or the bot ever changes, expect these two to separate
again.

NOT YET MEASURED: the Linux figure. This machine has no Linux toolchain, so
111750/1423 is the **Windows** measurement. The whole point of B63 is that
Linux should now produce the same number - but that is a prediction until
someone runs it. The first Linux run is the real verification; if it disagrees,
the bug is in the PRNG port, not in the baseline.

Texture count unchanged at 68 - no asset was touched, and `gen_assets.py
--verify` was deliberately NOT run, because running it dirties every PNG
(B51).
