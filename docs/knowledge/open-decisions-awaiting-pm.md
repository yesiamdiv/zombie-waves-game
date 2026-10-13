# Open decisions — what is blocked and on whom

<!-- Project knowledge (Intercom-AI). Agents append entries; edit freely. -->

## 2026-10-09 12:36 UTC - note by assets: Three items were waiting on a PM scope call that did not exist yet; both devs are holding

As of 2026-10-09 both devs were deliberately holding code, not idle. These are
the live blocks.

WAITING ON A PM / OWNER SCOPE CALL:
1. **Swords (B40) - are they in the animation sprint at all?** Sword blades are
   not replicated today. [assets] leans "option 1": a new `NET_ENT_SWORD`
   snapshot kind, mirroring the other projectiles, inheriting `art`/`owner`,
   with no derivation. That is the right *shape*; whether it is *in scope* is a
   scope decision, not a dev one. If in scope it rides the same wire bump as
   B38. **[net] is holding all code until this is answered.**
2. **`GE_WAVE_END` (B39) and `GE_EXPLOSION` go-ahead.** [net] will not land
   either silently and wants the owner's go. `GE_WAVE_END` is free;
   `GE_EXPLOSION` forces a single-player events-log re-baseline. See
   [relayed-events].

WAITING ON A SPIKE, NOT ON A PERSON:
3. **Animation transport A vs B** - see [animation-transport]. Two measurable
   checks decide it. Explicitly NOT to be defaulted on and B must not be built
   speculatively.
4. **Reload audio** - whether reload state is on the wire. Audio spike answers
   it. Nobody builds one on speculation.

HARD-BLOCKED ON A HUMAN:
5. **Animation Phase D** - blocked until a human watches two windows and
   confirms I5 renders. See [i5-unverified-on-screen]. A green gate is not
   sufficient and both agents agreed to this.

BRANCHES NOT YET CUT: `feature/animation-assets` and `feature/audio` are named
throughout the plans but **neither exists**. `docs/ANIMATIONS_AGENT.md` and
`docs/AUDIO_AGENT.md` also name worktrees (`...-animation`, `...-audio`) that
were never created - and those paths follow the old beside-the-checkout layout,
which is no longer how this project is laid out. See [worktree-layout].

## 2026-10-13 09:28 UTC - decision by pm: 2026-10-13 owner resolved D1-D4, D6-D9 (D5 naming held)

Owner decisions, 2026-10-13 (via Intercom, reply to the PM decision queue):

D1 SWORDS (B40) — IN SCOPE for the animation update. Requirements:
  - must render on the CLIENT so other players see them;
  - new `NET_ENT_SWORD` snapshot kind (option 1), inheriting `art`/`owner`, no
    client-side derivation (B19-B23);
  - **must provision for future attributes** (e.g. attack speed and later
    additions) so the design extends without rework — "the provision must be
    made for adaptation".

D2 `GE_WAVE_END` (B39) — GO. Free; event exists, relayed host-side; no wire
bump, no single-player output change.

D3 `GE_EXPLOSION` — GO. Not free: a new `GameEventType` adds lines to the
single-player events log and so is a deliberate RE-BASELINE of
`knowledge/gate-baseline`. It rides the audio commit (AT7) and is re-verified
jointly with net. Owner's words: "we need the features then we add it; we make
other changes if needed."

D4 ANIMATION + AUDIO — STILL PLANNED, currently in the PLANNING PHASE (nothing
coded). They start only after the other blockers are resolved and net+assets
finish their current work. Additional changes may still come.

D5 WORKTREE/BRANCH NAMES for animation+audio — HELD: owner will specify ("this
case is a bit complicated, I'll tell you what to do by the end").

D6 SEPARATE AGENTS — owner or PM hires; do NOT hire yet. Hire only after the
clutter is cleared and planning is finished. PM recommends separate agents for
animation and audio.

D7 PLANNING DOCS — to live in the knowledge-docs system (`docs/knowledge/`), NOT
committed to `main` or `worktrees/assets`. (One interpretation pending owner
confirmation: full-plan migration vs decisions-only.)

D8 NET WORKTREE — resolved; see the operating model appended to
`knowledge/worktree-layout`.

D9 SPRINT_PLAN commit — left to PM judgement ("do whatever feels right ... commit
if needed").

HUMAN GATE — once all blockers are done, the PM tells an agent to run two game
windows so the OWNER can clear animation Phase D / the I5 mirror check.

FLAGGED, NOT TO ACT ON: `feature/assets-maps` is 96 commits ahead of its remote
and untouched since 2026-10-06. Owner said do not push it.

**Why:** Owner scope call 2026-10-13. Recording because a decision that lives only in a message is lost, and these freeze 13 tasks.

## 2026-10-13 14:01 UTC - decision by pm: 2026-10-13: D5 and D7 now resolved; queue fully closed

D5 (animation/audio branch + worktree names) — RESOLVED: see
`knowledge/worktree-layout` (animation -> worktrees/animation /
feature/animation-assets; audio -> worktrees/audio / feature/audio).

D7 (where the planning docs live) — RESOLVED: `docs/knowledge/`; the project is
migrating its docs into the knowledge system. See `knowledge/docs-system`.

All of D1-D9 are now closed.

**Why:** Closing out the owner decision queue raises.
