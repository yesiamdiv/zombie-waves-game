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

## 2026-10-22 13:23 UTC - decision by pm: Owner 2026-10-22: push the lane branches, NOT main

Owner instruction (2026-10-22, direct): "i want the local repos also to be pushed to github ok. but not main for now. thats only when needed and assured everything works fine right."

Current rule:

- **Lane/feature branches may be pushed** on the owner's go. Pushed 2026-10-22:
  `feature/animation-assets` @ `e45304c`, `feature/assets-maps` @ `7dfb49a`,
  `feature/audio` @ `8f3e4c3`, `feature/net-protocol` @ `cd7e8ea`.
- **`main` is NOT pushed** until the owner says so and the work is verified.
  `origin/main` stays at `069d4d0`.
- Only **committed** work is pushed; uncommitted WIP in a worktree (e.g. net's
  v9 bump in `net.h`/`net_codec.c`) is not part of a push.
- No force-push. No rewriting published history.
- Pushing a lane branch also uploads `main`'s unpushed commits **as ancestry**
  (the lanes forked from main). That does not move the `main` ref on GitHub and
  is accepted.

**Why:** The owner explicitly authorized feature-branch pushes while retaining
main as the verified-only branch. This amends (does not erase) the 2026-10-22
"nothing without the owner" rule: the owner gave the go for the lane branches;
main still needs its own go.
