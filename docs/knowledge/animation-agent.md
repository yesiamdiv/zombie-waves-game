# Animations agent — orientation

<!-- Project knowledge (Intercom-AI). Agents append entries; edit freely. -->

## 2026-10-13 14:20 UTC - note by assets: MIGRATED from docs/ANIMATIONS_AGENT.md (owner D7). Start-here for the animations agent.

You are the **animations agent**. This topic is your orientation — read it top to
bottom before opening an editor. The plan of record is [animation-plan]; process
rules are [agent-rules].

**Mission, one line.** Make entities *move* on screen — walking zombies, hurt
flashes, bobbing pickups — **without changing one byte of single-player
simulation output**. **Swords (B40) are in scope** (owner D1): a swung blade must
appear on the client, as a host-authored `NET_ENT_SWORD` snapshot kind with no
client derivation and provision for future attributes — [animation-plan] §3,
"Swords (B40)".

**Phase (owner D4, 2026-10-13):** still in the PLANNING PHASE — nothing coded, no
branch cut. You are not being asked to make the game harder, faster or different:
if your change alters what a bot does in a headless run, it is wrong by
definition.

## Where you work

| | |
|---|---|
| Branch | `feature/animation-assets` (not yet created; the owner cuts it) |
| Worktree | `worktrees/animation` (owner D5 — cut in-checkout, once `main` is clean and the gate is green) |
| Plan of record | [animation-plan] |
| Process rules | [agent-rules] |

Only ever work in your own worktree. Never edit `main`, `worktrees/assets`,
`worktrees/multiplayer`, or another agent's branch ([worktree-layout]).

## Read this first, in this order

1. `AGENTS.md` (root) — standing instructions, worktree discipline, the gate.
   Binding always.
2. `docs/SYSTEM_DESIGN_RULES.md` — mandatory. §1 (the host is the only authority
   for anything a player can see), §2 (classify every new value), §3 (single-player
   invariance), §4 (no gate can see a visual defect), §6 (adding a component).
3. `docs/VERSIONING.md` — mandatory. §2 registry table, §5 agent rules, §6 the
   worked example of a five-file wire bump. Summary of the decisions:
   [versioning-decisions].
4. `docs/I5_FIX.md` — **read §3 before you design anything**: why "let the client
   agree by duplicating the host's rule" is the bug, not the fix. §7 lists things
   you must not do to the appearance block.
5. `docs/BUGS.md` — find **B31**, `RETRACTED (no code path)`, and understand why
   it was retracted (commit `e254aed`).
6. [animation-plan] — your plan. §2 ground truth, §3 the open design question, §4
   your spikes.
7. `docs/EXTENDING.md` — how textures, sprites, systems and the asset manager work.

## Ground truth you must not re-derive

- **The animation machinery already exists and is dormant.** `COMP_ANIMATION`,
  `CAnimation`, storage, `system_animation()`, and a call site at `main.c:817`.
  **Nothing attaches the component and nothing reads `current_frame`** — that is
  the whole gap. Confirm with grep before writing a line (do not take it from
  prose): `grep -rn 'COMP_ANIMATION' src/`, `grep -rn 'current_frame' src/`.
- **Your first attached component reopens B31.** Animation phase is not
  replicated, and on a client there is no local ECS to hold it anyway
  (`render_only_client()`). **Reopen B31 in `docs/BUGS.md` yourself.**
- **The client has no local ECS** — the world exists only in the snapshot mirror
  (`net_mirror_sample()`). A component-based design animates your window and
  freezes the second one, exactly how I5 survived nine commits
  ([i5-unverified-on-screen]).
- **I5 already put appearance on the wire** (`art`, `size_q`, `tint[3]`), and the
  client's guessing code was *deleted*. Do not add a client-side fallback; if the
  host did not send it, draw nothing.
