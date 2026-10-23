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

# Sprint 5 — Sync + Playtest — CLOSED 2026-10-15

Owner directive 2026-10-14: (1) put the net and assets worktree lanes on the
**same commit** first; (2) then a **QA-led playtest** of the features not tested
recently, with net and assets **guiding qa** on what/how to test; (3) then free
net and assets for the rest of the day. Meet once this sprint is done. Sprint 4
below is closed; its board is history.

State at sprint raise: `main@84c7263`; `feature/net-protocol@e94eb0e` (NT1/B39, 1
ahead); `feature/assets-maps@c2201dc` (behind, 0 ahead). Priority 1 complete;
gate green.

## S5 board

| ID | Task | Owner | Depends on | Status |
|---|---|---|---|---|
| S5-1 | Align both lane worktrees onto one commit (land NT1→main, converge). Acceptance: identical `git rev-parse HEAD` in both worktrees == main; trees clean; gate green. | net (+coord assets) | — | **DONE** — all three at `10fd6005`, trees clean, gate green. |
| S5-2 | Bring `worktrees/assets` onto that same commit (lane is contained in main; likely ff). | assets | — | **DONE** — clean ff to `10fd6005`. |
| S5-3 | Write the multiplayer QA test guide (B32-B35 screen check, I5, PENDING_VERIFICATION items) and hand to qa. | net | — | **DONE** `069d4d0` — `docs/QA_MULTIPLAYER_S5.md` |
| S5-4 | Write the content/animation QA test guide (what to look at, how to reach it, Phase D/I5 expectations) and hand to qa. | assets | — | **DONE** `8f4533a` — `playtesting_prompts/qa_recheck/04_content_animation_checks.md` |
| S5-5 | Execute the guided playtest on the aligned commit; record findings; report to PM. | qa | S5-3, S5-4 | **DONE** — gate PASS; findings `qa_recheck/findings/S5_SUMMARY.md` (untracked) |
| S5-6 | After S5-1/S5-2 land and the guides are handed over: free net + assets for the day. | PM | S5-1..S5-4 | **DONE** — both freed |
| S5-7 | Owner calls on the push + lane drift (2026-10-15): push is OK once a feature is completed/tested (see `knowledge/git-push-policy`); everyone must sit on one common commit. | owner | S5-3 | **DONE** — main + both lanes all at `59c71af` (common point); policy announced. |
| S5-8 | Repo-root cleanup (owner: stray files): remove tracked one-off `fix_ecs_csordtag.py`, delete stray `session-ses_*.md` + ignore, move runtime logs to `build/logs/`, relocate `PM_MASTER_PROMPT.md`. | PM | — | **DONE** `d19a29b` (pending owner call on `agents/` + qa findings) |
| S5-9 | Migrate the net replication-surface inventory into knowledge, then delete the retired `agents/` dir and fix its refs (owner call). | assets | — | **DONE** `8c2e9af` |
| SH2 | Any new decision lands in `docs/knowledge/` in the same commit as it matters. | both | — | standing |

