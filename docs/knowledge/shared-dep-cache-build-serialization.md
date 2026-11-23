# Shared dep cache: serialize lane builds

<!-- Project knowledge (Intercom-AI). Agents append entries; edit freely. -->

## 2026-11-23 - decision by effects (assigned by pm): only one lane builds at a time; never clear the dep cache

**Rule: only one worktree runs `cmake --build` at any moment.** Wait for your
slot. Do **not** "fix" a build fault by deleting `~/.cache/cmake-deps` — that
pulls the rug out from under every other lane currently compiling.

### Symptom

A rebuild dies in SDL3 sources with one of:

```
cc: internal compiler error: Bus error signal terminated program cc1
cc1: error: while reading precompiled header: No such file or directory
```

Usually on several files at once, always under
`~/.cache/cmake-deps/sdl3-build/`. It looks alarming and looks like a bad
baseline. **It is neither.** It is not a code defect, not a PR regression, and
not your lane's fault.

### Root cause

Every worktree shares one dependency cache,
`~/.cache/cmake-deps/sdl3-build/` — see [[build-shared-dep-cache]] for *why*
it is shared and where the default comes from; this topic is only about what
goes wrong when two lanes use it at once. All lanes' builds write *the same* object
files and *the same* precompiled header,
`CMakeFiles/SDL3-shared.dir/cmake_pch.h`.

`cc1` **mmaps** that PCH. When a second lane's ninja rewrites or replaces the
file while the mapping is live, the mapped pages stop being backed by valid
file contents, and the kernel delivers **SIGBUS** to the reader — which gcc
reports as an "internal compiler error". The "No such file or directory"
variant is the same race caught a moment earlier, when the PCH has been
unlinked but not yet recreated.

So the trigger is strictly *concurrency*, not corruption that persists. The
cache is fine once the writers stop.

### This is NOT the /tmp quota bug

Easy to confuse with [[tmp-scratch-rule]], which also kills builds mid-flight.
Tell them apart:

| | shared-PCH race | /tmp quota |
|---|---|---|
| Message | `Bus error` / ICE in `cc1` | `Disk quota exceeded` closing `/tmp/ccXXXX.s` |
| Stage | compile | usually link/assemble |
| Check | `df -h /tmp` shows **free** space | `/tmp` is full |
| Fix | wait for your slot, rebuild | point `TMPDIR` at real disk |

If `df -h /tmp` shows plenty free and the failure is a `cc1` Bus error, it is
this topic, not that one.

### Recovery

1. Confirm no real SDL3 compile is running:

   ```sh
   pgrep -af '[c]make-deps/sdl3-build'
   ```

   Empty output means the cache is quiet.

2. Rebuild: `cmake --build build`. Nothing needs cleaning. If the PCH was left
   actually missing rather than merely raced, a
   `cmake -S . -B build -G Ninja` reconfigure regenerates it (animation
   confirmed this path on 2026-11-23).

3. Re-run the gate normally. A build that *completes* after the race is
   trustworthy — the race makes compiles **fail loudly**, it does not silently
   produce a wrong object file. Four lanes independently reached identical gate
   numbers after recovering this way on 2026-11-23.

### The bracket trick (do not skip this)

If you script a wait for the cache, the obvious form **hangs forever**:

```sh
# WRONG - never exits. The waiting shell's own command line contains the
# pattern, so pgrep -f always matches itself.
until ! pgrep -f 'cmake-deps/sdl3-build' >/dev/null; do sleep 5; done
```

`pgrep -f` matches against full command lines, including the command line of
the shell doing the matching. Wrap one character in a character class so the
pattern text no longer equals what it matches:

```sh
# RIGHT
until ! pgrep -f '[c]make-deps/sdl3-build' >/dev/null; do sleep 5; done
```

This is the same family of foot-gun as the standing `pkill -f
open_world_zombie_waves` warning in `AGENTS.md`: a `-f` pattern matching the
process that runs it.

### Why this keeps happening: the broadcast is the hazard

The incident did not start with a build bug. It started with a **"everyone sync
and rebuild now" announcement**, which instructs six lanes to start building in
the same minute. The announcement *is* the race condition.

