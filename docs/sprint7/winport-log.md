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
| W1 | plan recorded | (this commit) | in progress |
