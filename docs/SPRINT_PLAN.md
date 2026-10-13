# Sprint 4 — Unblock, Record, Enable Animation/Audio

PM: planning/scope/board | DEV: implementation | Status: ACTIVE — owner resolved
D1-D4 and D6-D9 on 2026-10-13; only **D5** (animation/audio worktree+branch
naming) is held. NT6 + AT8 done; NT1 unblocked; animation/audio still in the
planning phase.
Raised: 2026-10-09 (PM onboarding; replaces Sprint 3, which is closed — see
`docs/PROGRESS.md` for its FIXED tickets).

Source of truth for scope: the two KT answers of 2026-10-09. **Every task below
was named by the developer who owns it.** The PM invented none of them.

---

## 0. Sprint ceremony / reporting structure

- **Planning (PM)**: pick ticket scope, accept/estimate, record here.
- **Daily standup log**: `docs/PROGRESS.md` (found / done / next / blocked).
- **Review (PM)**: verify build + tests + playtest; mark tickets FIXED.
- **Board**: the Intercom task board (`intercom_corp_tasks`), mirrored in §3.
- **Decisions**: `docs/knowledge/` — cite the topic, never copy its numbers.

## 1. Goals

1. Unblock both devs: land the three owner scope calls and record them.
2. Commit the animation/audio planning set and cut the two missing branches.
3. Land the two free net items (`GE_WAVE_END`, the `/tmp` scratch-path fix).
4. Run the animation and audio Phase-0 spikes and record their verdicts.
5. Keep the single-player byte-identity gate green after every single ticket.

## 2. Owner decision queue — resolved 2026-10-13

Carried 2026-10-09; owner answered 2026-10-13. Recorded in
`knowledge/open-decisions-awaiting-pm`; only D5 remains open.

| # | Decision | Blocks | Owner's call |
|---|---|---|---|
| D1 | Swords (B40) in the animation sprint at all? | NT4 | **IN SCOPE.** Must render on the client for other players; option 1 `NET_ENT_SWORD`; **must provision for future attributes (attack speed etc.)**. |
| D2 | `GE_WAVE_END` (B39) go — free, no wire bump, no SP-output change. | NT1 | **GO.** |
| D3 | `GE_EXPLOSION` go — new `GameEventType`; **re-baselines the SP events log**; rides the audio commit and is re-verified jointly. | NT2, AT7 | **GO** (features first, re-baseline as needed). |
| D4 | Are animation and audio still planned? | AT1-AT7 | **PLANNED, still in the PLANNING PHASE** (nothing coded). Starts only after the other blockers clear and net+assets finish current work. |
| D5 | Branch + worktree names/location for animation & audio. | AT2 | **RESOLVED** — `animation`→`worktrees/animation`/`feature/animation-assets`; `audio`→`worktrees/audio`/`feature/audio`; animation first, audio rebased. Owner cuts them via `git worktree add -b`. Not `--create-worktree`. |
| D6 | Separate hired agents, or tasks on `assets`? | AT2-AT7 | Hire later, after clutter clears + planning done. PM recommends separate `animation` and `audio` agents. Seated chats in main's OpenCode, NOT per-agent OpenCode. |
| D7 | Where do the planning docs live? | AT1 | **RESOLVED** — `docs/knowledge/` is the docs system of record; migrate the planning set into topics. See `knowledge/docs-system`. |
| D8 | net's worktree identity / push. | net's lane | **RESOLVED** — cwd is always main; the lane is a worktree inside it. net lands its main work, declares it, then switches; **no push without owner's go.** See `knowledge/worktree-layout`. |
| D9 | Commit `docs/SPRINT_PLAN.md` to main? | PM | Left to PM judgement. |

Also flagged by the owner: `feature/assets-maps` is 96 commits ahead of its
remote, untouched since 2026-10-06 — **do not push it.**

Not owner decisions — evidence-gated, tracked so they cannot be defaulted on:

- **Animation transport A vs B** (`knowledge/animation-transport`): parked on the
  spike's two measurable checks. Lean A. **Nobody builds B speculatively.**
- **Reload audio**: the audio spike answers it. No `NetPlayerState` bit is built
  on speculation.

Hard-blocked on a human:

- **Animation Phase D / B32-B35 screen check**: a green gate is not sufficient.
  See `knowledge/i5-unverified-on-screen`. Once all blockers are done, the PM
  tells an agent to run two windows so the **owner** can clear it.

## 3. Board

Statuses are the board's; a task is only assigned once its owner confirms it.

### net — netcode lane (`worktrees/multiplayer`, `feature/net-protocol`)

