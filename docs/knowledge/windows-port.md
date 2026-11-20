# Windows port — durable platform decisions

<!-- Project knowledge. Written by `winport`, Sprint 7 lane W (2026-11-20). -->

The first time this project was built with MSVC since ENet, SDL_image, the net
layer, audio, animation and effects were added. Everything here is a decision
that outlives the sprint; the per-ticket narrative is in
`docs/sprint7/winport-log.md` and the defects are B52, B60–B64 in
`docs/BUGS.md`.

## The governing rule

Every platform difference is handled by a **compiler or platform guard**, never
by a branch the simulation can observe. On the POSIX side each guard expands to
exactly the code that was there before, so Linux output cannot move. That is
not a convention here, it is the project's non-negotiable gate
(`AGENTS.md`, `docs/SYSTEM_DESIGN_RULES.md`).

## Toolchain

- **MSVC is the only viable Windows toolchain for this project.** There is no
  GCC/Clang on the port machine that can build it; the MinGW present is a stale
  32-bit install with no SDL3. Do not spend time trying to make MinGW work.
- Generator: Visual Studio 17 2022, `-A x64`. The dependency cache at
  `D:/Development/.cmake-deps-msvc` is bound to that generator — switching to
  Ninja rebuilds all of SDL3 from source.
- Use `python`, not `python3`: `python3` is a broken WindowsApps stub there.

## Paths: binaries are NOT in `build/`

Visual Studio is a **multi-config** generator. Binaries land in
`build/Debug/` or `build/Release/`, with `assets/` and the SDL DLLs staged
beside them. Every `./build/zombie_tests` in `AGENTS.md` and the docs is a
Linux path that does not exist on Windows, and `ctest` finds **no tests at
all** unless given `-C <config>`. Anyone writing a cross-platform gate script
(QA lane, Q1) has to account for both.

## Warning flags are per-compiler

`-Wall -Wextra -Werror` is GCC/Clang only. MSVC hard-errors on `-Wextra`
(`D8021`) before compiling a file, and maps `-Wall` to the system-header
firehose `/Wall`. MSVC is gated at `/W4 /WX` with three justified disables
(C4996, C4200, C4127), each commented in `CMakeLists.txt`. The intent recorded
above `PROJECT_WARNINGS` — a real zero-warning gate — now holds on both
platforms; on Windows it had been silently meaningless.

`/W4` is stricter than `-Wall -Wextra` in places and found 15 narrowing
conversions and one shadowed declaration that GCC never mentions. All were
audited; none was a latent defect. Expect new MSVC-only diagnostics when
unfamiliar code lands.

## Third-party gaps to remember

- **ENet links its Windows libraries only under `if (MINGW)`** (upstream, still
  true at 1.3.18), so MSVC consumers link nothing. Patched on the imported
  target after `FetchContent_MakeAvailable(ENet)`, not by vendoring a fork.
  B60.
- **SDL3/_ttf/_image build as shared libraries on Windows** and nothing staged
  their DLLs, so every binary died with `STATUS_DLL_NOT_FOUND` before `main()`.
  `$<TARGET_RUNTIME_DLLS:tgt>` is the supported answer; it is empty on Linux,
  and `cmake -E copy_if_different` with no sources is an error, so the
  `if(WIN32)` guard is load-bearing rather than cosmetic. B61.
- **SDL_image must keep `GIT_SUBMODULES ""`.** Its submodule tree vendors
  aom/dav1d/libavif/libjxl and more; on Windows the nested paths exceed
  MAX_PATH and the populate step dies outright. The game makes exactly one
  `IMG_LoadTexture()` call, on our own flat RGBA PNGs.

## Stack size: Windows gives 1 MiB, Linux gives 8 MiB

This codebase puts large aggregates on the stack by habit — a `World`, a
`NetSnapshot`, a `Player[MAX_PLAYERS]` as ordinary locals. On Windows that
overflows *before the function runs a statement*, which presents as an instant
segfault with **no output at all** and nothing to diagnose from. The fix is
`/STACK:8388608` on the executables, matching the platform the code was written
against rather than shrinking shared code to fit Windows. B62.

**Carry-forward**: this covers the main thread only. A new thread on Windows
gets 1 MiB unless its creation says otherwise, so the same failure returns the
first time this project spawns a thread holding one of those structures.

## Text mode corrupts byte-compared files

Windows' CRT translates `\n` to `\r\n` for a stream opened `"w"`. The events
log is diffed byte for byte by the invariance gate and has a declared byte
count, so it must be opened **`"wb"`** (B52). On POSIX `"w"` and `"wb"` are the
same mode, which is why this is safe. The rule generalises: **any file this
project compares byte-wise, or declares a size for, opens in binary mode.**
`src/core/log.c` is deliberately left in text mode — it is prose, never
compared, and CRLF suits a Windows editor.

## Fonts are not assets here

`assets/` ships no font. The font search was two absolute Linux paths, so
Windows loaded none and **no text rendered anywhere** — menu, HUD, wave banner,
damage numbers (B64). It is now a platform-selected table of regular/bold
pairs: Segoe UI / Arial / Tahoma on Windows, the unchanged DejaVu / Liberation
list on Linux.

**Open distribution gap**: the Linux release zip depends on a system DejaVu or
Liberation font and will show a textless menu without one. The release workflow
warns about it in its README. Bundling a font is a licensing decision for the
owner, not a lane fix.

## Cross-platform determinism is structurally out of reach today

Same-platform determinism holds on Windows: two same-seed runs are
byte-identical, and Debug and Release agree byte for byte, which rules out
optimisation level and float codegen.

Cross-platform byte-identity **cannot** hold, because the simulation draws from
libc `rand()` (34 sites, 13 files) seeded by `srand()`. `rand()` is
implementation-defined — MSVC's `RAND_MAX` is 32767 against glibc's
2147483647, and the sequences differ entirely from the same seed. The two
simulations part company at the first spawn's `rand() % 360`. Full analysis in
B63.

**Do not "fix" this casually.** Replacing `rand()` with a project-owned PRNG
would make the log identical everywhere *and* re-baseline both declared Linux
references, which `AGENTS.md` requires be deliberate and announced. It is an
owner/PM decision.

Multiplayer is unaffected today: the host is the single source of truth and a
client only consumes host state, so a Windows client joining a Linux host runs
no `rand()`-driven simulation of its own. The risk is latent and would become
real with lockstep or client-side prediction.

## What a headless Windows gate still cannot prove

The same blind spot as Linux, and worth restating because CI makes it tempting
to forget: `render()` returns immediately when `game.headless` is set, so no
headless run — local or in Actions — says anything about what is on screen. The
Windows visual evidence in this sprint is **screenshot-verified**
(`build/logs/w6_*.png`), which is stronger than a green gate and still weaker
than a human looking (`docs/SYSTEM_DESIGN_RULES.md` §4).
