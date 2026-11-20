# Sprint 7 — W lane log (agent `winport`, branch `feature/windows-port`)

Lane: **W — Windows port & distribution** (W1–W8 in `docs/TICKETS.md`).
This file is the lane's durable record. The PM merges and updates the board;
this agent does **not** edit `docs/TICKETS.md`.

Environment measured in this lane: Windows 11, MSVC Visual Studio 17 2022
Community, x64, CMake 3.30, Python 3.10 + Pillow. Dependency cache:
`D:/Development/.cmake-deps-msvc` (Visual Studio 17 2022 generator, Debug x64,
pre-populated). Build dir `build/`, binaries in **`build/Debug/`** — not
`build/`, which is what every Linux-authored path in the docs assumes.

## Plan (written before implementing, per AGENTS.md workflow rule 4)

Dependency order; one commit per ticket minimum.

- **W1 — MSVC warning flags.** `CMakeLists.txt:115` sets
  `PROJECT_WARNINGS` to `-Wall -Wextra -Werror` unconditionally. MSVC maps
  `-Wall` to `/Wall` and then hard-errors `D8021: invalid numeric argument
  '/Wextra'`, so **all five project targets die before compiling a file**.
  Decision: branch on the compiler id. `MSVC` gets `/W4 /WX` plus narrowly
  targeted `/wdNNNN` disables, each one justified in a comment; GCC/Clang keep
  `-Wall -Wextra -Werror` byte-for-byte so Linux cannot move. Honour the
  author's intent (a real zero-warning gate) — fix warnings rather than
  silence classes of them. A warning that reveals a real defect is filed in
  `docs/BUGS.md` from **B52** up (B51 is taken).
- **W2 — `ZW_UNUSED`.** `__attribute__((unused))` at
  `src/systems/render.c:17,21,25` is a GCC extension. One portable macro in a
  shared header (`src/core/`), used in all three places. On GCC/Clang it
  expands to exactly `__attribute__((unused))`, so the Linux preprocessor
  output is unchanged.
- **W3 — portable timing shim.** `clock_gettime(CLOCK_MONOTONIC)` /
  `nanosleep()` in `tests/net_spike.c:131,405,463` and
  `tests/net_test.c:52,59,352`. Header-only shim in `tests/` — **not** SDL,
  because `zombie_net_spike` deliberately links only ENet so the transport is
  validated independently of the game. Windows path uses
  `QueryPerformanceCounter` + `Sleep`; POSIX path keeps the existing
  `clock_gettime`/`nanosleep` code verbatim.
- **W4 — portable scratch dir.** `tests/tests.c:6,50` uses `<sys/stat.h>` and
  POSIX two-argument `mkdir(path, mode)`. `_mkdir(path)` via `<direct.h>` on
  Windows; POSIX branch unchanged.
- **W5 — Windows gate.** Build with zero warnings in project sources, `ctest
  -C Debug` 3/3, `zombie_tests` green, `python tools/gen_assets.py --verify`
  clean. Numbers recorded as a **new platform section** in
  `docs/knowledge/gate-baseline.md`; the Linux reference is not touched.
- **W6 — runtime.** Reproduce the old blank-menu report on a real windowed run
  before changing anything. Suspect order from `docs/FUTURE_IDEAS.md`:
  renderer creation flags, colour channel order, asset path/cwd, high-DPI.
  Evidence is PowerShell screen-capture PNGs in `build/logs/`; that is
  "screenshot-verified", never "owner-verified" (`SYSTEM_DESIGN_RULES.md` §4).
  Fixes behind `#ifdef _WIN32` only — never a branch the simulation observes.
- **W7 — determinism.** Two same-seed Windows runs must be byte-identical to
  each other. Then *report*, not force, whether the log matches the Linux
  reference (125938 bytes / 1603 lines, unlimited-ammo). Cross-platform float
  identity is a separate question; measuring it honestly is the deliverable.
- **W8 — release zips.** GitHub Actions workflow, `ubuntu-latest` +
  `windows-latest`, both archives attached to a tag, DLLs and `assets/` beside
  the exe. Cannot be executed from here: it ships as **untested CI**.

## Ledger

