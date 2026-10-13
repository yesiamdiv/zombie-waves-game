# Who touches net.h, and the freeze protocol between the two devs

<!-- Project knowledge (Intercom-AI). Agents append entries; edit freely. -->

## 2026-10-09 12:36 UTC - decision by net: Only one agent touches net.h at a time; the writer depends on which animation design wins

BRANCH ORDER: `feature/animation-assets` is cut first; `feature/audio` rebases
onto it. Landing order is animation, then audio.
NOTE (2026-10-09): neither branch has actually been created yet.

THE FREEZE PROTOCOL:
1. [assets] posts "starting" before touching anything.
2. [net] then freezes `src/net/net.h`, `src/net/net_codec.c`, `tests/net_test.c`
   and confirms the freeze.
3. [assets] works; [net] does not touch those three files during the window.

WHO WRITES THE VERSION CONSTANT - the point of this entry, so only one agent
ever edits `net.h`:
- **If design A wins:** `NET_ASSET_VERSION` lives in `net.h`, but the append
  commit is what invalidates it, so **[assets] writes that one line** in that
  commit. [net] does not touch `net.h` during the window.
- **If design B wins:** **[net] owns it all** - the wire bump 8 -> 9, the codec,
  the u16 count (B38) and the asset-version line, in one commit together with
  [assets]'s appended art table. Landed as a single commit.

FILE OWNERSHIP ESTABLISHED IN THE EXCHANGE:
- `relay_net_events()` / the relay list (`src/main.c:1396`) - [net]
- the drawing functions in `render.c` - [assets]/animations
- `system_cleanup()` - **read-only for everyone.** It emits
  `GE_KILL`/`GE_ENTITY_DEATH` and calls `rand()`, so it is inside the
  determinism gate.
- `main.c` frame loop - [net] had no queued edits and will announce in advance
  if that changes.
- the I5 fix in `system_render_mirror()` - done, [net] is not revisiting it.

B38 (snapshot count is a u8 against a 1024 limit, latent):
- If design B wins: fold the `u16` count into the same `NET_WIRE_VERSION` 8 -> 9.
  One bump, not two.
- If design A wins: lower the limit to 255 **with a loud guard**. Do not pay a
  version bump for a latent issue. **The guard matters more than the number.**

**Why:** net.h and the codec test are the two files the agents would collide in, and there is no merge to fall back on for a shared constant.
