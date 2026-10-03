# SYSTEM_DESIGN_RULES.md — rules for building systems that don't break each other

**Status: MANDATORY. Read this before touching gameplay, rendering, or anything
that crosses the network.**

Companion: `docs/VERSIONING.md` (how to number things). Both are binding on any
agent working in this repo.

These rules exist because this project has been broken five times in five
different ways by code that was *locally correct* and *globally wrong*. Every
rule below is one of those five, written down so it is not rediscovered by
breaking it again.

---

## 1. The authority rule (multiplayer)

> **The host is the single source of truth. A client may only consume host
> state.**

A client may compute something itself **only** if it is *cosmetic* **and**
*deterministic across different clients, hosts and systems*. Anything else is a
latent desync bug.

This is not theoretical. The client HUD reads:

| Value | Source the client used | Correct source | Symptom |
|---|---|---|---|
| Own health | its own local ECS (`hud.c:140`) | `NetEntitySnap.hp`, already on the wire | health bar never falls while dying |
| Points / weapon / ammo | its own local inventory (`hud.c:137`) | host-relayed `GE_POINTS`/`GE_ITEM_PICKUP` | readouts never update |

And the client's renderer invented a circle size per entity kind (R13-I5) while
the host drew real sprites.

The pattern to notice: **every one of these was plausible-looking.** Nothing
crashed, nothing logged an error, the numbers were merely wrong. A client's
guessed value is always more convincing than an absent one, which is exactly why
guessing is banned.

**Allowed client-side computation** — mirror interpolation, camera smoothing,
and slot→colour lookup. All three consume host data, differ only in frame
timing, and affect nothing but pixels.

**The test to apply before writing any client-side calculation:** *if the host
were deleted right now, would this code produce a different answer on two
different machines?* If yes, it is inventing state.

## 2. Classify every new value: replicated, derived, or owned

When you add state, decide which it is **in the same commit**, and say so in a
comment:

- **Replicated** — the host decides; it crosses the wire. Size, colour, health,
  position, points, wave number.
- **Deterministically derived** — either side may compute it; both get the same
  answer. Art ids, texture paths, palette lookups, wave budget from player
  count.
- **Client-only presentation** — genuinely per-peer. Camera position, which
  weapon key you pressed.

If a value does not fit, that is the finding. Do not leave it unclassified.

## 3. Single-player invariance

> **Multiplayer-only behaviour stays behind `player_count > 1`.**

This is the branch's entire charter and the gate that proves it. When you add
anything networked:

- `if (player_count > 1)` is the *only* correct way to branch. Not a build flag,
  not "it happens to work out".
- The seed-RNG call order must not change on the single-player path. A single
  added `rand()` desyncs every later roll and fails the byte-identity gate.
- Headless runs deliberately keep the fixed classic 50x50 world (`world_init`),
  never a map file, so map selection cannot perturb the sim. **This looks like
  dead code to fix. Do not "clean it up"** — it is what makes the gate work.
- The gate is two same-seed runs that are `diff`-empty, plus wave 1 being exactly
  `8 zombies, interval: 1.90s, difficulty: 1.00`. Both must pass before commit.

## 4. The visibility blind spot (why bugs survive here)

> **`render()` returns immediately when `game.headless` is set. No automated gate
> in this repo can see a visual or audible defect.**

Every check available — `ctest`, `zombie_tests`, determinism diffs — runs
headless. R13-I5 survived nine commits and a full playtest round because no gate
could see it, and the user found it by looking at a second window.

Therefore:

- Any change to rendering, HUD text, colours, or audio is **unproven** when the
  gates pass. Say "unproven", not "fixed".
- Record it in `docs/PENDING_VERIFICATION.md` with what specifically a human must
  look at.
- Structural assertions (a shared helper returns the same value on both sides)
  are worth writing precisely because they are the only visual-adjacent thing CI
  *can* check.
- A headless signal that does exist and is worth watching: host stdout
  (`Spawned remote player entity for slot N`, `WAVE n STARTED`), client stdout
  (`CLIENT mirror seq=... ents=N`), and the client's `--events` log
  (`WAVE_START`, `KILL`, `POINTS`, `DAMAGE`, `ITEM_PICKUP`).

## 5. Determinism discipline

- **`rand()` in a replicated visual path is a bug.** The mirror renderer must
  contain none. Audit for it when touching rendering.
- Randomness that affects gameplay is resolved **once, on the host**, then
  transmitted — never re-rolled per client. Zombie size and colour variant are
  the worked examples: both were `rand()`'d into a local that never reached the
  wire.
- Values on the wire that affect appearance should be integers where possible.
  An integer decodes bit-identically everywhere; a float invites "close enough"
  drift into.
- Iterate entities in a stable order. Entity ids are ECS array indices and are
  relied upon as stable keys in the client mirror.

## 6. Adding a component or an entity kind

Checklist — every line has been a real bug at least once:

- [ ] `ECS_MAX_ENTITIES` and the component mask widened if needed
- [ ] Spawn sets **every** field the renderer will read; no uninitialised
      `CSprite` (`B9` was an uninitialised stack `GameWorld` crashing tests)
- [ ] Snapshot: does it cross the wire, and is that deliberate?
- [ ] Cleanup: freed, or the world leaks tiles per level (`B11`)
- [ ] `system_render_mirror` can draw it *without* inventing anything
- [ ] Multiplayer: is it host-authoritative per §1?
- [ ] Single-player unchanged per §3
- [ ] `docs/FEATURES.md` updated

## 7. Assets

- **Never assume a texture loaded.** `sprite_tex()` returns NULL headless and on
  a missing file. Every call site needs a flat-colour fallback, and the fallback
  must keep the same size and tint so nothing shifts when art appears.
- The asset manager is a global set by `asset_manager_init()`. If lookups
  silently return NULL, check it was initialised (`B14`: it was never called, and
  *every* sprite in the game rendered as a flat shape).
- `base_px` in the net art table must match the real PNG's width. It converts a
  wire diameter into a draw scale.
- Changing anything in `assets/` requires a `NET_ASSET_VERSION` bump, so two
  peers with different art refuse to connect rather than rendering differently.

## 8. Interfaces between systems

- **Update order is a contract.** Systems in `main.c` run in a fixed order and
  depend on it (`system_player_input` before `system_movement` before
  `system_collision`). Adding a system means choosing its position deliberately
  and saying why in a comment.
- Pass the slot, never a global. Pickups and weapons must resolve against *that*
  player's inventory, not player 0's — see the per-slot loops in `update()`.
- Read the real occupied slot count (`players_active_count()`), never
  `MAX_PLAYERS`. Passing the constant made 4-player wave budgets run in
  single-player (`R13-C1`).
- Keep `headless` paths allocation-free and side-effect-free.

## 9. Process

- **One commit per fix.** Never batch unrelated fixes; if two fixes touch one
  file, stage them in separate passes.
- **Bump versions in the same commit as the change** — see `VERSIONING.md` §5.
- Update the bug sheet in the same series, with the commit SHA, *after* the
  commit exists. The sheet is the durable record; chat is not.
- Prefer fix-forward. Never rewrite history that has been pushed.
- If a change cannot pass the gate, it is not ready to commit — including when
  the gate is inconvenient.

## 10. The one-question check

Before you commit a change that touches shared state, ask:

> **"Who owns this value, and would a second instance of this program agree with
> the first?"**

If you cannot answer both halves in one sentence, you have found the next bug.
It will not be found by a test, because the test cannot see it.