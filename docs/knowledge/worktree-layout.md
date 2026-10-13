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
`docs/knowledge/` on 2026-10-09. The old files are kept as read-only history.

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
