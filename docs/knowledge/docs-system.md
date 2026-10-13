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

## 2026-10-13 14:20 UTC - note by assets: AT1 DONE — the planning set migrated into five topics; the loose docs are removed

AT1 executed as one migration commit (scoped to the docs migration + the
`CROSS_BRANCH_CONFLICTS.md` fix that owner D7 attached to it). One topic per
subject; orientation prose kept but de-duplicated against existing topics.

| Was (loose `docs/` file) | Now (topic) |
|---|---|
| `AGENT_RULES.md` | `knowledge/agent-rules.md` |
| `ANIMATION_PLAN.md` | `knowledge/animation-plan.md` (the A-vs-B question stays in `animation-transport`) |
| `AUDIO_PLAN.md` | `knowledge/audio-plan.md` |
| `ANIMATIONS_AGENT.md` | `knowledge/animation-agent.md` |
| `AUDIO_AGENT.md` | `knowledge/audio-agent.md` |

D1 swords (B40) folded into `knowledge/animation-plan` §3, Phase F and two risk
rows; `CROSS_BRANCH_CONFLICTS.md:211` fixed and committed in the same migration.

Live cross-references repointed to the topics: `docs/README.md` (index +
topics table), `knowledge/versioning-decisions.md`, `knowledge/i5-unverified-on-screen.md`,
`knowledge/animation-transport.md`, `.gitignore` (comment only). **Left as
history, deliberately:** dated decision entries in this topic and
`knowledge/worktree-layout.md` / `knowledge/open-decisions-awaiting-pm.md`,
`docs/BUGS.md:741` (B37 narration), and `docs/SPRINT_PLAN.md` (PM-owned).

**Why:** Owner D7 / AT1. Recording the mapping so a reader of an old reference can find where each doc went, and so the "left as history" calls are explicit rather than an omission.

