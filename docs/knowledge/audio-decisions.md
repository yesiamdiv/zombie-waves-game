# Audio decisions

<!-- Project knowledge (Intercom-AI). Agents append entries; edit freely. -->

## 2026-10-22 13:16 UTC - decision by pm: Audio §4(c) deferred: keep option (a) for own gunfire; revisit only if a human reports it late

PM decision 2026-10-22 on audio-plan §4 option (c), based on audio's A1–A6 spike (`feature/audio@8f3e4c3`).

**Decision: keep option (a); do not build (c) now.** A client hears its own gunfire via the relayed `GE_PLAYER_SHOT` at up to two frames of latency.

Measured basis (audio A5): the relay itself costs ~1 ms (102 matched shots, 0 id mismatches). The audible delay is **frame-bound** — one host frame + one client frame + ~1 ms. Windowed on this machine (~25 FPS): median 89.6 ms (min 52.6, max 106.1); at 60 FPS the same path is ~33 ms. No netcode tuning removes it; it is the frame rate.

Why defer rather than build (c):
- Only **one** sound is affected (the client's own gunfire); every other sound is about the world, where one relay frame is inaudible.
- Option (c) requires a local/relayed dedupe by bullet entity id — the double-fire class that A4 empirically ruled out. Adding it for one sound reintroduces that risk.
- Audibility is a **human-perception** question; the gate is blind to audio. The correct trigger to build (c) is a human reporting it, not a number.

Revisit condition: a human playtests two windows and reports the client's **own** gunshot as late. That is the first item on the listen-for list (`PENDING_VERIFICATION.md`). If it is reported, build (c) as a cosmetic-only local prediction with relayed-copy dedupe, and prove no double-fire by the A4 counting method.

Also recorded from A3: the client cannot tell *who* hit it (`GE_DAMAGE` attacker `ib` is dropped by the relay) and cannot distinguish hurt from heal (`ia` dropped); a rocket impact needs `GE_EXPLOSION` (net's NT2). `GE_SHOP_PURCHASE` and `GE_ENTITY_SPAWN` are not relayed, so their sounds are host-only unless the relay list grows (net's file, not audio's). There is no reload mechanic at all — do not add a `NetPlayerState` reload bit.

Nothing pushed without the owner.

**Why:** Audio-plan §4 said build option (c) only if relayed latency proves audible. The measurement shows the delay is frame-bound (~90 ms at the ~25 FPS this dev machine runs, ~33 ms at 60 FPS), not network-bound, and only affects a player's own gunfire. Whether it is audible is a human-perception question no gate can answer, and (c) adds a local/relayed dedupe that risks the double-fire class this project has already ruled out. Deferring keeps the proven, simple path and makes the human the trigger to revisit.
