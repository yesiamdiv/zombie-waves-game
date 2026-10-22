# Ui damage vignette

<!-- Project knowledge (Intercom-AI). Agents append entries; edit freely. -->

## 2026-10-22 12:07 UTC - decision by pm: Damage vignette uses a baked radial-alpha texture (route 1), not real-time bands

Owner feedback 2026-10-22 on B41 (damage vignette). Two routes were offered: (1) bake a vignette image and blit it, fading on a timer; (2) draw concentric circles/layers in real time. PM decision: **route 1, baked texture**.

Spec:
- Screen-sized vignette texture, red, with a radial alpha ramp: alpha 0 at the centre, rising from ~40% of the radius outward to maximum at the edges/corners.
- Generate it through `tools/gen_assets.py` (existing asset pipeline + alpha conventions), not a hand-made binary.
- Draw stretched to the full screen in the HUD. On damage show at full; decay over time (existing `damage_flash` timer in `src/ui/hud.c`); a fresh hit resets to full.
- Cosmetic/local only — no authority or determinism concern.
- Adding the texture re-baselines the texture count in `docs/knowledge/gate-baseline.md` (33 -> 34) in the same commit.

Replaces the B41 band-based fix `e637a5d`. Queued behind the animation/audio stage (board task `task_01a16382656225660a3f39fe`); do not start until the owner declares that stage begun. On-screen result stays UNPROVEN until the owner looks (`knowledge/i5-unverified-on-screen`).

**Why:** The current implementation draws 24 concentric SDL_RenderFillRect edge bands (assets commit e637a5d), which the owner judged banded/incomplete. A baked texture gives a smooth radial falloff in one blit, matches the existing gen_assets pipeline and the project's determinism/asset rules, and avoids per-frame multi-rect drawing. The trade-off is one extra texture (gate-baseline texture count 33 -> 34), which must be re-baselined deliberately in the same commit.
