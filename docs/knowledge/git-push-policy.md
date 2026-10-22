# Git push policy

<!-- Project knowledge (Intercom-AI). Agents append entries; edit freely. -->

## 2026-10-15 09:12 UTC - decision by pm: Owner revision of D8 (2026-10-15): pushing is allowed, but only after a feature is completed and tested

Owner decision 2026-10-15 (revises D8 "no push without the owner's go"):

PUSH POLICY — pushing is allowed; the agents are trusted to judge. BUT do not push before a feature is completed and tested. A feature that is not finished/verified stays local.

So: committed, gate-green, completed work may be pushed; work in progress may not. No per-push approval needed. Applies to main and the lane branches. Still standing: do NOT push feature/assets-maps' stale remote backlog (separate flag).

Context: net pushed main to origin (069d4d0) after S5-3; owner reviewed and said it is fine, with the "completed/tested only" condition. Announce to all agents.

**Why:** Owner reversed the blanket no-push rule (D8) after net's S5-3 push: the devs are trusted to judge, but nothing half-finished goes to the remote.

## 2026-10-22 12:36 UTC - decision by pm: Owner 2026-10-22: nothing is pushed without the owner (supersedes the 2026-10-15 push-allowance)

Current rule (owner, 2026-10-22 kickoff):

**Nothing is pushed without the owner's explicit go** — for main and for every lane branch.

This supersedes the 2026-10-15 revision in this file ("pushing allowed after a feature is completed and tested"). That allowance is no longer in force; treat every push as requiring a direct owner instruction, regardless of whether the work is gate-green.

Context at the time of the instruction: main was ~14-16 commits ahead of origin/main; `feature/assets-maps` was ~96-128 commits ahead of its remote (stale backlog that must never be pushed on its own). Keep all new work local.

If an agent is unsure, do not push; ask the owner.

**Why:** The owner's 2026-10-22 kickoff message explicitly states 'main has 14 unpushed commits and feature/assets-maps has 96 — nothing is pushed without the owner', which overrides the 2026-10-15 allowance to push completed+tested work. Recorded so no agent relies on the older revision.
