# Pending verification & known-open work

Written 2026-09-30 on `feature/multiplayer`, before the cross-branch merge, so
the items survive into `main`.

These are **not** regressions and **not** known-broken. They are things that are
believed fixed or believed correct but have **not** been proven, plus two items
deliberately left open. Nothing here blocks the merge.

## Why this file exists

The R13 fix series was verified almost entirely headlessly. That is a real limit,
not a formality:

- `render()` returns immediately when `game.headless` is set, so **no visual
  change can be verified headlessly at all**.
- The `--script` harness cannot trigger the pause menu. It sets *held* keys
  (`input.keys[]`) while `input_key_pressed()` reads the *press-edge* array
  (`keys_pressed[]`), which only real SDL events fill. A scripted
  `key Escape down` is a no-op for pause.
- On this machine `xdotool` is absent, `libXtst` is present but the XTEST
  extension is **not** available on the XWayland display (`XTestQueryExtension`
  returns present=0), GNOME's screenshot D-Bus API is blocked, and
  `ffmpeg x11grab` cannot see composited Wayland windows (mean luma ~16/255).
  So a real keypress and a real screenshot are both unavailable to an agent here.

The last point is why **R13-I5 survived nine commits**: it is a visual defect,
and every automated gate in this repo is blind to visual defects.

## Verified (safe to close)

Headless-verified, reproducible, in `docs/R13_BUGFIX_TRACKER.md`:

- R13-C1 wave budget uses real player count — 2P waves 1-3 =
  `10/1.85/1.00`, `13/1.75/1.15`, `16/1.65/1.30`
- R13-C2 remote player entity spawns and is reconciled; client mirror `ents` grows
- R13-C3 + D1 client log carries `WAVE_START`/`KILL`/`POINTS`/`DAMAGE`/`ITEM_PICKUP`;
  `KILL == POINTS` counts align per kill
- R13-I1 mirror clock does not pin — `host_sim` 1.04 → 85.04, 0 non-increasing
  steps, 0 samples clamped at 1.000
- R13-I6 mid-match joiner anchors to `world_get_spawn_point()` — spawns at
  `(1510, 1470)` = spawn `(1440, 1440)` + `(70, 30)`
- R13-I4 relay half — `GE_HOST_PAUSE` reaches the client and logs
  `host PAUSED` / `host resumed` (`EVT=HOST_PAUSE` x2)
- SP gate held throughout: Release `-Werror`, 0 project warnings, `ctest` 3/3,
  same-seed diff-empty, wave 1 `8z/1.90s/1.00`

## OPEN — real defect, deliberately left open

### R13-I5 — the client renders dots, not sprites (Major, open)

**Status: FIXED in code (`f7c44f3`) — UNPROVEN on screen.**

The host now transmits appearance (`art`, `size_q`, `tint`) and the client
renderer reproduces it instead of guessing. Every headless gate passes. That
is not evidence: `render()` returns immediately when `game.headless` is set, so
nothing in CI can see a sprite.

**A human must run two real windows and check:**
1. All six entity kinds draw as sprites on the client, not circles.
2. A client's zombies match the host's for the same zombie — both the size
   variation and the per-variant colour.
3. The three pickup types look different (medkit / ammo / speed).
4. The client's own character is a sprite in its slot colour.
5. Each client's sprite sizes match the host's pixel-for-pixel at the same zoom.

Root cause is confirmed and the fix is designed in `docs/NET_PROTOCOL_DESIGN.md`
(Sprint N1), with B19-B21 filed in `docs/BUGS.md`. The decision was reversed
on 2026-10-03: rather than approximate the client's renderer, the host will
transmit appearance explicitly (`art`, `size_q`, `tint`), which is both correct
and future-proof.

Tracked in `docs/R13_BUGFIX_TRACKER.md` and `docs/NET_SPRINT_PLAN.md`.

The host draws entities from their own `CSprite` (players are
`sprite_rect(16, 16, color)`). The client runs a different renderer,
`system_render_mirror`, which never reconstructs sprite geometry — it switches
on `NetEntitySnap.kind` and invents a circle size per kind (player 10px, zombie
11px, bullet 3px, item 7px). Same player, two windows, two shapes.

A related consequence found during the audit: zombie size and colour variant,
and pickup subtype, are *unreplicated* rather than merely mis-rendered, so the
client cannot derive them even in principle (B20, B21). Two further host-authority
violations were found alongside it: the client's HUD reads its own un-simulated
ECS health (B22) and its own stale inventory (B23). Those are Sprint N3.

