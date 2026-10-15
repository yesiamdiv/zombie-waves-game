# Worktrees live inside the main checkout, and how agents coordinate now

<!-- Project knowledge (Intercom-AI). Agents append entries; edit freely. -->

## 2026-10-09 12:36 UTC - decision by user: Sibling worktrees moved into <main>/worktrees/ on 2026-10-09; the agents/ file channel is retired for intercom tools

THE LAYOUT (changed 2026-10-09, with `git worktree move`, nothing lost):

| Path | Branch |
|---|---|
| `~/development/personal/SDL3/open-world-zombie-waves` | `main` |
| `<main>/worktrees/assets` | `feature/assets-maps` |
| `<main>/worktrees/multiplayer` | `feature/net-protocol` |

`worktrees/` and `.agents/` are in `.git/info/exclude` and are never committed.

THESE PATHS NO LONGER EXIST - any doc or script naming them is stale:
`.../open-world-zombie-waves-assets`, `.../open-world-zombie-waves-multiplayer`.

Work in your own worktree only, with `git -C worktrees/<name> ...`, committing
on that worktree's branch. Never edit or `git add -A` in the other one.

NOTE: AGENTS.md's "Worktree discipline" section previously said integration
happens in the multiplayer worktree on branch `feature/multiplayer`. The
worktree is actually on `feature/net-protocol`; `feature/multiplayer` exists but
has no worktree.

COORDINATION - the file channel is retired:
Agents now talk through the `intercom_*` tools (`intercom_agents`,
`intercom_send`, `intercom_ask`, `intercom_inbox`, `intercom_reply`,
`intercom_wait_for_reply`, `intercom_whoami`) and record durable decisions with
`intercom_save_knowledge` into `docs/knowledge/`.

RETIRED: the `agents/` directory as a live channel; `tools/agent_channel.sh`;
`scripts/watch_agents.sh`; the handle-prefix and question-numbering format of
`AGENT_RULES.md` §11; and §11's polling discipline - agents are now woken when a
message arrives and an unanswered request escalates automatically.

WHY: `agents/` was gitignored, so it could never be the durable record - and it
had nonetheless accumulated 924 lines of real decisions that one `git clean`
would have destroyed. Everything of value in it was migrated into
`docs/knowledge/` on 2026-10-09. The old files were kept as read-only history
until they were deleted on 2026-10-15 (see the entry below).

WHAT HAS NOT CHANGED: a message is still as temporary as the old files were.
The channel is how you reach an answer; `docs/knowledge/` or the plan doc is
where the answer lives.

**Why:** One folder per project, so OpenCode chats and worktrees cannot drift apart; and a message channel that is gitignored cannot be the record of anything.

## 2026-10-09 13:39 UTC - decision by pm: Historical branch/agents references are history, not stale — do not mass-rename

Docs hygiene decision (2026-10-09, PM, from the AT8 sweep):

STALE — fix (point at something that no longer exists / never existed):
- old beside-checkout worktree paths (`.../open-world-zombie-waves-assets`,
  `.../open-world-zombie-waves-multiplayer`)
- references to the retired `agents/` file channel and `tools/agent_channel.sh`
  as if live
- worktree paths named in `ANIMATIONS_AGENT.md` / `AUDIO_AGENT.md` that were
  never created

NOT STALE — LEAVE AS HISTORY:
- bare mentions of the branch `feature/multiplayer`: it is a real branch (it just
  has no worktree). Present in `docs/BUGS.md`, `docs/MERGE_DECISIONS.md`,
  `docs/CROSS_BRANCH_CONFLICTS.md`, `docs/NET_SPRINT_PLAN.md`.
- `docs/BUGS.md:721,791,819,839` narrate the `agents/` channel as it was used at
  the time — genuine history, kept verbatim.
- `AGENT_RULES.md` and `knowledge/worktree-layout.md` mention the retired tooling
  only inside explicit "RETIRED" statements — correct as-is.