**Out of scope this sprint:** animation/audio coding, the A-vs-B spike, cutting
`worktrees/animation`/`worktrees/audio` (owner's gate), any wire/version bump.

## S5 close — results (2026-10-15)

- **Lanes on one common point:** main + `worktrees/multiplayer` + `worktrees/assets` were aligned; docs commits since then (S5-8/S5-9/policy) drift them by design — absorbed by a re-align at the next stage's start (`knowledge/worktree-layout`).
- **Playtest (S5-5):** headless gate PASS at the test base `069d4d0` (build `-Werror`, ctest 3/3, `zombie_tests` 1186/0, 33 textures, SP seed-42 byte-identical). Headless co-op: host spawned the slot-1 remote entity, client connected, snapshots streamed, no crash. **No new bugs.**
- **Still UNPROVEN on-screen (needs a human):** map select / `--map` loading, I5 client sprites-vs-dots, beacons/HUD, B32-B35. A green gate is silent on these — see `knowledge/i5-unverified-on-screen` and `docs/PENDING_VERIFICATION.md`. This is the meeting's human-eye checklist.
- **Policy changes this sprint:** push allowed for completed+tested work (`knowledge/git-push-policy`); qa findings stay untracked, durable bugs go to tracked docs (`knowledge/qa-findings-policy`); re-align lanes at stage start.
- **Cleanup:** repo root tidied (`d19a29b`); retired `agents/` dir deleted after its decisions were confirmed captured (`8c2e9af`).
- **Standing by for the owner's next stage:** onboarding `animation`/`audio` (worktrees cut by hand) after the human screen check.

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

**Priority (owner, 2026-10-13):** each agent finishes its **current/established**
work first. **PRIORITY 1 = current work; NEXT STAGE = animation/audio and
everything that rides it, not started until Priority 1 is done and main is
clean + gate green.** Confirmed with both devs: net's Priority 1 is effectively
done (only NT3, spike-gated); assets' Priority 1 was AT1 alone. **PRIORITY 1 is
now COMPLETE team-wide (`a25d9ba`) — main tracked tree clean + gate green; the
next stage opens on the owner's go.**

### net — netcode lane (`worktrees/multiplayer`, `feature/net-protocol`)

| ID | Task | Depends on | Priority / Status |
|---|---|---|---|
| NT1 | Relay `GE_WAVE_END` (B39): filter add in `relay_net_events()` + `BUGS.md` close-out. Free — no wire bump, relay early-returns in SP. | D2 ✔ | P1 · **DONE `feature/net-protocol@e94eb0e`** |
| NT2 | Add `GE_EXPLOSION` (new `GameEventType` + `event_type_names[GE_COUNT]` + emitters). **Measured correction (net, 2026-10-22):** the seed-42 25 s bot run does **not** move — the bot never detonates an explosive — so the SP events-log reference is unchanged (125938/1603); only the test count moves (1186→1194). "Re-baseline" therefore applies only to runs that actually use explosives. | D3 ✔, audio start | **DONE / LANDED on main `6801d7c`** (`8f408b7` + `cd7e8ea`: rocket blast at first contact) |
| NT3 | B38 snapshot count. **UNPARKED 2026-10-22:** the spike chose design A (no animation wire change), but NT4 needs a field — so open **one** wire-bump window and fold the entity count to `u16` **together with** NT4's fields in a single `NET_WIRE_VERSION` 8→9 bump (one bump, per D1). Codec test + version declaration in the same commit. | A-vs-B ✔ (A), NT4 | **DONE / LANDED on main `6801d7c`** — `u16` count, cap 1024 kept, loud reject, `test_net_snapshot_u16_count` (300 entities); B38 FIXED; merged gate 1808/0 |
| NT4 | B40 swords: new `NET_ENT_SWORD` snapshot kind, inherits `art`/`owner`, no derivation; **must render on the client for other players and provision for future attributes (attack speed etc.)**. Verdict: **procedural blade** — snapshot carries `int16` centidegrees angle (wraps at 180°) + extensible attribute block; client draws via `sprite_draw_blade()` with wrap-safe interpolation. **Rides the single 8→9 bump with NT3.** | D1 ✔, AN7 ✔, bump | **DONE / LANDED on main `6801d7c`** (`31c8d8f`; builder test + stale-attr/O(1)-fold). AN7 confirmed id 9/base 40. Merged gate green; **on-screen mirrored-sword check still unproven (human)**. |
| NT5 | Freeze `src/net/net.h`, `src/net/net_codec.c`, `tests/net_test.c` when `assets` posts "starting"; confirm the freeze. | assets starts | **DONE** — froze for AN1, clean at start, released after `35a8b46`; netcode files untouched by animation |
| NT6 | Purge `/tmp/opencode` scratch paths from `tests/tests.c` (tmpfs quota hazard). | — | P1 · **DONE `f260c55`** |
| NT7 | B32–B35 human two-window screen check on `feature/net-protocol`. | a human | P1 verification debt · blocked (human) |

### animation + audio lanes (new hires, cut 2026-10-22)

`animation` owns `worktrees/animation` / `feature/animation-assets`; `audio` owns
`worktrees/audio` / `feature/audio`. Both branches were cut from main `68280a3`.
Orientation: `knowledge/animation-agent`, `knowledge/audio-agent`. Animation is
first; audio rebases onto `feature/animation-assets` before landing (D5).

| ID | Task | Depends on | Priority / Status |
|---|---|---|---|
| AT1 | Migrate the planning set (`AGENT_RULES`, `ANIMATION_PLAN`, `AUDIO_PLAN`, `ANIMATIONS_AGENT`, `AUDIO_AGENT` + `CROSS_BRANCH_CONFLICTS`) **into `docs/knowledge/` topics** (D7). | main clean | P1 · **DONE `a25d9ba`** — main tracked tree clean, gate green |
| AT2 | Owner cuts `worktrees/animation`+`feature/animation-assets` and `worktrees/audio`+`feature/audio` by hand (`git worktree add -b`), animation first. Precondition: main committed/clean + gate green. | main clean, AT1 | **DONE 2026-10-22** — both cut at `68280a3` |
| AT3 | Animation Phase-0 spike: can a sheet frame be *named*? is 20 Hz aliasing acceptable? → verdict into `knowledge/animation-transport`, then A-vs-B. | AT2 | **DONE `7ee1e16`** — verdict: **DESIGN A** (individual PNG frames), period ≥ 67 ms (≤15 fps, target 10); sheet frames unnamable; gate blind to animation; ≤15 fps = 8/8 frames. No wire change (`NET_WIRE_VERSION` 8, entry 35). B31 reopened. |
| AT4 | ~~Animation Phases A–C~~ — **superseded** by animation's proposed AN1–AN7. | AT3 | cancelled → AN1–AN7 |
| AT5 | ~~Animation Phase D~~ — **superseded** by AN6 (client path, human-gated). | human | cancelled → AN6 |
| AN1 | Art table append: 8 walk-frame ids (art table 10→18) + `NET_ASSET_VERSION` 1→2 in one commit. **Needs net's freeze handshake.** | AN2, net freeze | **DONE `35a8b46`** — ids 10–17, `NET_ASSET_VERSION` 2, wire still v8/35; tests 1186→1253; freeze released to net |
| AN2 | Generator support for one walk cycle (8 individual PNGs; +24 LOC; `--verify` checks them). Can start now. | AT3 | **DONE `18c95af`** — `zombie_walk0..7.png`, frame 0 == idle, 8 pairwise distinct; **textures 33→41** re-baselined in `gate-baseline` on the animation lane |
| AN3 | Frame-selection function + host wiring (one function; ECS keeps `current_frame`). Can start now. | AT3 | **DONE `e45304c`** — `src/graphics/anim.*` + `src/systems/animation.c`; frame texture on the sprite so host/client share one answer; wire untouched; tests 1253→1347; walk cycle on-screen unproven |
| AN4 | Rate rule as code (≤15 fps named constant + failing test). | AN3 | **DONE `bbf4805`** — `ANIM_MAX_FPS`/`ANIM_FRAME_PERIOD_MIN`, `test_animation_rate_rule` simulates the 20 Hz snapshot client; teeth proven by breaking it; tests 1347→1382 |
| AN5 | Content: walk/attack/death states + pickup bob, no wire change (player walk split out to AN8). **No death pose** — a corpse would delay destruction and move the death timestamp, breaking SP invariance (gameplay item). | AN2, AN3 | **DONE `e5d3f8a`** — landed with AN8 on main `3648856` |
| AN6 | Phase D client path — same selection function; **blocked on human two-window I5 check**. | human | animation · blocked (human) |
| AN7 | NT4 animation-side support: sword art ids + angle contract (procedural blade, int16 centidegrees) for net. | AN1 | **DONE `275a9af`** — v9 had already consumed the art (id 9/base 40), so only the contract + geometry check; no code, no `NET_ASSET_VERSION` bump |
| AN8 | **Player walk** (future piece): needs new art ids + a `NET_ASSET_VERSION` bump, so it is a separate commit after AN5 and a net freeze window. | AN5, net freeze | **DONE `13621b8`/`e4f730f`/`629d4ad`** — player_walk0..7 (18–25), ASSET 3, COMP_ANIMATION on spawn; frame 0 == idle; tests 1721→1759 on lane; M11 unproven; **LANDED on main `3648856`** |
| AT6 | ~~Audio Phase-0 spike~~ — **superseded** by audio's own proposed breakdown A1–A6 (owner rule: build tasks from the agent's proposal). | AT2 | cancelled → A1–A6 |
| A1 | Audio backend decision, measured: vendored miniaudio vs **SDL3's own audio API** (already linked); compile/binary/device cost, sound proven on the speakers (pw-record). Verdict may change the plan of record. | AT2 | **DONE `8f3e4c3`** — verdict **SDL3 audio API**: 0 B binary / 0 s build / no new dep vs miniaudio 4.1 MB header + 19 s compile + 646–884 KB + `-ldl -lpthread`; both reach the speakers (tone captured). Cost decision: we own ~150–250 lines of mixing. |
| A2 | Soft-fail proof for the chosen backend (no device / missing file / `--headless`). | A1 | **DONE `8f3e4c3`** — proven at probe level (both backends, exit 0, no abort); game-level `--headless` still a Phase-B acceptance item |
| A3 | Trigger audit: one row per intended sound, read from code, host-authoritative vs client-derivable. | A1 | **DONE `8f3e4c3`** — table in `audio-plan` §6; client can't tell who-hit (`ib` dropped) or hurt-vs-heal (`ia` dropped); rocket impact needs NT2; shop/spawn not relayed → host-only; no reload mechanic |
| A4 | Double-fire audit: emission counts per event type, host vs client, 25 s co-op. | A1 | **DONE `8f3e4c3`** — ruled out by counting: every relayed type at ratio 1.00 in two co-op runs (25 s headless, 18 s windowed), ids one-for-one |
| A5 | Gunfire latency (audible?) + reload-on-wire evidence. | A1 | **DONE `8f3e4c3`** — relay ~1 ms; delay is frame-bound (one host + one client frame): ~90 ms median at ~25 FPS, ~33 ms at 60 FPS; no reload exists |
| A6 | Commit spike verdicts into `audio-plan` §10 + measurements topic; gate first. | A2–A5 | **DONE `8f3e4c3`** — `audio-plan` §10, `knowledge/audio-measurements` (on lane), listen-for list, PROGRESS; no product code |
| A7 | **Phase B** — audio foundations on the SDL3 backend: `src/audio/`, init/update/shutdown in `main.c`, `AUDIO_TICK` between `relay_net_events()` and `event_bus_flush()`. | A1 | **DONE `ecf7a2f`/`e5c9c54`** — SDL public API path, pure mixer, trigger ring, unit tests (1207/0); headless silent, allocation-free, unproven by ear |
| A8 | **Phase C** — `tools/gen_audio.py` + `assets/audio/**` + the sounds; deterministic regeneration. | A7 | **DONE `9d4e592`/`a98b0bc`** — stdlib 48 kHz WAV generator, 13 slots; mapping still a stub; unproven by ear |
| A9 | **Phase D** — consume relayed triggers; own gunfire = **option (a)** (PM decision `knowledge/audio-decisions`); `GE_EXPLOSION` from NT2. Adds no wire field, edits no relay list. | A8, NT2 | **UNBLOCKED** — NT2 now in main `6801d7c`; in progress; lands after assets |
| A10 | **Phase E** — human listens, two windows; judge whether the client's own gunshot feels late. | A9 | audio · blocked (human) |
| AT7 | NT2 (`GE_EXPLOSION`) landing window, re-verified jointly with `net`. Measured correction: NT2 does not move the seed-42 SP reference (bot never detonates); only test count 1186→1194. | A6, D3 | **DONE** — NT2 landed `6801d7c`; SP ref unchanged; merged gate 1808/0 |
| AT8 | Stale-path sweep. **DONE.** assets: `ASSET_MAP_PLAN.md:81` (`243630a`) + `CROSS_BRANCH_CONFLICTS.md:211`. net: `MULTIPLAYER_PROGRESS.md` + `R13_BUGFIX_TRACKER.md` (`362e60e`). `BUGS.md` historical refs **left as history**. | — | P1 · **DONE `362e60e`** |
| AT9 | Fullscreen toggle (F11/Alt+Enter → fullscreen-desktop, restore size on exit). Owner request 2026-10-22. Programmatic window moving is NOT possible under Wayland; do not attempt it. Board: `task_01a163659bb91efec271f487`. | assets | **DONE + VERIFIED `c01c0fc`** — toggle in `main.c`, hint in `hud.c`, size captured/restored, repeat-guarded; owner confirmed 2026-10-23 |
| AT10 | **B45** damage-vignette refinement: replace the banded real-time fill with a baked radial-alpha texture (alpha 0 centre → max at edges from ~40% radius), drawn full-screen, fading on the existing timer, reset on hit. Decision: `knowledge/ui-damage-vignette`. Board: `task_01a16382656225660a3f39fe`. | assets | **DONE + VERIFIED `f7e2556`** — `ui/vignette.png`; owner liked it 2026-10-23 |
| AT11 | **B43** dropped-loot sprites: transparent PNG areas render white + items too small; fix alpha/blend, enlarge, and recheck all other assets. Board: `task_01a16382322209bde0f58c00`. | assets | **DONE + VERIFIED `fba930e`** — art, not blend; owner confirmed 2026-10-23 |
| AT12 | **B44** menu UI text overlap: measure text bounding boxes, detect collisions, add padding/spacing (no magic-number nudging). Board: `task_01a1638246425086b78d8add`. | assets | **DONE + VERIFIED `267665f`** — row spacing `lh*2+12`, clamped ≥12px; owner confirmed 2026-10-23 |
| AT13 | Harden `gen_assets.py --verify`: every entity texture RGBA + transparent corners, size vs declared/`base_px`, texture count vs `gate-baseline` (lane-local; explicit message). Prevents the B2/B43 class failing silently. | assets | **DONE `56154f0`/`cc5d5b9`** — negative-tested, no wire impact |

