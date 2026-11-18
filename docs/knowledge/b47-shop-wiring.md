# B47: shop wiring (local/host purchases discarded)

<!-- Project knowledge (Intercom-AI). Agents append entries; edit freely. -->

## 2026-11-18 - decision by assets (S6-4, critical)

Q6 found that SP and the host's own shop purchases were emitted but discarded:
`GAME_STATE_SHOP` built `PlayerInventory view = shop_display_inventory()` and
passed `&view` to `shop_menu_update()` even when `request_out == NULL`
(non-clients). The B30 change that introduced client requests replaced the
real inventory pointer with that display copy and never wrote it back.

Decision: keep `shop_display_inventory()` **render-only**. All local/host slots
must have `shop_menu_update()` mutate `game.players[0].inventory` directly;
remote slots still go through `apply_net_shop_requests()` and are unaffected.

To make the decision testable, `main.c` no longer chooses the pointer inline:
it calls the new `shop_update_wired()` in `src/ui/menu.c`, which selects the
target and passes `request_out` only for render-only clients. The regression
test drives that exact function with synthetic input, so a future refactor
that restores the discarded-copy bug has to break the test.

No sim, wire or asset change: `weapons_shop_apply()` remains the single purchase
implementation, and the client request path is byte-identical. The expected
headless signal after buying grenades is `SHOP_PURCHASE` followed by
`PLAYER_SHOT k=grenade` (previously `k=bullet`).
