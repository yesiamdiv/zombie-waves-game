# QA Test Guide — Multiplayer (S5-3)

Target lanes: worktrees/multiplayer (feature/net-protocol) and worktrees/assets (feature/assets-maps), both on 10fd6005.

## Build & Setup
- Configure and build from the multiplayer worktree:
  - cd worktrees/multiplayer && cmake -S . -B build -G Ninja && cmake --build build -j
- Always use -G Ninja; FETCHCONTENT_BASE_DIR defaults to $HOME/.cache/cmake-deps (shared). Never build under /tmp.
- Gate: build -Werror clean, ctest --test-dir build (3/3), ./build/zombie_tests green, python3 ../../tools/gen_assets.py --verify (33 textures byte-identical). Two seed-42 SP runs must be byte-identical. The exact expected counts are declared once in docs/knowledge/gate-baseline.md — cite that topic, do not copy the numbers here.

## Two-instance local test (AGENTS.md)
Run host and client in background (detached) — GUI windows cannot block the tool call:
```bash
R=/home/divyam-redkar/development/personal/SDL3/open-world-zombie-waves/worktrees/multiplayer
cd "$R/build"
IP=$(hostname -I | awk '{print $1}')
setsid ./open_world_zombie_waves --host --port=5123 --name=Host \
  >/tmp/opencode/coop_host.log 2>&1 </dev/null &
sleep 2
setsid ./open_world_zombie_waves --join=$IP:5123 --name=Guest \
  >/tmp/opencode/coop_join.log 2>&1 </dev/null &
```
Note: use explicit PIDs to stop them (do not `pkill -f open_world_zombie_waves` — it matches the calling shell). Logs go to /tmp/opencode/coop_*.log.

Headless verification signals (both --auto-start or observe after join):
- Host: "Spawned remote player entity for slot N", "WAVE n STARTED", "WAVE n COMPLETE"
- Client: "CLIENT mirror seq=..." and event log contains WAVE_START, KILL, POINTS, DAMAGE, ITEM_PICKUP, PLAYER_HEALTH. ENTITY_SPAWN count is 0 on client (expected).

## Human eye checks (from docs/PENDING_VERIFICATION.md)
B32–B35 (unproven on screen): run two windows side-by-side with a bot client or second human.
- B32 (client dies): when client player dies, client should not show a blank screen and must show its own name tag. Verify visually.
- B33 (shop prices): on client, shop prices reflect host state (not frozen local). Verify UI reflects updates after purchases.
- B34 (name tag overlap): player name tag must not overlap zombie/wave counter; check layout at bottom-left area.
- B35 (client own name tag color): client's own name tag should appear in correct color; compare Host vs Guest.

## I5 / Animation on-screen (human)
I5 is deliberately unverified on-screen in headless gates; check:
- Sprites/rendering for players/zombies/bullets/grenades/rockets/items appear reasonable; no missing visual states that affect gameplay.
- Animation-related rendering (if present) is cosmetic and does not affect determinism.
- Beacons, mirror interpolation, HUD, and pause overlay render correctly in two-window view.

## Pending human items (docs/PENDING_VERIFICATION.md)
Review all "unverified" items listed there. Anything marked "unproven" requires human confirmation. Note observations (screenshots not available here; describe pass/fail and brief evidence).

## References
- docs/knowledge/gate-baseline.md (current test count, texture count, SP log reference)
- docs/PENDING_VERIFICATION.md (B32–B35 checklist)
- docs/BUGS.md (B32–B37 fixed, B38–B40 open)
- AGENTS.md § "Launching two local instances"
