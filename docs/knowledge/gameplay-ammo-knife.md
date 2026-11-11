# Gameplay: starter knife + limited pistol ammo + unlimited toggle

<!-- Project knowledge (Intercom-AI). Agents append entries; edit freely. -->

## 2026-11-10 - decision by pm (confirmed with owner), implemented by assets

Sprint item "Gameplay: Ammo limits + knife starter + toggle". PM made these
design calls (announced the gate re-baseline to the owner in parallel):

1. **Knife = a new always-unlocked starter melee**, `WEAPON_KNIFE`, appended
   as the LAST `WeaponType` value (=4). It reuses
   `textures/entities/sword_blade.png` scaled down, so `NET_ASSET_VERSION` and
   the 50-texture count are unchanged. The purchasable Sword stays the
   upgrade. Keys become 1-5 (gun slots stay stable; knife is key 5) and the
   HUD hint must say so.
2. **Pistol ammo = one simple counter, no magazine/reserve/reload.** Default
   LIMITED (starts at 60 shots); ammo pickups refill it. No reload mechanic:
   audio's A5 audit confirmed there is no reload on the wire today, and a
   reload would add net surface with no named requirement.
3. **Gate re-baseline (deliberate, not silent).** The canonical single-player
   invariance gate (`--seed=42 --ai=bot --run-seconds=25`) keeps its exact
   reference **125938 bytes / 1603 lines** by running it with
   `--unlimited-ammo`, which proves net/effects didn't perturb the sim. A
   separate default (limited-ammo) run is recorded as the new default-gameplay
   reference. Both are declared in `gate-baseline.md`.
4. **Toggle** is a CLI flag `--unlimited-ammo` plus an in-game key toggle
   (`U`), mentioned in the HUD hint. Default = limited so the feature is
   visible.

## Consequences

- `pistol_ammo` is host state the client HUD must display. That requires a
  `NetPlayerState` field, and `WEAPON_KNIFE` is a new enum value that can
  travel in `NetPlayerState.weapon` -> `NET_WIRE_VERSION` 9 -> 10, with the
  codec test updated in the same commit. PM routed that bump to **net**
  (NT8); the assets lane adds the enum and host/SP logic, and wires the
  MP-correct client readout after v10 lands.
- Host authority is unchanged: single-player and the host simulate ammo; a
  render-only client only displays `NetPlayerState.pistol_ammo`. The `U`
  toggle is a host/SP debug control; on a client it cannot change the host's
  rule.