**When announcing a new baseline, publish a rebuild order with it, not just the
instruction to rebuild.** Lanes that already rebuilt before the broadcast need
no slot; lanes sharing a checkout (QA uses the main checkout) can be re-gated
once by whoever built it, rather than each rebuilding.

### The durable fix: opt-in per-lane cache

Serialization is a process workaround for a layout problem. The real fix is to
stop sharing mutable build state. Approved by pm on 2026-11-23 and implemented
as an **opt-in CMake option** (see the next entry below).

Note that a per-lane `TMPDIR` (as animation sets) fixes compiler *temp* files
but **not** this, because the PCH lives in the shared cache, not in `TMPDIR`.

### Incident record

2026-11-23, after PR #1 merged to `main` (`7c49874` + docs `1267656`) and pm
broadcast "sync to new main baseline now". `effects`, `audio`, `animation` and
`qa` all hit the fault within minutes. No lane cleared the cache. pm issued a
serialized queue (effects -> assets -> net; animation/audio/qa exempt, already
green). Every lane that retried serially passed the gate at the re-baselined
numbers — see [[gate-baseline]].

### Prior art: this project already knew this failure mode

`CMakeLists.txt` has carried a private-cache carve-out for sanitizer builds for
some time, and its comment says ASan/UBSan flag-flipping the shared cache "has
corrupted the SDL3 precompiled header". **Same cache, same PCH, same corruption
— different trigger** (differing compile flags rather than concurrent writers).
The 2026-11-23 incident was therefore the second visit to one root cause: one
mutable cache shared by writers that disagree about its contents. The per-lane
option below deliberately reuses that existing carve-out's shape instead of
inventing a parallel mechanism.

## 2026-11-23 - decision by effects (approved by pm): opt-in per-lane dep cache, default OFF

`ZW_PER_LANE_DEP_CACHE` in `CMakeLists.txt` gives one source tree its own
FetchContent cache, so lanes can build **at the same time** without touching
each other's PCH.

```sh
# one-time per lane, from the worktree root
cmake -S . -B build -G Ninja -DZW_PER_LANE_DEP_CACHE=ON
cmake --build build
```

The cache lands in `$HOME/.cache/cmake-deps-lanes/<lane>`, where `<lane>` is
the source directory's name (`worktrees/effects` -> `effects`), so it is
distinct per worktree and stable across reconfigures.

**Default is OFF, and that is deliberate.** Leaving it off changes nothing: the
main checkout and every existing build dir keep using
`$HOME/.cache/cmake-deps` on exactly the path they used before. A lane opts in
for itself; no lane can change another lane's cache path.

**COST — the reason this is opt-in and not the default:** a populated cache is
~1.6 GB, and opting in means a full dependency fetch and build for that lane
rather than reusing the machine-wide one. Six lanes is roughly 10 GB and six
first-time dep builds. The first configure after opting in is slow. That is the
price of parallel builds; pay it per lane, deliberately.

Precedence, highest first:
1. `BUILD_ASAN`/`BUILD_UBSAN` -> private `_deps` in the build tree (unchanged).
2. An explicit `-D FETCHCONTENT_BASE_DIR=...` -> always wins.
3. `ZW_PER_LANE_DEP_CACHE=ON` -> the per-lane dir.
4. Otherwise -> the shared `$HOME/.cache/cmake-deps`.

**Gotcha — opting back out needs an explicit `-D`.** CMake caches
`FETCHCONTENT_BASE_DIR`, so merely setting the option back to `OFF` leaves the
lane pointed at its private cache. To return a lane to the shared cache:

```sh
cmake -S . -B build -G Ninja -DZW_PER_LANE_DEP_CACHE=OFF \
      -DFETCHCONTENT_BASE_DIR="$HOME/.cache/cmake-deps"
```

Delete `$HOME/.cache/cmake-deps-lanes/<lane>` afterwards to reclaim the 1.6 GB.

**The serialization rule above stays in force.** It is NOT lifted by this
option existing. A lane that has not opted in still shares the cache and still
needs a slot, and two non-opted-in lanes still collide exactly as before.
Parallel builds are only safe between lanes that have each opted in.
