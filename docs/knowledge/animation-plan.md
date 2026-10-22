# Sprite animation — plan

<!-- Project knowledge (Intercom-AI). Agents append entries; edit freely. -->

## 2026-10-13 14:20 UTC - note by assets: MIGRATED from docs/ANIMATION_PLAN.md (owner D7). Spike-first plan; nothing implemented.

Status: PLANNED — spike-first. Nothing here is implemented. Owner: the animation
agent ([animation-agent]). Branch `feature/animation-assets` in
`worktrees/animation` (owner D5); the owner cuts it, once `main` is clean and the
gate is green. Scope (owner D4, 2026-10-13): still in the PLANNING PHASE; starts
only after the other blockers clear and net+assets finish current work.

## 1. Goal and non-goals

**Goal.** Entities animate — a walking zombie walks, a hurt player flashes and
recovers, pickups bob and glow — **without changing a single byte of
single-player simulation output.**

**Non-goals.** No skeletal/rigged animation. No UI/menu animation. No tile or
water animation (separate idea in `FUTURE_IDEAS.md`). No gameplay consequence of
any frame: frame index is never read by movement, AI, collision, damage or wave
code.

That last point is the whole feature. Every gate here is blind to a visual defect
(`SYSTEM_DESIGN_RULES.md` §4), so a visual change is the cheapest kind to get
wrong without noticing. Animation must be provably invisible to the simulation.

## 2. Ground truth (verified at the time of writing)

- **Version numbers live in `src/net/net.h`** — read them there, never from prose
  ([versioning-decisions]). The snapshot entry already carries an appearance
  block: `uint8_t art`, `uint8_t size_q`, `uint8_t tint[3]`.
- `src/net/net_art.c` is a **shared source file** (one copy, both peers) mapping
  `art` id → texture path + `base_px`. Adding an id is a one-line append needing
  no *wire* bump but a `NET_ASSET_VERSION` bump — see [versioning-decisions].
- The host fills `art` from the entity's live `CSprite` via
  `asset_manager_name_of()` — a path→id reverse lookup that reports what it is
  actually drawing, including `NET_ART_NONE`.
- The client's per-kind size table, hardcoded colours and slot colour lookup were
  **deleted** in the I5 fix; there is no fallback to fall back to. That deletion
  is the proof the fix is real — do not reintroduce a guess.
- Texture and gate counts are declared once in [gate-baseline]; cite, never copy.

### The machinery already exists — and it is dormant

| Exists | Does not exist |
|---|---|
| `COMP_ANIMATION` (`ecs.h:33`) | anything that **adds** `COMP_ANIMATION` |
| `CAnimation` (`ecs.h:139`): `current_frame`, `frame_timer`, `frame_duration`, `total_frames` | any renderer that **reads** `current_frame` |
| storage `animations[ECS_MAX_ENTITIES]` (`ecs.h:173`) | any frame art — every entity has one static texture |
| `system_animation()` (`render.c:215`), advancing from `dt`, no RNG, no events | any notion of a frame on the wire |
| its call site at `main.c:817` | a client-side path of any kind |

Confirm before writing a line (do not take it from this topic):
`grep -rn 'COMP_ANIMATION' src/` and `grep -rn 'current_frame' src/`.

### The client has no local ECS

`render_only_client()`: a render-only client has an empty local ECS — the world
exists only in the snapshot mirror, interpolated by `net_mirror_sample()` from
two `NetEntitySnap`s. So `system_animation()` (which walks `game.ecs`) **does
nothing at all on a client**. A component-based design animates the host/SP
window and silently freezes the second — exactly how R13-I5 survived nine commits
(`I5_FIX.md` §2, [i5-unverified-on-screen]). The client's frame must arrive
through the I5 appearance block or something added next to it.

## 3. The open question the spike must answer

### B31 asked this, and was retracted

`docs/BUGS.md` carries **B31 — "Animation phase not replicated" → RETRACTED (no
code path)** (commit `e254aed`). The retraction was correct *only* because
nothing attached `COMP_ANIMATION`. **Your first attached component makes it
false.** Reopen B31 in `docs/BUGS.md` yourself rather than letting it be
rediscovered. "Did you reopen B31?" is the first review question.

The question: **who owns "which frame is this entity showing right now?"**

### Designs A / B / C — the A-vs-B home is [animation-transport]