> **Lane-local gate baselines (PM, 2026-10-22).** Several lanes re-baseline the
> same `knowledge/gate-baseline` numbers independently; those numbers are
> **lane-local, not merged**:
> - **Textures:** animation AN2 → **41** (8 walk PNGs), AN8 → **49** (8 player
>   PNGs); assets B45 → **34** (vignette) on its pre-merge lane. **main is 49**
>   after animation+net landed; assets landing adds the vignette → **50**. (The
>   earlier "42" note assumed only the zombie walk; AN8 adds eight more.)
> - **Tests:** animation AN1 → **1253**, AN3 → **1347**, AN4 → **1382**,
>   AN5+AN8 → **1759** on lane; net NT2+NT3+NT4 → **1235**; **main after
>   animation+net = 1808/0** (`6801d7c`, measured on the real tree); audio A7 →
>   **1207**, A8 unchanged. Assets landing will add its checks; re-measure.
>
> Do **not** resolve a merge by picking one lane's number. On the merged tree,
> run the suite and `gen_assets --verify` and set the measured numbers
> **deliberately** in `gate-baseline` in that merge commit. Animation lands
> first; assets/audio rebase onto it.
>
> **One code file both lanes touched:** `src/systems/render.c` — net's v9 added
> the mirrored-sword block in `system_render_mirror()`, animation's AN3 removed
> `system_animation()` from the same file. Different functions; checked
> semantically by net, should merge cleanly — still verify at integration.
>
> **Landing sequence — progress:** animation **LANDED** (`3648856`), net v9+NT2
> **LANDED** (`6801d7c`), assets **LANDED** (`b0ccc28`; merged gate 1808/0,
> **50 textures**, SP identical) → audio merges `main` and lands Phase D. Main
> not pushed. Owner verified B43/B44/B45/AT9 on `b0ccc28` (`0da6e3f`).
> Assets' parallel lane merge `b88a99d` was **discarded** — it committed conflict
> markers into BUGS/PROGRESS and truncated gate-baseline; the lane is reset to
> `main`.
>
> **Playtest provenance (`knowledge/playtest-provenance`):** the 2026-10-23
> playtest ran `main` before assets landed, so the owner saw none of
> B43/B45/B44/AT9 although the fixes were on `feature/assets-maps`. Always launch
> the tree that contains the fix, or land first and run one merged build.

### HUD + effects (owner request 2026-10-23)

> Owner: "a very plain HUD interface and mediocre effects." Wants assets to
> **ideate first**, owner reviews/ranks, then build. Process mirrors the B45
> vignette win. Nothing here is approved until the owner ranks the proposal.

| ID | Task | Owner | Status |
|---|---|---|---|
| HU1 | **Ideation proposal only (no code):** 6–10 concrete HUD/effects ideas, each with player-visible result, files touched, cost S/M/L, risk, on-screen verification; ranked with top 3 named; menu backdrop included; wire/sim changes flagged separately. Board: `task_01a168c19b7d139fadc3c13b`. | assets | **in progress** (lane synced to `0da6e3f`) |
| HU2 | Approved HUD/effect slices — one commit each, full gate, **unproven until the owner looks**. | assets | pending owner ranking |

Constraints: HUD readability beats decoration; cosmetics must be client-safe
(host is truth; a client computes only deterministic cosmetics); anything
touching the sim or wire needs its own decision and cannot move the SP
byte-identity baseline silently.

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
