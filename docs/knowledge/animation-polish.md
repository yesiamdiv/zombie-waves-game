# Animation polish — what was approved, in what shape, and where it lives

<!-- Project knowledge (Intercom-AI). Agents append entries; edit freely. -->

## 2026-10-27 10:32 UTC - decision by pm: P1 is DISPLAY-ONLY (option A); P3 allowed as cosmetic; P4 is assets'

Owner ranked P1-P5 DO NOW (2026-10-27). pm resolved the one ambiguity that
could have broken the non-negotiable gate:

**P1 "stagger via late spawns from server" means VISUAL staggering, not delayed
spawning.** Option B (real staggered spawns) is rejected: it would move
`rand()` order (variant/size draws happen at different ticks), change wave
timing, move the SP events log off 125938 bytes / 1603 lines, and force a
re-baseline of every doc that cites it. Not worth it.

Approved shape of each item:

| Item | Approved shape | Sim impact |
|---|---|---|
| P1a walk-phase stagger | Each zombie's `current_frame` starts at `hash(entity id) % ANIM_WALK_FRAMES`. Spawn tick unchanged, period unchanged, so the offset persists forever. | none - the frame is display and travels as the art id |
| P1b spawn-in | Purely visual fade/scale-in for ~0.15 s while collider, health and AI are live from tick 0. **No new art, no art ids, no `NET_ASSET_VERSION` bump.** New art would need a separate decision. | none |
| P2 item bob phase | `CItemTag.bob_timer` starts at a deterministic hash offset instead of 0, so drops are not all in phase. | none - the bob was already display-only via `base_pos` (AN5) |
| P3 muzzle flash | **Allowed by pm as cosmetic** under `SYSTEM_DESIGN_RULES` §"cosmetic and deterministic": driven by the already-relayed `GE_PLAYER_SHOT` (B37), fixed parameters, no `rand()`, host and guest both draw it, **procedural/additive - no PNG**, so no asset or wire bump. | none |
| P5 sword trail | Render-only trail from the recent blade angles (host has the angle, client has the mirrored angle). | none |
| P4 player damage feedback | **NOT animation.** pm routed it to assets (HUD/effects, E1-E6) to avoid two damage-feedback systems. Animation must not duplicate it. | n/a |

**Hard constraints that shaped the above** (all verified against the tree):

- `rand()` is order-sensitive: the death burst already draws from it
  (`src/systems/render.c`), so any new `rand()` call anywhere shifts the seed
  sequence and moves the SP log. Every polish item is rand-free by design.
- The frame is replicated as the art id (`net_snapshot.c` resolves the live
  sprite texture through `net_art_from_path`), so display-only frame changes
  reach the client with no wire and no client-side computation.
- Particles and swords are deliberately **not** replicated (`net_snapshot.c`),
  so effects are drawn locally by each peer from the same event stream.

**Placement rule (the "animation systems architecture" question):** animation
*state* that other code reads lives in components and is advanced by
`src/systems/animation.c`; anything that only affects pixels lives in
`src/systems/render.c`. Effects are not new systems: they are render-side
consumers. When pm's `docs/EVENT_LISTENER_ARCHITECTURE.md` registry lands,
effect listeners register through it rather than adding another event scan.

**Gate obligations per commit:** build clean `-Werror`, ctest 3/3,
`zombie_tests` green, `--verify` texture count unchanged, two `--seed=42` SP
runs byte-identical (125938 / 1603). "Purely visual" is never an exemption -
it is the reason the log must stay byte-identical.

**Unproven until a human looks:** the staggered horde, the spawn-in feel, the
bob de-sync, the flash and the trail (see `PENDING_VERIFICATION.md`).