Do not restate the designs here. In brief: **A** = frame travels as an art id (no
wire change, ~60-80 ids, 20 Hz aliasing risk); **B** = a phase field travels
(NET_WIRE_VERSION bump, smooth, ~15 ids, id-reuse caveat); **C** = the client
derives the frame itself — **already rejected twice over** (`I5_FIX.md` §3 and
`SYSTEM_DESIGN_RULES.md` §1: it invents state). The spike measures A vs B; B must
not be built speculatively. Full detail, the two deciding checks and the why:
[animation-transport].

### The frame-selection principle

A and B differ in *transport* but both must feed **one function that answers
"which frame?"** with two input providers: single-player uses the entity's local
state (no snapshot — `render()` runs off the ECS); the client uses the snapshot
appearance block. **Two independent code paths is a review failure** — that is a
second opinion about appearance, the exact I5 bug class.

### Swords (B40) — IN SCOPE (owner D1, 2026-10-13)

Sword blades are not replicated today, so a client cannot see another player's
swing. Owner ruling, three binding requirements:

1. **Must render on the CLIENT** — rides the snapshot mirror, must work on a
   `render_only_client()` with an empty ECS (Phase D).
2. **New `NET_ENT_SWORD` snapshot kind** (option 1): a first-class snapshot entity
   that **inherits `art` and `owner`** like the other projectiles, with **no
   client-side derivation** ([versioning-decisions] trap; `SYSTEM_DESIGN_RULES.md`
   §1 / B19-B23).
