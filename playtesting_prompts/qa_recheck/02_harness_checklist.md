
## Q2: Gate Manifest (must complete)
For every QA round, verify against docs/knowledge/gate-baseline.md:
- [ ] Build clean (cmake --build build)
- [ ] ctest --test-dir build passes
- [ ] ./build/zombie_tests passes
- [ ] python3 tools/gen_assets.py --verify (note lane count)
- [ ] SP determinism: 2 runs with same seed -> byte-identical events log
- [ ] Record binary path/worktree/HEAD/branch

## Q3: Safer Two-Instance Launch
When launching host+join:
- Use `setsid` and detach (do not let GUI block tool call)
- Write logs to `/tmp/opencode/`
- Never use `pkill -f open_world_zombie_waves` (matches shell); use PIDs or `pgrep` carefully
- Print IP/ports/PIDs before joining
- Host must be ready before join (sleep 1-2s minimum)