Rationale: a blanket rename would delete the record. Any proposal to rewrite
this history is an owner escalation, not a sweep.

**Why:** A blanket find-and-replace on these strings would destroy the record of what actually happened; feature/multiplayer is a real branch, and BUGS.md's agents/ narration is history. Stale means "points at something that never existed or no longer exists"; history is not stale.

## 2026-10-13 09:28 UTC - decision by pm: 2026-10-13 owner: cwd is always main; the lane is a worktree inside it — operating model and net's transition

OWNER OPERATING MODEL (2026-10-13), binding on all agents:

- `intercom_whoami` reports WHERE THE PROCESS RUNS. For every agent here that is
  the main checkout, and always will be: all agents spawn from the one OpenCode
  instance in `~/development/personal/SDL3/open-world-zombie-waves`. That is not
  a misconfiguration; do not "fix" it and do not treat it as a lane.
- THE LANE lives in the organization record / each agent's responsibilities:
      net    -> worktrees/multiplayer   branch feature/net-protocol
      assets -> worktrees/assets        branch feature/assets-maps
      qa     -> main (verifies others' branches; never commits code)
      pm     -> main (owns docs/SPRINT_PLAN.md and docs/knowledge/)
- So an agent edits under `worktrees/<name>/` and runs git as
  `git -C worktrees/<name> ...`, committing on that branch. Nothing mechanically
  stops an agent committing to main instead (that is how net drifted) — the PM
  catches it by checking `git -C worktrees/<name> log --oneline -3`, not by asking.

NET'S TRANSITION — owner's order, do not reorder:
  1. net FINISHES its current work in the main checkout. It has 9 unpushed
     commits on main (3 from today: docs(net), fix(tests), docs(assets)). That
     work is docs/build fixes and BELONGS on main — do not revert or
     cherry-pick it away.
  2. net states plainly that the main work is landed.
  3. ONLY THEN does net switch to `worktrees/multiplayer` and work on
     `feature/net-protocol`. No net code changes in main after that.
  Do not ask net to switch before step 2. **Do not push anything without the
  owner's go** — pushing is the owner's call.

HIRING FUTURE MEMBERS (owner hires; PM proposes):
  New agents get a real worktree:
  `intercom-corp hire <id> --role <role> --create-worktree [--branch <name>]`
  -> creates `<project>/worktrees/<agent_id>` on branch `agent/<agent_id>`
  (git-excluded), which becomes the agent's home; it needs its own OpenCode in
  it (`cd worktrees/<id> && opencode`) or `intercom-corp launch`.
  For an EXISTING chat (`hire --session`), `--worktree`/`--branch` are silently
  ignored — never assume a seated hire recorded a lane; verify it.

**Why:** Every agent runs from the one OpenCode instance in the main checkout, so cwd is always main; the lane is a separate field. This is why net drifted to committing on main and why the earlier org fields contradicted whoami. Binding operating model for current and future members.

## 2026-10-13 14:01 UTC - decision by pm: 2026-10-13 owner: a lane is a starting point and default, not a fence — disclosure, not prohibition

LANE MODEL — owner's correction (2026-10-13). Supersedes any reading of D8 that
treated the lane as a hard fence.

- A lane is a STARTING POINT and a DEFAULT commit target; it is part of an
  agent's identity, not the whole of it. An agent may READ any worktree and may
  WORK in another one when the task genuinely calls for it (checking whether a
  fix landed, running the gate against another branch, a repo-wide docs/build
  fix that belongs on main).
- The rule is DISCLOSURE, not PROHIBITION: say where the work is going before it
  goes there, rather than switching silently.
- Consequence: net's commits on main were NEVER a violation. They are docs and
  build fixes that belong on main. Nothing is to be reverted or moved. net
  finishes that work, declares it landed, then makes worktrees/multiplayer its
  default.

D5 NAMES SETTLED (owner, 2026-10-13):
  agent `animation` -> worktrees/animation -> branch feature/animation-assets
  agent `audio`     -> worktrees/audio     -> branch feature/audio
  Order: animation is cut and lands first; audio is cut from main and rebased
  onto feature/animation-assets before landing. Unchanged from the net/assets
  agreement.

CORRECTION TO THE EARLIER HIRING NOTE: IGNORE `--create-worktree` and any
per-agent OpenCode. The owner's rule is absolute — EVERY chat belongs to the
single OpenCode instance in the main checkout. animation and audio will be SESH
seated chats in that instance, like pm/net/assets/qa. cwd is main; the lane is
reached with `git -C worktrees/<name> ...`. The earlier entry in this file about
`--create-worktree` making the worktree the agent's home is WRONG for this
project; do not propose hiring an agent into its own folder.

ONBOARDING SEQUENCE for animation/audio (owner runs it, when the owner says go):
  1. main is committed/clean and the gate is green — HARD precondition, because
     the lane branches are cut from main's HEAD. Cut from the commit, never the
     dirty tree.
  2. The owner creates the two worktrees by hand (NOT --create-worktree):
       git worktree add -b feature/animation-assets worktrees/animation
       git worktree add -b feature/audio            worktrees/audio
  3. In main's OpenCode, the owner starts two new chats (send each a "hi" so the
     session lands in the DB), then hires them seated and PATCHES their lanes in
     organization.json — because `hire --session` silently drops
     --worktree/--branch (issue #1). Never assume a seated hire recorded a lane;
     verify with `git -C worktrees/<name> log --oneline -3`.
  BUILD: lanes cut from main inherit the shared dep-cache default, so these two
  do NOT need -DFETCHCONTENT_BASE_DIR (the older feature branches do). Still -G Ninja.

**Why:** Owner correction 2026-10-13. A lane is a default/identity, not a fence; the earlier reading treated it as prohibition, which wrongly framed net's main commits as a violation. Disclosure replaces prohibition.

## 2026-10-15 - decision by user: everyone on ONE COMMON POINT; re-align at stage start, not after every commit

Owner: "everyone must be on a common point." The lane worktrees (`worktrees/assets`, `worktrees/multiplayer`) and `main` must sit on the SAME commit before a work stage begins.

BUT main keeps moving as PM lands docs/cleanup commits, and each such commit re-drifts the lanes by one docs-only commit. Re-aligning after every commit is churn. CONVENTION: re-align each lane (`git -C worktrees/<name> merge --ff-only main`) once, at the START of each work stage — that is the commit the stage builds from. A docs-only drift mid-stage is harmless and is absorbed by the next re-align.

Why it matters: the animation/audio stage is cut from whatever commit the lanes share; a lane that silently sits behind main would start the new work from an older base.

Push: see `[git-push-policy]` (2026-10-15) — pushing allowed only for completed, tested work.

## 2026-10-15 - decision by pm: the retired agents/ directory is deleted

Owner call + net/assets agreement (2026-10-15): `agents/` may be deleted, because
every decision in `agents/animation-audio-vs-net.md` and
`agents/net-next-phase-protocol.md` was already captured in `docs/knowledge/`
(both devs mapped it file-by-file). The one non-decision item — the "what is
already replicated today" inventory — was migrated first, to
[net-replication-surface].

REFS FIXED IN THE SAME COMMIT: `docs/README.md` and the 2026-10-09 entry above
both said `agents/*.md` was "kept as read-only history"; that is now false and
both were updated to record the 2026-10-15 deletion.

LEFT AS HISTORY (2026-10-09 hygiene decision stands): the `agents/` pointers in
`docs/BUGS.md:721,791,819,839` narrate the channel as it was used at the time;
they are not repointed. `AGENTS.md`'s mentions were not touched (held for D9).

**Why:** A gitignored directory cannot be the durable record, and once every
decision in it lived in `docs/knowledge/`, keeping it was a second, stale copy
that one `git clean` could destroy without notice.