| ID | Task | Depends on | Status |
|---|---|---|---|
| NT1 | Relay `GE_WAVE_END` (B39): filter add in `relay_net_events()` + `BUGS.md` close-out. Free — no wire bump, relay early-returns in SP. | D2 ✔ | **DONE `feature/net-protocol@e94eb0e`** |
| NT2 | Add `GE_EXPLOSION` (new `GameEventType` + `event_type_names[GE_COUNT]` + emitters) **with** the SP events-log re-baseline, jointly re-verified with `assets`. | D3 ✔, audio start | **UNBLOCKED (D3 GO)** but rides AT7 — after audio starts |
| NT3 | B38 snapshot count: if design B, fold a `u16` into the `NET_WIRE_VERSION` 8→9 bump; if design A, lower `NET_SNAP_MAX_ENTITIES` to 255 **with a loud encoder guard**. | A-vs-B spike | blocked (measurement) |
| NT4 | B40 swords: new `NET_ENT_SWORD` snapshot kind (option 1), inherits `art`/`owner`, no derivation; **must render on the client for other players and provision for future attributes (attack speed etc.)**. Rides NT3's bump if B. | D1 ✔, animation start | **IN SCOPE (D1)** — rides the animation lane (D4 planning phase) |
| NT5 | Freeze `src/net/net.h`, `src/net/net_codec.c`, `tests/net_test.c` when `assets` posts "starting"; confirm the freeze. | assets starts | blocked (external) |
| NT6 | Purge `/tmp/opencode` scratch paths from `tests/tests.c` (tmpfs quota hazard); move to a build-tree temp path. | — | **DONE `f260c55`** (gate re-run by net) |
| NT7 | B32–B35 human two-window screen check. | a human | blocked (human) |

### assets — animation/audio lane (`worktrees/assets`, `feature/assets-maps`)

| ID | Task | Depends on | Status |
|---|---|---|---|
| AT1 | Migrate the planning set (`AGENT_RULES`, `ANIMATION_PLAN`, `AUDIO_PLAN`, `ANIMATIONS_AGENT`, `AUDIO_AGENT`) **into `docs/knowledge/` topics** (D7). Names now known (D5): animation→`feature/animation-assets`, audio→`feature/audio`. | main clean | **READY** |
| AT2 | Owner cuts `worktrees/animation`+`feature/animation-assets` and `worktrees/audio`+`feature/audio` by hand (`git worktree add -b`), animation first. **Hard precondition: main committed/clean + gate green** — cut from the commit, never a dirty tree. | main clean, AT1 | blocked (owner onboarding) |
| AT3 | Animation Phase-0 spike: can a sheet frame be *named*? is 20 Hz aliasing acceptable? → verdict into `knowledge/animation-transport`, then A-vs-B. | AT2 | blocked |
| AT4 | Animation Phases A–C, strictly downstream of the snapshot (no RNG, no sim feedback). **Design-A only:** the ~60–80 `net_art` appends + `NET_ASSET_VERSION` 1→2 apply only if the spike picks A; the append touches the frozen `net.h`/`net_art.*` so it goes through the freeze protocol with `net`. | AT3 | blocked |
| AT5 | Animation Phase D — **blocked on a human two-window I5 check**, not on a gate. | human | blocked (human) |
| AT6 | Audio Phase-0 spike: vendor miniaudio; soft-fail headless; trigger audit; no-double-fire counting log; reload; gunfire latency (AUDIO_PLAN §4/§5) → verdict into `knowledge/`. | AT2 | blocked |
| AT7 | Audio phases; `GE_EXPLOSION` SP re-baseline rides this commit and is re-verified jointly with `net`. | AT6, D3 | blocked |
| AT8 | Stale-path sweep. **DONE.** assets: `ASSET_MAP_PLAN.md:81` (`243630a`) + `CROSS_BRANCH_CONFLICTS.md:211` (untracked, rides the D7 docs commit). net: `MULTIPLAYER_PROGRESS.md` + `R13_BUGFIX_TRACKER.md` (`362e60e`). `BUGS.md:721,791,819,839` **left as history** (PM call, `knowledge/worktree-layout`); the `2026-09-18` creation command kept as annotated history. `AGENTS.md:151,154` + `:57-60` still held for D9; `*_AGENT.md` placeholders for D5. | — | **DONE `362e60e`** |

### shared

| ID | Task | Owner | Status |
|---|---|---|---|
| SH1 | Every phase transition re-runs the full gate; if `GE_EXPLOSION` lands, `net` and `assets` re-verify the SP baseline **together**. | both | standing |
| SH2 | Any new decision lands in `docs/knowledge/` in the same commit as it matters. | both | standing |

## 4. Definition of done (all tickets)

1. `cmake --build build -G Ninja` — zero warnings in project sources.
2. `ctest --test-dir build` — 3/3.
3. `./build/zombie_tests` green (numbers: `knowledge/gate-baseline`).
4. `python3 tools/gen_assets.py --verify` when assets were touched.
5. Two same-seed SP runs, diff-empty, wave 1 unchanged.
6. One commit per fix, decision written down **before** the code.
7. `docs/BUGS.md` row updated with the fix SHA; `docs/PROGRESS.md` standup row added.

**A change that cannot pass the gate is not ready to commit** — including when
it is visual or audible and the gate is headless. Then the row says `unproven`,
not `fixed`, until a human has looked or listened.

## 5. Not in scope this sprint

Particles/effects replication (B29 stays accepted), client zero-latency gunshot
feedback (option (c), optional later), `NET_AUDIO_VERSION` (agreed: no such
number), any speculative design-B wire work, reloading the old sibling-worktree
paths.
