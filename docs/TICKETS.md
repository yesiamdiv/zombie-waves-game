# TICKETS — Sprint 7: Windows port, art overhaul, QA automation

Raised 2026-11-20 on owner directive: "test it and run it and automate test it",
"use a different branch and worktree for each of your agents and use ticket
systems", "find bugs", "make it look good", "make new assets for the player and
zombies and also better animations".

The owner then went off-shift ("im taking a long nap now, so you'll have to work
by yourself"), so this sprint runs **autonomously**: no owner questions, every
decision recorded here or in `docs/knowledge/` rather than asked.

PM/integrator: the owner's main chat (works in the `main` worktree, integrates).
This file is the board. It is the durable record; the chat is not. Every ticket
row gets its status and commit SHA updated as it lands. Bugs found along the way
are filed in `docs/BUGS.md` with a `B<n>` id and cross-referenced here.

## Lanes

| Lane | Agent | Branch | Worktree |
|---|---|---|---|
| W — Windows port & distribution | `winport` | `feature/windows-port` | `worktrees/windows` |
| A — Art & animation overhaul | `art` | `feature/art-overhaul` | `worktrees/art` |
| Q — QA automation & bug hunt | `qa` | `feature/qa-automation` | `worktrees/qa` |

Coordination is manual (PM <-> agent prompts + this board + `docs/knowledge/`).
Intercom-AI was considered and **not** used this sprint: this session has no
`intercom_*` MCP tools wired, and standing them up needs an interactive MCP
config the owner would have to approve — impossible while the owner is away.
Decision recorded so it is not re-litigated.

## Platform context (why this sprint exists)

The last Windows build in this repo was `build/` dated **2026-09-18**, a 32-bit
MSVC configure predating the net, assets, audio, animation and effects work.
Nothing merged since then has ever been compiled on Windows. The known POSIX
dependencies are small and enumerated in W2/W3/W4.

## Owner decisions for this sprint (2026-11-20)

- **B50 / art direction**: *richer pixel art in the same style*. Keep the 16px
  top-down silhouette and the procedural `tools/gen_assets.py` pipeline; raise
  quality inside it. Do **not** change the base grid size and do **not** hand
  author PNGs outside the generator — both were offered and declined.
- **B49 / wave timeout**: *make stragglers hunt the player*. Replace the silent
  force-destroy with an enrage: at the timeout, surviving zombies get a speed and
  aggro boost plus a visible tint, and the wave resolves through gameplay.

## W — Windows port & distribution (agent `winport`)

| ID | Task | Status | Commit |
|---|---|---|---|
| W1 | `PROJECT_WARNINGS` is GCC-only (`-Wall -Wextra -Werror`). MSVC takes `-Wall` as `/Wall` and silently drops the rest (D9002), so the "zero warnings" gate means nothing on Windows. Make the flag set generator-aware (`/W4 /WX` + targeted disables for MSVC) without changing the GCC/Clang flags. | TODO | |
| W2 | `__attribute__((unused))` at `src/systems/render.c:17,21,25` is a hard compile break on MSVC. Introduce one portable `ZW_UNUSED` macro in a shared header and use it. | TODO | |
| W3 | `clock_gettime(CLOCK_MONOTONIC, ...)` and `nanosleep()` in `tests/net_spike.c` (`:131,:405,:463`) and `tests/net_test.c` (`:52,:59,:352`) do not exist in MSVC's CRT. Add a small portable timing shim (monotonic-now + sleep) used by both tests. | TODO | |
| W4 | `tests/tests.c:6` includes `<sys/stat.h>` and creates its scratch dir with the POSIX `mkdir(path, mode)` two-argument form. Make the scratch-dir creation portable. | TODO | |
| W5 | Full green Windows gate: configure + build with zero warnings in project sources, `ctest` 3/3 (net_spike, net_loopback, core_tests), `zombie_tests` green, `python tools/gen_assets.py --verify` clean. Record the Windows numbers in `docs/knowledge/gate-baseline.md` as a **separate platform section** — do not overwrite the Linux reference. | TODO | |
| W6 | Runtime on Windows: a real windowed run must show the menu with visible text, then the world and HUD. `docs/FUTURE_IDEAS.md` already lists the suspect list from the old report (renderer creation flags, colour channel order, asset path/cwd, high-DPI). Reproduce first, then fix; every fix behind a platform `#ifdef`, never a branch the sim can observe. | TODO | |
| W7 | Determinism parity: the seed-42 SP events log must be byte-identical **between two Windows runs**. Cross-platform byte-identity with Linux is a separate question — measure it and report, do not force it. | TODO | |
| W8 | Release zips (owner request, `docs/FUTURE_IDEAS.md` 2026-11-19): a GitHub Actions workflow building on `ubuntu-latest` + `windows-latest` and attaching both archives to a tag. Must ship the DLLs and `assets/` beside the exe. | TODO | |

## A — Art & animation overhaul (agent `art`)

| ID | Task | Status | Commit |
|---|---|---|---|
| A1 | Player sprite redesign inside `gen_assets.py`: readable silhouette at gameplay size, 3-tone shading, clear facing. Same 16px base grid. | TODO | |
| A2 | Zombie sprite redesign + visual variants (e.g. walker / brute / fast) so a crowd does not read as one repeated blob. Variants must not change collision or draw size unless re-baselined in the same commit. | TODO | |
| A3 | Walk cycle overhaul: more frames, real limb swing and body bob, so motion reads as walking. This is the core of B50. | TODO | |
| A4 | Idle/attack frames if the animation transport supports them (`docs/knowledge/animation-transport.md`, design A) — confirm against the transport before adding frames. | TODO | |
| A5 | `tools/gen_assets.py --verify` stays clean, and the declared texture count moves only as a deliberate re-baseline **in the same commit**, per `docs/knowledge/asset-verify-invariants.md` and `docs/VERSIONING.md`. | TODO | |
| A6 | Close B50 as `unproven` until the owner looks at it on screen (`docs/SYSTEM_DESIGN_RULES.md` §4). Headless gates cannot see art. | TODO | |

## Q — QA automation & bug hunt (agent `qa`)

| ID | Task | Status | Commit |
|---|---|---|---|
| Q1 | Windows smoke/gate script (PowerShell or Python, runnable from a clean checkout) that runs the whole verification ritual and prints a pass/fail summary. The ritual in `AGENTS.md` is written in `sh` with `./build/...` paths that do not exist on Windows. | TODO | |
| Q2 | Soak/fuzz headless runs: long runs, many seeds, odd flag combinations, looking for crashes, asserts, leaks and non-termination. File every finding in `docs/BUGS.md`. | TODO | |
| Q3 | Automated determinism harness: N seeds x 2 runs each, diffed, as a single command — so the invariance gate stops being a hand-run ritual. | TODO | |
| Q4 | Adversarial bug hunt across the merged systems (net, shop/economy, waves, items, effects). Prefer reproducible headless repros; findings policy per `docs/knowledge/qa-findings-policy.md`. | TODO | |
| Q5 | B49 enrage behaviour (owner decision above) — verify the wave timeout no longer silently deletes zombies, and that the SP seed-42 references are **unchanged** (the gate runs are 25s, the timeout is at 60s, so a correct change cannot move them). | TODO | |

## Gameplay / infrastructure tickets owned by the PM/integrator

| ID | Task | Status | Commit |
|---|---|---|---|
| G0 | **Blocking**: SDL_image was fetched at its defaults, dragging in its whole vendored codec tree (aom, dav1d, libavif, libjxl + its own 9 submodules, libtiff, libwebp) for one `IMG_LoadTexture()` call. On Windows the nested paths exceed `MAX_PATH` and the configure could never complete — this is *why* there was no Windows build. Narrowed the fetch. | **done** — configure 99s, `sdl_image-src` gigabytes -> 78 MB. Decision: `knowledge/sdl-image-dependency-scope.md` | `c968dbd` |
| G1 | B49 implementation: replace the silent force-destroy in `src/world/waves.c` with the enrage behaviour the owner chose. Keep the soft-lock guarantee: the wave must still be able to end. | **implemented, runtime-unverified** — compile-clean under `gcc -fsyntax-only -Wall -Wextra`; `test_wave_timeout_enrages()` added but **not yet run**, because no Windows binary can be produced until W1 lands | `bee8286` |

## Deferred — raised this sprint, deliberately not done

| ID | Task | Why deferred |
|---|---|---|
| D1 | Enraged zombies render untinted on a **joined client**. The `enraged` flag is host-side only and not on the wire, so the client's snapshot mirror cannot know. | Fixing it means a `NET_WIRE_VERSION` bump for a purely cosmetic bit. `docs/SYSTEM_DESIGN_RULES.md` says a client may add computation only if it is cosmetic *and* deterministic across machines — this is cosmetic but **not** derivable client-side, so it needs a real wire field. Not worth a protocol bump mid-sprint with no net agent staffed. The host/single-player tint is correct. |
| D2 | B51 — `gen_assets.py` dirties all 50 PNGs with no pixel change (PNG encoding is not reproducible across Pillow versions; pixels verified identical). | Filed in `docs/BUGS.md`. Cosmetic-to-git, but it makes the asset gate dirty the tree it guards, which can hide a real art change inside 50 files of noise. Fix options recorded; none chosen. |

## Non-negotiable for every lane

The single-player byte-identity gate in `AGENTS.md` still applies, and on
Windows it is a *new* measurement, not an assumption. Anything visual or audible
is `unproven` until a human looks or listens — write `unproven`, not `fixed`.

---

## Sprint 7 status at wind-down (2026-11-20)

**The game builds, tests and runs on Windows for the first time.** Measured on
`main` after integrating lanes A and W:

| Gate | Result |
|---|---|
| `cmake --build build --config Debug` | **0 errors, 0 warnings** in project sources, under a real `/W4 /WX` gate |
| `zombie_tests.exe` | **2114 passed / 0 failed** |
| `ctest -C Debug` | **3/3** (net_spike, net_loopback, core_tests) |
| `gen_assets.py --verify` | clean, **68 textures** (re-baselined from 50) |
| Two same-seed SP runs, Windows | **byte-identical** to each other |

### Cross-platform determinism: NOT established, and that is a finding

The Windows seed-42 unlimited run is **144170 bytes / 1829 lines**; the Linux
reference is **125938 / 1603**. Two separate causes, and only the first is
cosmetic:

1. **CRLF translation** — confirmed present. `src/events/event_bus.c:58` opens
   the recorder with `fopen(path, "w")`, i.e. text mode, so every `\n` becomes
   `\r\n`: +1829 bytes. Mechanical, fixable with `"wb"`.
2. **A real simulation divergence** — the line count itself moved, +226 events.
   Line endings cannot do that. The prime suspect is `rand()`: `src/world/waves.c`
   uses libc `rand()` for spawn placement and zombie size, and its sequence is
   implementation-defined, so MSVC's CRT and glibc produce different games from
   the same seed. **Same-platform determinism holds; cross-platform does not.**
   Owned by W7. If confirmed, the fix is the project's own small PRNG rather
   than libc's, which is a determinism-surface change and needs its own ticket.

### Lane results

- **Lane A (art)** — complete, all **unproven** pending a human look. Found the
  real cause of B50 and it was worse than "weak art": entity features were drawn
  in the *same palette tone* as the body beneath them (an invisible-by-
  construction sprite), and the stride was authored at 32px but drawn at 16px
  through a NEAREST downscale that discards every odd row, so a +-2px arm swing
  became +-1 screen px and neither figure had legs at all. Re-authored on a
  16x16 logical grid emitted 2x. A4 blocked by the art-id transport (not
  skipped). Variants are unreferenced art pending wiring.
- **Lane W (winport)** — W1-W5 done and integrated; W6-W8 still in progress at
  wind-down. Found B60 (ENet links winmm/ws2_32 only under MinGW), B61 (no DLL
  staging: every SDL binary died with STATUS_DLL_NOT_FOUND before `main()`),
  B62 (Windows' 1 MiB default stack overflows in net_test).
- **Lane Q (qa)** — **not started.** Deliberately dropped at the owner's
  instruction to limit token spend. The tickets stand for a later sprint.

### Bugs filed this sprint

B51 (PNG byte churn), B54 (player hitbox 1.75x its sprite — **fixed**, G2),
B55 (snapshot flat-shape diameter ignores scale — open), B60, B61, B62.

### Still open for the owner

- **B50** stays OPEN/unproven: a clean `--verify` proves the pipeline, not the
  art. Contact sheets are the best available review and are not a sign-off.
- **B49** behaviour is implemented and tested but has **never been seen on a
  display**.
- The cross-platform determinism divergence above needs a decision once W7
  reports.

---

## Sprint 7 CLOSED (2026-11-20) — final integrated state on `main`

All of lane W and lane A are merged. Measured on merged `main`, Windows,
MSVC x64 Debug:

| Gate | Result |
|---|---|
| build, project sources | **0 errors, 0 warnings** under a real `/W4 /WX` gate |
| `zombie_tests.exe` | **2114 passed / 0 failed** |
| `ctest -C Debug` | **3/3** |
| `gen_assets.py --verify` | clean, **68 textures** |
| Two same-seed SP runs (Windows) | **byte-identical**, 142341 bytes / 1829 lines |
| CRLF in the events log | **gone** (B52 `"wb"` fix confirmed) |
| Runtime | **screenshot-verified**: menu, world and both HUD panels render |

### LINUX IS UNVERIFIED — the one thing to do first

Nothing in this sprint was run on Linux; there is no Linux toolchain on this
machine. Every change touching a file Linux compiles was audited and argued to
be a no-op there (GCC/Clang warning flags byte-for-byte unchanged; `ZW_UNUSED`
expands to the original attribute; the POSIX branches of `portable_time.h` and
`ZW_MKDIR` hold the original calls verbatim; `"wb"` == `"w"` on POSIX; the
narrowing casts make explicit what the compiler already inserted implicitly).
**That is reasoning, not measurement.** Re-run the Linux gate before trusting
this merge, and expect the seed-42 references to be unchanged there.

### B63 — the decision waiting for the owner

Cross-platform determinism is **structurally impossible today** and no platform
`#ifdef` can fix it. The simulation draws from libc `rand()` at 34 sites across
13 files. Its sequence is implementation-defined and was measured diverging at
the very first spawn: MSVC `srand(42)` -> 175, 400, 17869 (`RAND_MAX` 32767)
versus glibc -> 71876166, 708592740 (`RAND_MAX` 2147483647).

Same-platform determinism is unaffected and passes on both platforms; Debug and
Release are byte-identical to each other, which rules out optimisation level
and float codegen. Multiplayer is unaffected **today** because the host is the
single source of truth and a client runs no `rand()` simulation of its own —
the risk is latent until lockstep or client-side prediction.

The only fix is a project-owned PRNG, which re-baselines **both** Linux
references (125938/1603 and 129504/1648). That is a deliberate announced
re-baseline and an owner call, so it was filed, not shipped.

### Still unproven by a human

B49 (enrage) and B50 (art) are implemented, tested and reviewed from contact
sheets and screenshots — and **nobody has watched the game move**. A clean
`--verify` proves the pipeline, not the art; a screenshot proves a frame, not
an animation. Both stay OPEN.

### Not done

Lane Q (QA automation, Q1-Q5) was **never started** — dropped on owner
instruction to limit token spend. W8's release-zip CI is **untested**: the YAML
is validated locally but has never executed, and the Linux zip ships no font,
which is the mirror image of B64 and a licensing decision.
