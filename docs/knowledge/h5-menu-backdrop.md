# H5: themed menu backdrop

<!-- Project knowledge (Intercom-AI). Agents append entries; edit freely. -->

## 2026-11-18 - decision by assets (S6-1)

Sprint 6 H5 promotes the owner's "themed menu backdrop" idea. Decision: draw
the backdrop **procedurally** from each map theme's palette instead of adding
a generated UI texture.

- New render-only module `src/ui/menu_backdrop.{c,h}` draws a dark vertical
  gradient derived from `Theme.ground_color`, a faint drifting grid using
  `Theme.grid_color`, and an optional red wash for Game Over. A second helper
  replaces the flat black pause/shop dim with a themed semi-transparent
  gradient.
- No new PNG, no `tools/gen_assets.py` change, no texture-count move, no
  `NET_ASSET_VERSION` bump, no gate-baseline re-baseline. This also avoids
  relying on `gen_assets.py --verify` while the Pillow NEAREST environment bug
  (`pillow-nearest-env-bug`) blocks that gate step.
- Theme source: full-screen menus (main, map select) use the map highlighted
  in the picker; in-game menus (pause, shop, game over) use the world actually
  being played. `menu_theme_for_state()` in `main.c` is the single source of
  that choice.
- No sim, ECS, asset or wire state is read or written; the drift clock is the
  caller's own menu timer, not `SDL_GetTicks()`, so the drawing is
  deterministic. Headless runs never call `render()`, so both SP event
  references must stay byte-identical.
- Visual quality is **unproven** until a human looks at every menu on all four
  themes at windowed and fullscreen sizes; text contrast is the acceptance
  criterion.
