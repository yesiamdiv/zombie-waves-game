
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

## Process & /tmp Cleanup Checks (required)
After any run that captures audio or launches game instances:
- [ ] Check for leftover processes: `pgrep -af "open_world_zombie_waves|pw-record|paplay|parec"` (review output)
- [ ] Check /tmp size/growth: `du -sh /tmp` before/after or note new large files in `/tmp/opencode/`
- [ ] Kill only by explicit PID (never `pkill -f open_world_zombie_waves` — it matches the calling shell)
- [ ] Clean up test artifacts if requested (do not delete unrelated files)