| ID | What was done | Commit | Status |
|---|---|---|---|
| W1 | MSVC hard-errored on `-Wextra` (D8021) before compiling a file. `PROJECT_WARNINGS` is now compiler-aware: MSVC `/W4 /WX` + 3 justified disables; GCC/Clang flags byte-for-byte unchanged. Then drove the `/W4 /WX` build genuinely clean. | `68bbf59` | done |
| W1 | 15 narrowing conversions made explicit (all value-preserving, none a latent defect). | `67365df` | done |
| W1 | Shadowed `snap` in net_test renamed `hsnap` (C4456). | `edeb98d` | done |
| W1 | C4127 disabled: the suite asserts compile-time constants on purpose. | `25e3d30` | done |
| W2 | `ZW_UNUSED` in new `src/core/compiler.h`; GCC/Clang expansion identical to the old attribute. | `8d7e991` | done |
| W3 | `tests/portable_time.h` (`zw_now_ms`/`zw_sleep_ns`). POSIX branch is the old code verbatim. Deliberately not SDL, so `zombie_net_spike` stays ENet-only. | `ba60a4b` | done |
| W3 | B60: ENet links winmm/ws2_32 only under MinGW -> MSVC link failed. Patched on the imported target. | `1c58b1f` | done |
| W4 | `ZW_MKDIR`: `_mkdir` on Windows, `mkdir(path,0755)` unchanged on POSIX. | `4b32d63` | done |
| W5 | B61: no DLL staging existed at all -> every SDL-linked binary died with STATUS_DLL_NOT_FOUND. POST_BUILD `$<TARGET_RUNTIME_DLLS>` + ctest PATH. | `c241831` | done |
| W5 | B62: Windows 1 MiB stack vs Linux 8 MiB -> net_test overflowed before `main()`. `/STACK:8388608`. | `d680d61` | done |
| W5 | Windows platform section added to `gate-baseline.md`. Texture count deliberately NOT restated (cites the existing declaration). | `b6fcf9d` | done |
| W6 | B64: font chain was two absolute Linux paths -> no font loaded -> no text rendered anywhere. Platform-selected candidate table. Reproduced before fixing; screenshots before and after. | `101c53e` | screenshot-verified |
| W7 | B52: events log opened in text mode -> CRLF in a byte-compared artifact. `fopen(path,"wb")`. | `5863c5f` | done |
| W7 | B63 filed (OPEN, needs owner decision): libc `rand()` is implementation-defined, so cross-platform byte-identity is structurally impossible. | `5863c5f` | measured |
| W8 | Release workflow: ubuntu-latest + windows-latest, ctest per platform, staged-payload verify, zips attached to a v* tag via `gh`. | `24fc654` | untested CI |

Plan commit (decisions recorded before implementing): `ab7b3dd`.
Knowledge topic: `docs/knowledge/windows-port.md` (`?`).

## Measured gate (Windows 11, MSVC VS 17 2022, x64) — both configurations

| Check | Debug | Release |
|---|---|---|
| build, project sources | 0 warnings | 0 warnings |
| `ctest -C <cfg>` | 3/3 | 3/3 |
| `zombie_tests` | 2091 passed / 0 failed | 2091 passed / 0 failed |
| seed-42 SP log, two runs | 142341 bytes / 1829 lines, identical | 142341 bytes / 1829 lines, identical |

`python tools/gen_assets.py --verify` -> clean, exit 0 (count cited, not
restated — see the gate-baseline note). Debug and Release logs are byte-identical
to each other. 2091 matches the Linux merged-`main` count exactly.

## Screenshots (W6 evidence)

- `build/logs/w6_menu.png` — defect reproduced: backdrop only, no text.
- `build/logs/w6_menu_fixed.png` — full menu after the fix.
- `build/logs/w6_world_full.png` — world, sprites and both HUD panels.

## Decision points recorded rather than asked (owner asleep)

1. **B63 / cross-platform determinism.** Proceeding on the assumption that the
   Linux references stay authoritative and Windows is recorded as a parallel
   platform measurement. Replacing `rand()` would re-baseline both Linux
   references, so it is not a lane fix.
2. **Bug id collision.** The PM filed its own B54/B55 concurrently; this lane
   moved to B60-B62 and left B56-B59 unused, documented in `docs/BUGS.md`.
3. **Linux release zip ships no font** and will show a textless menu without a
   system DejaVu/Liberation. Warned in the zip README; bundling a font is a
   licensing decision.
4. **`src/core/log.c` left in text mode** - prose, never byte-compared.
