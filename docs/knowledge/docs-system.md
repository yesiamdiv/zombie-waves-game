# Docs system

<!-- Project knowledge (Intercom-AI). Agents append entries; edit freely. -->

## 2026-10-13 14:01 UTC - decision by pm: 2026-10-13 owner (D7): docs/knowledge is the docs system of record — migrate the planning docs into it

DOCS SYSTEM (owner, 2026-10-13, resolving D7):

- The project is COMPLETELY migrating to the intercom knowledge-docs system.
  `docs/knowledge/<topic>.md` is the system of record for durable decisions and
  information ("docs that can be used by everyone ... remembered forever").
- The animation/audio planning set (`ANIMATION_PLAN.md`, `AUDIO_PLAN.md`,
  `AGENT_RULES.md`, `ANIMATIONS_AGENT.md`, `AUDIO_AGENT.md`) and other loose
  planning docs are to be MIGRATED into knowledge topics, not committed as loose
  files to main or to a lane branch.
- Owner delegated the method: "do it as you please."
- Practical reading: one topic per subject; cite a topic, never copy its
  contents; keep the plan's decision-bearing content, drop orientation prose
  that duplicates a topic.
- Consequence for tasks: AT1 becomes "migrate the planning set into
  docs/knowledge topics"; assets' untracked CROSS_BRANCH_CONFLICTS.md fix also
  rides this migration rather than a standalone commit.

HUMAN GATE CAVEAT (owner, 2026-10-13): run the two-instance launch from a build
of the BRANCH UNDER TEST, not from main, or it proves nothing about that branch.
The launch snippet in AGENTS.md now points at worktrees/multiplayer.

**Why:** Owner D7: the project is completely migrating its docs to the intercom knowledge system. Durable facts belong in docs/knowledge/ topics, not in loose doc files committed to branches. This changes where the animation/audio plan docs live.