Colour is correct (`net_slot_color(0) == COLOR_BLUE`), so it is purely shape/size.

**To fix:** make `system_render_mirror` call the same `sprite_rect` /
`sprite_circle` constructors the host's spawners use, keyed off
`NetEntitySnap.kind`. Do **not** widen the snapshot — that changes
`NET_SNAP_ENTRY_BYTES` (26) and the wire version. Alternative considered and
rejected; rationale is in the tracker.

**Note:** R13-I2a/I2b (beacon under player) could not have caught this. On the
client there was never a real player sprite underneath, so ordering a beacon
under a 10px dot looks correct while the character is still missing.

## UNVERIFIED — believed working, not proven

Each needs a real on-device two-instance run with a human looking at the screen.

| ID | What is unproven | How to check |
|---|---|---|
| M8 | **The client freeze itself.** Headless proves the *relay* arrives. It does not prove the client actually stops simulating/rendering or shows "Paused by host". A GUI run showed the host paused for 109 frames while the guest paused **0**, but that host was launched without `--events` and the guest logged no relayed event at all, so the run could not distinguish a real failure from a broken harness. Treated as unproven, not passing. | Two windows, host launched **with** `--events`. Press ESC on the host. Guest must freeze and show "Paused by host"; a second ESC must resume it with "Host resumed". |
| M5-GUI | R13-N1 — the client bot acts on the snapshot mirror rather than a local ECS. The headless mirror clock is proven; the *behaviour* is not. | Guest runs `--ai=bot`; its bot must aim and shoot at zombies the host spawned. |
| M6 | R13-I2a/I2b — the remote player is visible on the client and its beacon sits *under* the sprite, not over it. | Guest window: host's avatar readable as a character, marker beneath it. |
| M7 | R13-I3 — dropped-item bullets render and clear on pickup. | Host or guest window: bullet visible on drop, gone after pickup. |

## Known harness gaps worth fixing later

- `playtesting_prompts/07_*` does not exist. `AGENTS.md` refers to `01…08`, and
  `playtesting_prompts/findings/07_R13_interactive_device_test_2026-09-25.md` is
  a *findings* file from a run, not a prompt that was ever committed. Either
  write `07` or renumber `08` -> `07`.
- A scripted ESC cannot pause (above). If scripted playtests ever need to drive
  pause, the script harness must also populate the press-edge array, or the
  harness needs a `key` action that pushes a real event.
- No automation exists for a visual assertion. Everything in the M5-GUI/M6/M7
  column depends on a human or a vision-capable agent.

### N2 — a refusal a human can actually read

- [ ] Host and client on **different builds**. Client must show
      `Different game art (host N, you M). Both players need the same build.`
      on the main menu, not a bare menu.
- [ ] The message does not overflow the menu box or overlap the buttons.
- [ ] The host's console shows `NET: refused peer ... reason=asset mismatch`.
- [ ] A full server (2 players, 3rd joins) still refuses politely.

## Sprint N3 + N4 (client state, wave panel, beacons, health bars)

Headless proves the values *arrive*; nothing here proves they are *drawn*. All
of the following need one human two-window run:

- [ ] **Client HP bar moves** when the host takes damage. Before N3 the client
      bar was frozen at its join-time value.
- [ ] **Client points / ammo / weapon** update as the host earns/spends them.
- [ ] **Client wave panel** shows the real wave number, zombie count and
      "Next wave in Xs". Before N4 it read `Wave: 0` / `Kills: 0` forever.
- [ ] **Client Kills/Score** match the host's.
- [ ] **Client respawn countdown** appears and counts down (a dead slot has no
      entity, so this state comes only from the per-slot block).
- [ ] **Beacons sit where the host's beacons sit.** Before N4 the client
      recomputed their positions from a copy of the host's formula.
- [ ] **Zombie health bars stay inside their background at wave 3+.** Before
      N4 they overflowed, because the bar divided by the starting HP while
      difficulty was scaling the real maximum.
- [ ] **Client pressing B** shows "Shop is host-only (not networked yet)"
      instead of opening a shop whose purchases do nothing.
- [ ] **Client dead/eliminated overlay** matches the host's game mode.

## Still open by decision

- Particles/effects are not replicated (B29, accepted).
- Animation phase is not replicated (B31, accepted).
- Co-op shopping does not exist (B30, needs a purchase-request protocol).
- Zombie speed/size variants (`docs/FUTURE_IDEAS.md`) — lowest priority, after
  all of the above, and only with an explicit decision about single-player
  gameplay invariance.