3. **The provision must be made for adaptation** (owner's words). The
   `NET_ENT_SWORD` entry must carry room for future per-sword attributes —
   starting with **attack speed**, and later additions — as an *additive* change,
   not a second wire redesign. Design the entry with an extensible/versioned
   attribute block (or explicitly reserved, versioned fields), not the bare
   minimum that renders today's swing. Exact encoding is `[net]`'s behind the
   freeze protocol ([netfile-ownership-and-freeze]); animation states the required
   fields **and** the extensibility requirement.

It rides the **same wire bump as B38** (the `u8` snapshot-count fix) — one bump,
not two. Do not derive a blade from the attacker; do not ship minimum fields and
"add attack speed later".

### What the spike decides

| Question | Measured by |
|---|---|
| Sheets vs individual frame files | Spike A — build time, diff noise, `--verify` cleanliness |
| A, B, or C | smoothness at 20 Hz vs the cost of a wire bump, written up as a table |
| How many art ids the design consumes | counted against the 255 `uint8_t` budget, headroom stated |
| Where the frame index lives | ECS component vs render-side table — Spike B |

**Write the verdict into §8-equivalent slots below before any implementation
branch starts** ([agent-rules] §8: commit the plan before implementing it).

## 4. Spikes — cheap, throwaway, in order

### Spike A — frame storage: sheets vs individual files

Build **one** animated zombie end to end, both ways: (1) a horizontal sprite
sheet sliced at load; (2) one PNG per frame, separate textures. Record: generator
LOC, files under `assets/textures/`, `gen_assets.py --verify` wall time, whether
`--verify` leaves `git status` clean in both, and what a one-frame change looks
like as `git diff --stat`.

- Both: filenames are the API ([agent-rules] §6). Either the slice metadata lives
  in the generator or each frame is a separate texture key — decide what must stay
  in sync when an artist renames a frame.
- Sheets specifically: `asset_manager_name_of()` takes a path and returns an id.
  If a frame lives inside a sheet, the reverse lookup must be able to name it
  (slice as a first-class asset, own key, own `base_px`) — or design A is
  unavailable for sheets and that alone decides §3. Check first; it can end the
  spike early.

### Spike B — prove animation is invisible to the gate

No new tick is needed (`system_animation` exists, is called at `main.c:817`,
emits no events, consumes no RNG). What is new is *attaching the component*.

1. Attach `COMP_ANIMATION` to one entity type as a deliberate no-op
   (`total_frames = 1`).
2. Build clean; run `ctest`, `zombie_tests`.
3. Two same-seed SP runs, `diff`-empty.
4. Wave 1 still matches [gate-baseline].

### Spike C — cost

Measure the draw-time and build-time cost of the chosen design; state it as a
number.

## 5. Phases

Each phase ends with a green gate and a commit; no phase starts on an unclean
parent. **Cut from the clean tip, never from a dirty tree.**

| Phase | Deliverable | Acceptance |
|---|---|---|
| **A. Spike** | the verdicts below filled in | frame-storage and wire design decided *before* code; **B31 reopened in `docs/BUGS.md` with the design answer** — **DONE 2026-10-22, see §8** |
| **B. Foundations** | attach `COMP_ANIMATION` to real entities; renderer reads `current_frame`; frame-selection function; generator support; art ids; `NET_ASSET_VERSION` bump | gate green; SP runs byte-identical; ids counted against budget |
| **C. Content** | zombie, player, pickup animations; missing-texture fallback of identical size and tint | `gen_assets.py --verify` clean; `sprite_tex()` NULL path renders a flat shape, not a hole |
| **D. Multiplayer** | the client path uses the *same* frame-selection function | no new client-side appearance opinion; `I5_FIX.md` §7 honoured; **works on a `render_only_client()` with an empty ECS** |
| **E. Verification** | two-window human check | recorded in `docs/PENDING_VERIFICATION.md` with what to look for |
| **F. Swords (B40)** | `NET_ENT_SWORD` kind; blade renders on the client; extensible attribute block provisioned for attack speed | client shows another player's swing; no derivation; attributes additive (§3 "Swords (B40)") |

> **Phase D is BLOCKED until a human has watched two windows.** I5 and B32-B35
> are fixed in code with gates green and **none proven on screen**. Designing
> client-side animation on a mirror path nobody has ever seen run is the exact
> assumption this project keeps getting burned by. **Do not open Phase D on a
> green gate alone** ([i5-unverified-on-screen]).

## 6. Acceptance for the feature as a whole

- [ ] `cmake --build build` — zero warnings under `-Werror`
- [ ] `ctest --test-dir build` — 3/3
- [ ] `./build/zombie_tests` — green
- [ ] `python3 tools/gen_assets.py --verify` — clean tree afterwards
- [ ] Two same-seed SP runs `diff`-empty; wave 1 exactly the string in [gate-baseline]
- [ ] Every new value classified per `SYSTEM_DESIGN_RULES.md` §2, in a comment, in the introducing commit
- [ ] `NET_ASSET_VERSION` bumped in the same commit as the asset addition ([versioning-decisions])
- [ ] `docs/FEATURES.md` **F15** written; `PENDING_VERIFICATION.md` updated
- [ ] **Human has watched it on screen.** Until then the status line reads `unproven`, not `done`

## 7. Risks

| Risk | Why it bites | Mitigation |
|---|---|---|
| Reintroducing a client-side appearance guess | the original I5 bug; the deletion that proved the fix is the thing to undo | one frame-selection function; Phase D review |
| Wire bump sneaking in outside a spike | five files, one codec test, a stale peer striding the wrong distance | spike decides first; a wire change is its own commit naming `NET_WIRE_VERSION` |
| Art id exhaustion | 255 ids, ~60-80 for frames under design A | count in the spike; state remaining headroom |
| Art ids appended without the `NET_ASSET_VERSION` bump | old peer resolves an unknown id to NULL and skips frames *silently* | [versioning-decisions]; [agent-rules] §6 |
| Entity-id reuse under design B | snapshot ids are raw ECS indices with no generation counter; a recycled id briefly wears a dead entity's phase | **moot - A won the spike**; the art id is host-authored per snapshot like every other appearance field |
| `base_px` mismatch | right sprite, wrong size, on a peer nobody sees | assert against the real PNG in `--verify` |
| Animation feeding back into sim | desync that only shows as a byte-diff | Spike B; `system_animation` takes no RNG, emits no events — keep it so; never reorder around `system_cleanup` |
| Working only on the host | the client's ECS is empty; the second window freezes, exactly like I5 | Phase D acceptance; design for `net_mirror_sample()` from the start |
| Waking dormant state nobody consumes | attaching `COMP_ANIMATION` with no renderer reading `current_frame` proves nothing | Phase B requires the renderer to consume the frame in the same series |
| 20 Hz aliasing (design A) | a walk cycle that stutters or runs backwards on the second window only | **measured 2026-10-22**: it stutters below ~15 fps (frames skipped) but never runs backwards; hard rule: frame period >= 67 ms (<= 15 fps), target 10 fps |
| Breaking `system_cleanup` while editing `render.c` | kills stop firing → audio and wave progression break, invisibly | [agent-rules] §2: `system_cleanup` is read-only |
| **Sword shipping with minimum fields only** | attack speed and later attributes force a second wire redesign (owner D1 required provision) | add `NET_ENT_SWORD` with an extensible/versioned attribute block |
| **Sword derived client-side** | the B19-B23 / I5 class | `NET_ENT_SWORD` is host-authored, inherits `art`/`owner`, no derivation |

## 8. Spike verdicts

*Filled in 2026-10-22 on `feature/animation-assets` from the measured spike.
The measurements live in [animation-transport] §"2026-10-22 18:30 UTC"; this
section is the summary. No implementation code landed with it.*

- **Frame storage: individual PNGs, one file per frame.** A sheet cannot be
  named by path - all 8 frames of a sheet share one path, so `asset_manager_name_of()`
  and `net_art_from_path()` return the same art id for all of them. Making a
  sheet work was measured by writing it: `net_art.h` +8/-1, `net_art.c` +39/-10,
  **one line in `src/net/net_snapshot.c` (net's file)**, `render.c` +5/-1, and
  `tests/tests.c` +5/-2 with the current art-table test failing 7 times until
  it is changed. Individual files need **no code change outside the art table**
  and **no client render change at all** - `system_render_mirror()` already
  resolves art id -> path -> texture.
- **Wire design: A.** The frame travels as the art id already in the snapshot.
  No new field, no `NET_WIRE_VERSION` bump (stays 8), no `NET_SNAP_ENTRY_BYTES`
  change (stays 35). **B is not built.** The measured aliasing does not force
  B; it forces a rate limit.
- **Art id budget:** 10 ids today, `uint8_t`, so 255 total. Design A consumes
  frames directly: 8 frames per cycle. A realistic content set - 3 cycles
  (walk / attack / death) for 4 animated types - is 96 ids, i.e. 106 of 255
  with **149 ids of headroom**. `NET_ART_COUNT` must stay <= 255.
- **Frame index location: the ECS** - `COMP_ANIMATION.current_frame`, owned by
  `system_animation()` (already exists, already called, no RNG, no events).
  Frame -> art id -> texture is a render-side table (the shared art table), not
  ECS state. Spike B: attaching `COMP_ANIMATION` to zombies left the SP events
  log byte-identical (125938 bytes / 1603 lines) and the suite at 1186/0, so
  the animation component is invisible to the gate.
- **Build cost:** the generator grows by one function over a shared frame
  builder (+24 lines for individual files, +30 for a sheet);
  `gen_assets.py --verify` stays at 0.11 s with a clean tree. The art table
  grows by one line per frame (8 lines for a walk cycle). The client grows by
  **nothing**: the art id it already receives is the frame.
- **HARD RULE, binding on Phase C content: the animation frame period must be
  >= 67 ms (<= 15 fps).** Measured over 8 s at 20 Hz snapshots: at 24 fps only
  6..7 of 8 frames reach the client per cycle (30 skipped steps), at 30 fps
  5..6 of 8 (68 skipped steps); at 15 fps and slower every cycle shows all 8
  frames with 0 skipped and 0 backwards steps. Target 10 fps - an 8-frame
  cycle is then 800 ms. The client can never see the cycle run *backwards*
  (0 backwards steps at every rate): it holds the last id it was given.
- **NT4 (sword): (b) procedural blade**, not frame-based sword art. The blade
  angle is continuous and unbounded (5.0 rad/s = 286 deg/s), so frames would
  need >= 12.7 fps - on the aliasing limit - and would change the
  single-player picture. The snapshot carries the angle (int16 centidegrees,
  wrapping at 180 deg) and the client draws the blade with the same
  `sprite_draw_blade()` path, interpolating wrap-safe between two host frames.
  Field list: kind + owner + art + size_q + tint + angle, plus the extensible
  attribute block (attack speed) owner D1 asked for. One wire bump with B38.

## 9. Why animations merge first

Animation is the larger and riskier change, it owns the render path the I5 fix
just landed in, and audio's only shared files are `main.c`, `systems.h`,
`CMakeLists.txt`. Audio rebases onto the settled render path ([audio-plan] §12,
[agent-rules] §9).

**Why:** Design A gives maximum host authority (the client draws what the host
drew — the property I5 protects) and needs no wire change, but two measurable
constraints can force B; the spike decides, not the argument. Swords must ship
with room for attack speed so the wire is not redesigned twice (owner D1).
