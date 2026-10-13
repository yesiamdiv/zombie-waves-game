# Priority current work first

<!-- Project knowledge (Intercom-AI). Agents append entries; edit freely. -->

## 2026-10-13 14:06 UTC - decision by pm: Owner directive (2026-10-13): finish each agent's current/established work before opening the animation/audio stage

Owner directive, 2026-10-13: each agent FINISHES its current/established work ("what they were doing before all of this planning started") before moving to the next stage. Animation/audio is the NEXT STAGE and does not open until Priority 1 is done and main is clean + gate green.

Board is split into PRIORITY 1 (current work) and NEXT STAGE in docs/SPRINT_PLAN.md §3.

Confirmed with the devs (same day):
- net: Priority 1 is effectively DONE for code he can land now. Remaining: NT3/B38 is spike-gated (belongs to the bump window, parked not abandoned); B32-B35 are human-verification debt (NT7). NT2/NT4/NT5 are next-stage. He has no other in-flight lane (weapon-upgrades, map-select-draft are idle).
- assets: Priority 1 = AT1 alone (the knowledge migration). The assets/maps lane (feature/assets-maps) is PARKED, not unfinished — 0 commits ahead of main (all merged), worktrees/assets clean, last touched 2026-10-06. AT2-AT7 are next-stage.

Consequence: the animation/audio worktrees are NOT cut yet, no spikes yet. Owner cuts worktrees by hand once Priority 1 lands and main is clean + gate green.

**Why:** Owner wants the established netcode + assets lanes closed out first, so the huge animation/audio stage starts from a clean, finished base rather than on top of half-open work.
