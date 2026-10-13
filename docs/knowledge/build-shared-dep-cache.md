# The shared FetchContent cache, and why feature branches re-download 1.5G

<!-- Project knowledge (Intercom-AI). Agents append entries; edit freely. -->

## 2026-10-09 12:36 UTC - note by user: The default lives only on main; both feature branches predate it and need the -D flag or a merge

This machine mounts /tmp as RAM-backed tmpfs with a per-user quota, so
per-build-dir `_deps` trees (SDL3, SDL_ttf, SDL_image, ENet) were pure waste.
Commit `b369a05` made CMakeLists default `FETCHCONTENT_BASE_DIR` to
`$HOME/.cache/cmake-deps`, shared across every build dir. It is ~1.6G.

**THE CACHE IS GENERATOR-BOUND. Always configure with `-G Ninja`** - a
different generator will refuse the shared cache.

THE TRAP (found 2026-10-09 while moving the worktrees): `b369a05` is on **main
only**. Both `feature/assets-maps` and `feature/net-protocol` predate it, so
their `CMakeLists.txt` has **no default at all**. A plain
`cmake -S <worktree> -B build -G Ninja` in either worktree therefore
re-downloads and rebuilds ~1.5G of dependencies into `build/_deps`.

TWO WAYS OUT:
1. Pass it explicitly every time you configure in those worktrees:
   `-DFETCHCONTENT_BASE_DIR=$HOME/.cache/cmake-deps`
2. Better: merge main's build commit into the feature branch so the default
   travels with it.

Note `10b8cbc` ("drop the duplicated FETCHCONTENT_BASE_DIR block") did NOT
remove this - it deduplicated two copies and kept one. The default is live on
main at `CMakeLists.txt:18`. The branch lag is the whole cause.

Both `worktrees/assets/build` and `worktrees/multiplayer/build` were deleted and
reconfigured on 2026-10-09 (CMake had cached the pre-move absolute paths) and
both came back green: build clean, ctest 3/3, zombie_tests 1186/0.
