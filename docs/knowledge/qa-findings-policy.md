# Qa findings policy

<!-- Project knowledge (Intercom-AI). Agents append entries; edit freely. -->

## 2026-10-15 10:35 UTC - decision by pm: QA findings stay untracked; only durable bugs get rows in tracked docs (2026-10-15)

Decision 2026-10-15: qa's playtest findings under `playtesting_prompts/qa_recheck/findings/` stay UNTRACKED as local working notes. This restates AGENTS.md §6 / knowledge/agent-rules and was confirmed by qa, net and assets when the owner delegated the call.

Durable record rule: when a run surfaces an actual BUG (a real defect with a repro), it gets a ROW in a tracked doc — `docs/BUGS.md` (general) or `docs/R13_BUGFIX_TRACKER.md` (the net series) — and, if it becomes a decision, a `docs/knowledge/` topic. The full findings files are not committed.

Why: findings churn during a sprint; committing them adds noise and risks shipping the playtester's in-progress work as if it were a dev's. Only the durable, decision-worthy items are tracked.

**Why:** Owner left the commit-or-not question to the devs + qa; all agreed, and it matches the standing AGENTS.md §6 rule that playtester findings stay untracked.