- **Version numbers live in `src/net/net.h`** — read them there and in
  `VERSIONING.md` §2, never from prose, including this topic
  ([versioning-decisions]). Texture and gate counts: [gate-baseline].

## Files

**Yours:** `src/graphics/sprite.*`, `src/graphics/anim.*` (new),
`tools/gen_assets.py`, `assets/textures/**`, plus the *drawing* functions in
`src/systems/render.c` (`system_render`, `system_render_mirror`,
`system_render_beacons`, `system_animation`).

**Not yours — read-only:** `system_cleanup` in that same file. It emits
`GE_KILL`, awards kills, spawns items and calls `rand()`; it is part of the
determinism gate and the audio agent's triggers are born inside it. Moving a line
breaks two systems at once ([agent-rules] §2).

**Shared, expect conflicts:** `src/main.c`, `src/systems/systems.h`,
`CMakeLists.txt`, `src/net/net.h` (version registry), and the append-only docs.

**Never:** anything under `src/net/**` **except** a one-line append in
`net_art.h`/`net_art.c` to register a new art id, plus the matching
`NET_ASSET_VERSION` bump in `net.h` — appends need no **wire** bump but do need
the **asset** one ([versioning-decisions]; a stale peer resolving an unknown id
skips frames silently). `net.h` is shared: read first, announce over Intercom
(`intercom_send`) before you write it. Nothing else; if you think you need it,
read `I5_FIX.md` §7 and raise it in [animation-plan]. Never
`playtesting_prompts/**`. Full matrix: [agent-rules] §2.

## Your first task

**Spike, not code.** Fill in [animation-plan] §8:

1. **Frame storage** — build one animated zombie both ways (sheet vs individual
   frames), measure, pick. Watch the constraint in [animation-plan] §4 Spike A: if
   a frame lives inside a sheet, `asset_manager_name_of()` cannot name it, and
   that alone can decide the design.
2. **Prove animation is invisible to the gate** — add the component as a no-op
   stub, confirm two same-seed runs are `diff`-empty, then advance a real frame
   and confirm again.
3. **Settle the A-vs-B design** — the home for that question is
   [animation-transport]. C is already rejected; the choice is A vs B, and A has
   an aliasing risk you must *measure*, not argue.

Then commit the verdict **before** writing implementation code ([agent-rules] §8).

## Definition of done

```
cmake --build build                    # zero warnings, -Werror
ctest --test-dir build                 # 3/3
./build/zombie_tests                   # green
python3 tools/gen_assets.py --verify   # clean tree after
./build/open_world_zombie_waves --headless --ai=bot --run-seconds=25 \
    --events=a.log --seed=42           # x2, diff-empty
```

Wave 1 must read exactly what [gate-baseline] records (that topic is the single
declaration of every gate number — cite it, never copy it). Plus:
`docs/FEATURES.md` **F15** written, `PENDING_VERIFICATION.md` lists what a human
must *look* at, and **B31 reopened with your design answer**.

## The ways this fails

- **It works in your window and freezes in theirs.** Your component never reaches
  the mirror. (This is I5 — the failure to design against.)
- **Two ways to answer "which frame?"** One function, two input providers. Two
  functions is a review failure.
- **The frame leaks into the simulation** — an animation timer read by AI or
  collision, or a `rand()` for frame variation. Desync, invisible until the
  byte-diff fails.
- **A wire bump slipped in.** Five files, one codec test, a stale peer. It is a
  separate deliberate commit that names `NET_WIRE_VERSION` — or it is not allowed.
- **You "fixed" `system_cleanup`** because it looked like render code.
- **You write `fixed` instead of `unproven`.** No gate can see your work; a human
  watching two windows is the proof.

## How to work

One commit per unit. Keep the todo list current. Record findings in
`docs/BUGS.md` / `docs/PROGRESS.md` **after** the commit exists — chat is not the
record. Rebase onto `main` at the end of every phase, not when it hurts. Report
in PM/standup form: planned / done / next / blocked.
