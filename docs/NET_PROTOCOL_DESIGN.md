# NET Protocol Design — appearance replication, compatibility gating, host authority

Branch: `feature/net-protocol` (forked from `feature/multiplayer` @ `398f8d1`).
Author: DEV. Scope agreed with PM. Companions: `docs/NET_SPRINT_PLAN.md`,
`docs/BUGS.md`.

## 0.1 Decision log

Choices made during planning, with the reasoning, so they survive chat
compression and are not quietly re-litigated mid-implementation.

| # | Decision | Rationale | Status |
|---|---|---|---|
| ADR-1 | The host transmits appearance; the client never derives it | Appearance is host-owned state. Deriving it is the root cause of R13-I5, and any heuristic is a future desync waiting to happen. | accepted |
| ADR-2 | **Art ids are a shared numbered table, not texture names on the wire** | A per-entity name would repeat ~8 bytes at 20 Hz per entity to say something both builds already agree on. The table costs zero bytes and makes new artwork a one-line append. The cost is that the table must match across builds — which is exactly what `NET_ASSET_VERSION` (Sprint N2) enforces, turning a silent visual mismatch into a loud connection refusal. | **accepted by PM 2026-10-03** |
| ADR-3 | `size_q` is quantised to half units, not a float | Integers decode bit-identically on every client; a float invites "close enough" drift. Half-unit precision on a 10–16 unit sprite is already finer than a pixel. | accepted |
| ADR-4 | `tint` is the host's literal RGB, not a theme index | A theme index would force the client to re-derive a colour from local theme state — precisely the derivation this document exists to delete. It also makes the client correct even if its theme lookup were ever wrong. | accepted |
| ADR-5 | Payload growth 26 → 31 bytes/entity is accepted | The requirement is that the protocol stays correct as art changes; that is worth 19% on a field that is already the smaller half of a snapshot. | accepted by PM 2026-10-03 |
| ADR-6 | Snapshot MTU/fragmentation is **not** addressed here | Pre-existing: the 1024-entity cap already produced a ~26 KB packet before this change. Mixing it in would confound the desync work. Deferred, tracked in the sprint plan backlog. | deferred |

### Art table (ADR-2)

Owned by the host, resolved by both peers from the same table:

```c
enum {
    NET_ART_NONE    = 0,   /* host is drawing a flat circle: reproduce it */
    NET_ART_PLAYER  = 1,
    NET_ART_ZOMBIE  = 2,
    NET_ART_BULLET  = 3,
    NET_ART_GRENADE = 4,
    NET_ART_ROCKET  = 5,
    NET_ART_MEDKIT  = 6,
    NET_ART_AMMO    = 7,
    NET_ART_SPEED   = 8,
    NET_ART_SWORD   = 9,
};
```

Rules:

- `net_art_path(id)` maps an id to the asset path. **Ids are append-only** — a
  released id never changes meaning, so a stale peer that does not recognise an
  id renders `NET_ART_NONE` (a circle) instead of the wrong sprite.
- Adding artwork appends one enum value and one table row. No struct change, no
  wire-version change.
- `NET_ART_NONE` is load-bearing, not a placeholder: it is how the host says
  "this entity really is a flat circle", so the client is faithful rather than
  guessing.

## 0. The governing rule (why this document exists)

**The host is the single source of truth. A client may only consume host state.**

A client is allowed to add computation on top of host data **only** when that
computation is *cosmetic* and *deterministic across different clients, hosts and
systems*. Anything else is a latent desync bug.

Three classes of violation exist in the current code:

| Class | Instance | Verdict |
|---|---|---|
| Client invents a value the host owns | `system_render_mirror()` invents sprite size/colour/shape (R13-I5) | **Violation** — appearance is host-owned |
| Client reads its own un-simulated state | client HUD reads local ECS health + local inventory | **Violation** — host owns HP/points/ammo |
| Cosmetic + deterministic | mirror interpolation, camera follow, slot tint | Allowed |

The rule is written down here because R13-I5 survived nine commits precisely
because "the client looked plausible" was never expressed as a requirement.

## 1. Sprint N1 — appearance over the wire

### Problem

`NetEntitySnap` is 26 bytes and carries `{id, kind, pos, vel, hp, flags, owner}`.
It says *what* and *where*, never *what it looks like*. `system_render_mirror()`
therefore guesses:

```c
/* src/systems/render.c:152 — the whole bug */
Sprite s = sprite_circle(size, color);
```

with invented per-kind sizes. Two hosts can disagree with the client about
zombie size (`10 + rand()%6`), zombie variant tint (a `waves.c` local that never
reaches the ECS), and pickup subtype (all three pickups share `NET_ENT_ITEM`).

### Decision: send appearance explicitly

The user's requirement is that "what it looks like" is transmitted, and that
growing the payload is acceptable *because it future-proofs the protocol*. So we
send an explicit appearance descriptor per entity rather than deriving it.

`NetEntitySnap` gains five bytes:

```c
typedef struct {
    uint16_t id;
    uint8_t  kind;
    Vec2     pos;
    Vec2     vel;
    float    hp;
    uint8_t  flags;
    uint16_t owner;      /* unchanged */
    uint8_t  art;        /* NET_ART_* texture id, 0 = none/fallback circle */
    uint8_t  size_q;     /* sprite diameter, quantised to half units       */
    uint8_t  tint[3];    /* exact RGB from the host's CSprite              */
} NetEntitySnap;         /* 26 -> 31 bytes */
```

Rationale per field:

