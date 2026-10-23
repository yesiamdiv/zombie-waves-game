# Hud effects ideation

<!-- Project knowledge (Intercom-AI). Agents append entries; edit freely. -->

## 2026-10-23 12:35 UTC - decision by pm: HUD/effects work: agent proposes, owner ranks, then build in small verified slices

**Process for HUD + effects (owner request 2026-10-23):**
1. The owning agent (assets) writes a proposal only — no code, no commits: 6–10 ideas, each with the player-visible result, files/systems touched, cost S/M/L, risk, and how a human verifies it on screen. HUD separated from effects; ranked with a recommended top 3.
2. The owner reviews and ranks/approves. PM does not pre-select the design.
3. Approved items become small tickets: **one commit each**, full gate, and stay `unproven` until the owner looks (the headless gate is blind to visuals).
4. The optional themed menu backdrop is a candidate in the same list.

**Constraints:** HUD readability beats decoration. Cosmetics must be client-safe — host state is truth, and a client may only compute cosmetics that are deterministic across machines. Anything needing a sim or wire change is a separate decision and must not silently move the single-player byte-identity baseline.

Task: `task_01a168c19b7d139fadc3c13b` (HU1), board rows in `docs/SPRINT_PLAN.md` §HUD + effects.

**Why:** Owner asked for a less plain HUD and better effects (2026-10-23) and explicitly wanted the agent to invent options and get owner sign-off before coding, rather than PM prescribing a design. The same ideate-then-rank loop produced the B45 vignette, which the owner singled out as good.
