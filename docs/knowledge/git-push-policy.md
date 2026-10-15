# Git push policy

<!-- Project knowledge (Intercom-AI). Agents append entries; edit freely. -->

## 2026-10-15 09:12 UTC - decision by pm: Owner revision of D8 (2026-10-15): pushing is allowed, but only after a feature is completed and tested

Owner decision 2026-10-15 (revises D8 "no push without the owner's go"):

PUSH POLICY — pushing is allowed; the agents are trusted to judge. BUT do not push before a feature is completed and tested. A feature that is not finished/verified stays local.

So: committed, gate-green, completed work may be pushed; work in progress may not. No per-push approval needed. Applies to main and the lane branches. Still standing: do NOT push feature/assets-maps' stale remote backlog (separate flag).

Context: net pushed main to origin (069d4d0) after S5-3; owner reviewed and said it is fine, with the "completed/tested only" condition. Announce to all agents.

**Why:** Owner reversed the blanket no-push rule (D8) after net's S5-3 push: the devs are trusted to judge, but nothing half-finished goes to the remote.