- **`art`** — an index into a *shared art table* (`NET_ART_*`, resolved by
  `net_art_path()` in `src/net/`). This is the future-proof axis: new artwork
  means a new table entry, not a new field or a new heuristic. `0` means the
  host is drawing a plain circle, so the client faithfully reproduces a circle.
- **`size_q`** — quantised to half units so `size = size_q / 2.0f`. Covers
  0–127.5 world units in one byte. An integer on the wire means every client
  decodes bit-identical geometry; a float would too, but this is smaller and
  the precision is irrelevant for a 10–16 unit sprite.
- **`tint[3]`** — the host's literal `CSprite.color`. Sending the resolved RGB
  (rather than a theme index) keeps the client free of theme-lookup logic, which
  is exactly the sort of derivation this document is trying to delete.

`NET_SNAP_ENTRY_BYTES` becomes **31**.

### Consequences, stated honestly

- Snapshot payload grows ~19% per entity (`NET_SNAP_MAX_BYTES` 26,657 → 31,769
  at the 1024-entity cap). This is a *pre-existing* condition: the cap already
  exceeded any sane MTU long before this change. Real sessions carry tens of
  entities, not 1024. **Not** addressed here; tracked separately.
- `NET_WIRE_VERSION` **must** be bumped 3 → 4. A v3 peer reads a 31-byte entry
  as a 26-byte one and desyncs immediately. This is not optional.

### Host side (authoritative)

`net_snapshot.c` fills `art`, `size_q`, `tint` from the entity's live `CSprite`.
Where the texture id cannot be derived from the path, the entity's own type
authorises it (`CItemTag.type` → `NET_ART_MEDKIT`/`AMMO`/`SPEED`). No guessing
anywhere; if the host has no sprite, it sends `art = 0`.

### Client side (obedient)

`system_render_mirror()` builds its sprite from `art`, `size_q` and `tint`
verbatim. The hardcoded size table and the hardcoded colour choices are deleted.
`rand()` may not appear in the mirror path.

## 2. Sprint N2 — compatibility gating

### What already exists (do not rebuild)

- `NET_WIRE_VERSION` (3) and `NET_WORLD_GEN_VERSION` (2).
- Host rejects a mismatched `JOIN` with `NET_REJECT_VERSION` (`net_server.c:258`).
- Client rejects a mismatched `HELLO` with `NET_REJECT_VERSION` /
  `NET_REJECT_WORLD` (`net_client.c:212`), and refuses an unknown `map_index`
  rather than silently desyncing terrain.

So the "exchange version, then decide" mechanism the user asked for is
structurally present. Sprint N2 closes the three real gaps:

1. **No asset/content version.** Two peers built from different asset trees
   render different pictures — precisely the failure class of B15–B18, where a
   missing texture became a silent flat circle on one side. Nothing detects it.
   Add `NET_ASSET_VERSION` to the `HELLO` body (appended after `map_index`, so
   the body grows rather than shifting) and reject on mismatch with a new
   `NET_REJECT_ASSET` reason and `NET_FLAG_ASSET_MISMATCH`.
2. **Rejection is invisible to the player.** The client only `LOG_ERROR`s and
   returns to the menu; a human sees nothing and concludes the game is broken.
   Surface a readable message naming both versions and the remedy.
3. **The host never learns it refused someone.** Add host-side feedback for a
   rejected peer.

Bump `NET_WIRE_VERSION` again (4 → 5) for the `HELLO` body change.

## 3. Sprint N3 — host-authoritative client state

Audit findings, both confirmed by reading the code:

### A. Client HUD health is the client's own stale ECS

`src/ui/hud.c:140` reads `ecs_get_health(ecs, local->entity)`. A render-only
client never simulates, so that entity is never damaged — the client's own HP
readout is whatever the local ECS was initialised to. The authoritative value is
already on the wire: `NetEntitySnap.hp`. Fix: the client HUD reads HP from the
mirror entity for its own slot.

### B. Client HUD inventory is the client's own stale inventory

`src/ui/hud.c:137` reads `players[0].inventory` for points, current weapon,
grenades and launcher ammo. The client never simulates pickups, so all four are
stale. The host already relays `GE_POINTS`, `GE_ITEM_PICKUP` and `GE_KILL`
(`main.c:1214`), but `drain_net_events()` only logs them — it never applies them
to anything. Fix: apply the relayed events to a client-side display inventory.
This stays host-driven: the client is a display, not a simulator.

### Explicitly *not* violations

Mirror interpolation and camera follow are consumed host data with cosmetic,
frame-rate-dependent smoothing. They stay.

## 4. Verification

Every sprint is gated by the standing rules in `AGENTS.md`:

1. Clean Release build, `-Werror`, zero warnings in project sources.
2. `ctest` 3/3 (`net_spike`, `net_loopback`, `core_tests`).
3. `./build/zombie_tests` green.
4. **Single-player byte-identity** — two same-seed headless runs are `diff`-empty,
   and wave 1 is still `8 zombies, interval 1.90s, difficulty 1.00`. All three
   sprints are multi-player-only and must stay behind `player_count > 1`; if a
   change perturbs single-player it is wrong, not merely unlucky.
5. Net-specific: `test_net_codec` gains appearance round-trip coverage, and
   loopback asserts the host rejects a stale `NET_WIRE_VERSION` peer.

**The honesty rule:** items 1–5 cannot see a rendering defect. `render()` returns
immediately when `game.headless` is set. Anything about appearance or HUD text
is *unproven* until a real two-instance on-device run
(`--host` + `--join`, two windows) is done by a human. Such items are recorded
in `docs/PENDING_VERIFICATION.md` and must not be reported as fixed on the
strength of a headless gate.