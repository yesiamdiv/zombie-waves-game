# Owner ranking ideation 2026 10 27

<!-- Project knowledge (Intercom-AI). Agents append entries; edit freely. -->

## 2026-10-27 10:25 UTC - decision by pm: Owner ranking: DO NOW 23 items, DO LATER H5/N3, N4 pending decision; added detailed UI/ammo/hit-feedback/system requirements

Owner's ranking (2026-10-27):
**DO NOW (23):** H1-H4, E1-E6, P1-P5, N1,N2,N5, Q1-Q5  
**DO LATER (2):** H5 (themed menu backdrop), N3 (relay overflow visibility phase 1)  
**REJECTED/DEFERRED:** None  
**UNASSIGNED PENDING DECISION (1):** N4 (reject message correctness - fix "host 0, you 0" and FULL capacity; client-local, no wire change)

Additional requirements:
- Low-HP edge glow must differ from damage vignette (color/shape/transparency)
- Single common UI font everywhere; panels as semi-transparent blur or just text with borders (prefer borders if plain)
- Main gun ammo limited by default; starter kit has knife + gun; ammo toggle unlimited/limited (unlimited for testing)
- Crosshair hit flash: research alternatives (zombie flash red or "plow plow" text near zombie)
- General pickup text system spawning text around player in different orientations; reusable for zombie damage numbers
- Build event-driven pluggable listener systems so features don't interfere and can be toggled
- Wave transition: text appears center, shrinks to panel position, or breaks like twinkles (not fade)
- Muzzle flash should be part of the damage feedback system
- P1: stagger via late spawns from server + add zombie spawn animation; coordinate with animation
- Many things could be animation systems - coordinate across agents
- P5 sword trail: implement, likely cosmetic

N4 is client-local/no wire change per net proposal - approve as safe fix.

**Why:** Owner reviewed all proposals and ranked: DO NOW has 23 items (all HUD H1-H4, all effects E1-E6, all animation P1-P5, net N1,N2,N5, qa Q1-Q5). DO LATER has H5 and N3. N4 (reject message fix) needs decision - owner listed it as UNASSIGNED PENDING DECISION. New requirements added: different low-HP glow from damage vignette; single common UI font everywhere; panels as semi-transparent blur or borders if plain text; main gun ammo limited (starter knife+gun); ammo toggle unlimited/limited (usually limited, unlimited for testing); crosshair hit flash alternatives (zombie flash red or "plow plow" text near zombie) to research; general pickup text system that can be reused for damage numbers; event-driven pluggable listener systems; wave transition as text shrinking to panel or breaking/twinkling (not fade); muzzle flash part of damage feedback system; P1 could have staggered spawn timing from server + zombie spawn animation; sword trail (P5) implement (likely cosmetic). Systems coordination requested.
