# HUD + Effects Improvement Proposal (ideation only)

## Context
- Lane reset to main (0da6e3f). All assets work merged; owner verified B43/B45/B44/AT9 on screen.
- Current state: damage uses baked vignette (B45, `assets/textures/ui/vignette.png`, client-local, authority-safe). HP/wave/score/weapon/points shown. Crosshair, pause hint, dead/respawn overlays exist.
- Goal: ideas first, no code. Cosmetic-only preferred; anything sim/wire-affecting is explicitly flagged.

## HUD Ideas (readability/layout/feedback)

| # | Idea | What player sees | Files/systems touched | Cost | Risk | On-screen verification | Notes |
|---|------|-------------------|----------------------|------|------|-------------------------|-------|
| H1 | **HP bar readability pass** | Subtle border/glow on health bar; color ramp tuned at low HP (warmer/yellow->red) without changing values. Optional numeric emphasis (bold/outline) when <25% HP. | `src/ui/hud.c` (bar drawing), `src/ui/hud.h` (optional state) | S | Low | Look at HP at full/half/low HP in play; confirm numbers match. | Purely visual; uses existing `hp_current/hp_max`. Host-authoritative values already displayed correctly. |
| H2 | **Low-HP pulse/edge glow** | Gentle pulsing of vignette or thin colored border when HP < threshold (e.g. <25%). Reuse damage_flash decay or add new timer. | `src/ui/hud.c`, `assets/textures/ui/` (optional), `src/ui/hud.h` | S | Low | Take damage at low HP, see rhythmic pulse; stop pulsing on heal. | Cosmetic/local only. No sim change. Must not conflict with B45 vignette timing. |
| H3 | **Wave/kill feedback consolidation** | Group left column (Wave/Zombies/Name) with better spacing/contrast; subtle panel background behind left cluster and right cluster to improve readability on busy maps. | `src/ui/hud.c` | S | Low | Check on grass/desert/snow/city at 1280x720 and 1920x1080. | Purely layout; respects existing text rows. |
| H4 | **Weapon/consumables clarity** | Add icon hint or clearer stack counts; dim unavailable ammo; keep "1-4 to switch" visible. Consider small cooldown/readout if applicable later (no). | `src/ui/hud.c` | S | Low | Switch weapons, pick up ammo/grenades, check right column. | Display-only from inventory/auth state. |
| H5 | **Themed menu backdrop (proposed earlier)** | Subtle themed backdrop behind menus (main/menu/pause/shop/map-select) - parallax/gradient/tile motif per map theme or neutral darkened panel. Improves legibility on bright maps. | `tools/gen_assets.py` (+1 texture), `src/ui/menu.c`, maybe `assets/textures/ui/` | M | Med | Navigate all menus in windowed/fullscreen; check text contrast vs backdrop. | **Optional per owner call**. UI-only art (+1 texture) → would require `gen_assets.py --verify` count bump and gate-baseline re-baseline in same commit (like B45). No wire/sim. Readability-first, subtle (not busy). |

## Effects Ideas (damage/hit/pickup/wave/low-health/muzzle/death/transitions)

| # | Idea | What player sees | Files/systems touched | Cost | Risk | On-screen verification | Notes |
|---|------|-------------------|----------------------|------|------|-------------------------|-------|
| E1 | **Hit flash on crosshair** | Brief white/red flash on crosshair when player damages enemy (or when taking hit - we have vignette). Client-side only if derived from local events; or use existing damage_flash for self. | `src/ui/hud.c` (crosshair draw), possibly `src/systems/render.c` or event hooks | S | Low | Shoot zombie, see crosshair flash; take damage, see vignette. | Self-hit uses existing `hud_track_player_hp` path. Enemy-hit flash needs event visibility - check if events accessible client-side without sim changes. Prefer cosmetic-only. |
| E2 | **Pickup feedback** | Subtle scale/glow on pickup icon momentarily on collection, or small "+HEALTH/+AMMO" text popup near center/top that fades. | `src/ui/hud.c`, `src/items/*` (collection events) | S | Low | Pick up medkit/ammo/speed; confirm popup/fx doesn't block HUD. | Must be client-local/cosmetic. Don't change pickup radius/logic. |
| E3 | **Wave transition polish** | Brief screen flash/fade or banner animation on wave start/end; use existing `hud_show_message` timing or new effect. | `src/ui/hud.c`, `src/waves/*` (display state only) | S | Low | Start new wave, see transition effect; between waves timing unchanged. | Uses authoritative wave state; purely visual. |
| E4 | **Muzzle flash** | Tiny bright flash at weapon origin on fire (billboard/sprite or simple rect). Client-side, non-authoritative. | `src/systems/render.c` or `src/graphics/`, weapon fire path (render only) | M | Med | Fire each weapon; check doesn't obscure crosshair badly. | Cosmetic only if not affecting sim; must not change firing rate/damage. Needs to be framerate-stable. |
| E5 | **Low-health screen treatment** | Combine H2 with subtle desaturation/vignette intensification below threshold (layered on B45). Keep readable. | `src/ui/hud.c`, `assets/textures/ui/` (optional variant) | S | Low | Low HP in combat, verify readability of UI text still good. | Build on existing vignette; avoid full-screen color wash. |
| E6 | **Death/respawn polish** | Smoother fade/overlay for eliminated/respawning states (multiplayer). | `src/ui/hud.c` | S | Low | Die in MP, see respawn countdown/eliminated overlay. | Display-only, uses existing auth state. |

## Ranking (do first)
1. **H1 - HP bar readability** (S/Low) - biggest legibility win, zero risk
2. **H2/E5 - Low-HP feedback** (S/Low) - high impact, builds on B45 vignette nicely  
3. **H3 - HUD consolidation/paneling** (S/Low) - cleans up busy screens

## Guardrails
- **SP byte-identity:** Must remain unchanged. All ideas above are UI/render-only or use existing display state. No changes to sim, events log content in fixed SP scenario, or world generation.
- **Multiplayer authority:** Host is truth. Client may only consume host state or add deterministic cosmetics. Never invent HP/wave/score/points.
- **Headless blind spot:** Visual changes need human on-screen verification (per `i5-unverified-on-screen`). Write "unproven" until confirmed.
- **Assets:** Adding textures requires `gen_assets.py` update + deliberate re-baseline in same commit (like B45/AN2). Use AT13 invariants.
- **One commit per change.** Gate required: build clean -Werror, ctest 3/3, zombie_tests, gen_assets --verify, SP byte-identical.

## Notes
- Themed menu backdrop (H5) is included as candidate per earlier proposal.
- Muzzle flash (E4) is most likely to need careful tuning to avoid looking noisy; start with subtler ideas first.
