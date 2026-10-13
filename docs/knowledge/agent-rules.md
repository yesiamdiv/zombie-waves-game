# Two agents, one codebase — parallel-work rules

<!-- Project knowledge (Intercom-AI). Agents append entries; edit freely. -->

## 2026-10-13 14:20 UTC - note by assets: MIGRATED from docs/AGENT_RULES.md (owner D7). The rules for two agents building at once.

Status: MANDATORY for the animation and audio work. These are the rules for
building in parallel; *how code is written* stays with `SYSTEM_DESIGN_RULES.md`
(design) and `VERSIONING.md` (numbers). This topic does not restate a rule from
either — where it and a design rule disagree, the design rule wins.

## 1. The shape of the work

Two feature branches cut from a clean `main`, worked in parallel, merged in a
fixed order:

```
main ──┬─────────────────────────────┬──────────────► main
       │                             │
       ├── feature/animation-assets  │   animation agent
       └── feature/audio             │   audio agent
                                     │
                                     └── animation merges first, audio rebases onto it
```

- Agents: `animation` → `worktrees/animation` → `feature/animation-assets`;
  `audio` → `worktrees/audio` → `feature/audio` (owner D5, 2026-10-13).
- Cut **only** from a `main` with a clean worktree and a green gate. Do not cut
  from a moving target; do not commit somebody else's half-finished files.
- Each agent works in its own worktree. Never edit another agent's worktree;
  never `git add -A` in a shared tree. Branch/worktree mechanics: [worktree-layout].

## 2. File ownership

Three buckets: **yours only**, **shared (expect a conflict)**, **never**.

### Yours only

| Animations | Audio |
|---|---|
| `src/graphics/anim.*` *(new)* | `src/audio/**` *(new)* |
| `src/graphics/sprite.*` | `vendor/miniaudio.*` *(new)* |
| `tools/gen_assets.py` | `tools/gen_audio.py` *(new)* |
| `assets/textures/**` | `assets/audio/**` *(new)* |

### One file is split: `src/systems/render.c`

| Function | Nature | Belongs to |
|---|---|---|
| `system_render` | pure draw, host/SP | animations |
| `system_render_mirror` | pure draw, client — where the I5 fix landed | animations |
| `system_render_beacons` | pure draw | animations |
| `system_animation` | advances `CAnimation` from `dt`; no RNG, no events | animations |
| `system_cleanup` | **simulation.** Emits `GE_KILL`/`GE_ENTITY_DEATH`, awards kills, spawns items/particles, calls `rand()` | **neither — read-only** |

Two consequences of `system_cleanup`:

- It **consumes the seeded RNG**, so its rolls are part of the single-player
  byte-identity gate ([gate-baseline]). Do not delete or "de-duplicate" a
  `rand()` call — that shifts every later draw and fails the gate even though
  behaviour looks identical. Byte-identity is not equivalent.
- It is **where audio's primary triggers are born** (`GE_KILL`,
  `GE_ENTITY_DEATH`). Animations touching it would break audio invisibly to every
  gate. If you must touch it, say so in the audio plan topic first.

### Shared — you will conflict, and that is fine

| File | Discipline |
|---|---|
| `src/main.c` | one line per system, at the designated slot (§4); keep the diff to a stub |
| `src/systems/systems.h` | one declaration per block, grouped by system |
| `CMakeLists.txt` | add only your own files; never reformat or reorder |
| `src/net/net.h` | version registry; read `VERSIONING.md` first; **never** bump to settle a merge |
| `docs/BUGS.md`, `docs/FEATURES.md`, `docs/PROGRESS.md`, `docs/PENDING_VERIFICATION.md` | append-only, in your own section; never renumber someone else's rows |

Conflict rule for shared files: **re-do your own change on top of theirs.**
Never take the merge's automatic resolution of a line you own, and never resolve
by reverting the other agent's addition.

### Never

- **`src/net/**`, with two named exceptions.** Not a netcode agent → a wire
  change is a desync waiting for an older peer. Exceptions:
  - `src/net/net_art.h` / `net_art.c` — **animations appends art ids here.** One
    line, no wire bump; do not reorder/renumber/resize existing ids, do not move
    `NET_ART_NONE`, keep `base_px` equal to the real PNG width. Audio has no
    business here.
  - `src/net/net.h` — version registry, already listed shared above.
  If you think you need anything else under `src/net/`, stop and read
  `I5_FIX.md` §7, then raise it in your plan topic.
- `playtesting_prompts/**` — owned by the playtest agent, left untracked.
- Any worktree that is not yours.

## 3. Version-number contention

Both agents add files under `assets/`, and the registry moves `NET_ASSET_VERSION`
for added assets. **Decided 2026-10-06: audio moves no version at all** — no
`NET_ASSET_VERSION` bump and no `NET_AUDIO_VERSION`; animation *does* bump it for
art ids. Full reasoning and the trap ("no wire bump" ≠ "no version bump"):
[versioning-decisions].

The rule if two numbers collide: it is bumped once per agent, in that agent's own
commit, and **never bumped to resolve a merge conflict** — take *their* value,
re-apply your own bump on top as a follow-up commit naming the version and why
(`VERSIONING.md` §5.2-3). Sequence if both bump: `1 → 2` (animation); audio does
not participate. Never reuse a retired number.

`NET_WIRE_VERSION` is off limits to both. Animation state on the wire is an open
design question owned by the animation spike ([animation-transport]) — not
something to add quietly while implementing frames.

## 4. Where new code runs in a frame

Update order is a contract (`SYSTEM_DESIGN_RULES.md` §8), so the insertion
points are fixed here, before either agent writes code:

```
update(dt)                          <- simulation; ends with system_cleanup()
drain_mirror()
drain_net_events()                  <- client pulls mirror + HUD text
broadcast_snapshots(dt)
event_bus_tick() / event_bus_samples()
relay_net_events()                  <- host reads the ring, relays a curated subset
event_bus_flush()                   <- THE RING IS DRAINED HERE (main.c:1504)
```

**Animations: no new tick.** `system_animation(World*, float dt)` already exists
(`src/systems/render.c`) and is already called at `main.c:817`, between
`system_bullets` and `system_particles`, before `system_cleanup`. It consumes no
RNG and emits no events, so its position is correct — do not move it. If a design
needs a *second* tick (e.g. after the mirror is drained on a client), that is a
new insertion and belongs in the reserved window below.

**Audio: the only permitted window is before `event_bus_flush()`.**

```
relay_net_events();        <- main.c:1503
AUDIO_TICK;                <- INSERT HERE: after relay, before flush
event_bus_flush(g_events); <- main.c:1504
```

Triggers are emitted during `update()` (`GE_KILL` from `system_cleanup`) and the
flush destroys the ring the same frame. Anything later reads nothing; anything
earlier misses this frame's kills. The host already walks the ring at exactly
this point with a `sid` watermark; audio does the same in the same window. Do not
move the flush. Neither agent may call `rand()` anywhere (`SYSTEM_DESIGN_RULES.md`
§5). Declare the insertion point in a comment and say why.

If a system does not fit its window, that is a finding: raise it in your plan
topic rather than silently moving next to a simulation system.

## 5. The gate

Rationale: `SYSTEM_DESIGN_RULES.md` §3-§4. The commands, and the fact that
**asserts** must stay in `Debug`, are operational; the numbers (test count,
texture count, SP events-log byte/line reference, wave-1 string) are declared
once in [gate-baseline]. **Cite that topic — never copy the numbers.** Run the
same commands in `Release` too when a phase's gate is set.

**A change that cannot pass the gate is not ready to commit — including when the
gate is inconvenient** (a visual/audio feature on a headless gate is the normal
condition, not an exemption). Write `unproven`, not `fixed`, until a human has
looked or listened.

## 6. Growing the asset tree

- **Generate, don't hand-edit.** `tools/gen_assets.py` owns every texture;
  `tools/gen_audio.py` will own every sound. Hand-editing a generated file means
  the next `--verify` silently reverts it.
- **`--verify` must leave `git status` clean.** A dirty tree means the generator
  is not deterministic — fix the generator, not the PNG.
- **Filenames are the API** (`EXTENDING.md`): renaming is a breaking change;
  adding is not; deleting one is a `NET_ASSET_VERSION` bump and a grep for
  callers first.
- **`base_px` in `src/net/net_art.c` must match the real PNG width** — a mismatch
  renders the right sprite at the wrong size on a peer you will never see.
- **New art ids are one-line appends** and need no *wire* bump — but "no wire
  bump" is not "no version bump". Append the id **and** bump `NET_ASSET_VERSION`
  in the same commit. Details: [versioning-decisions].
- **Headless must still work.** `sprite_tex()` returns NULL with no device / a
  missing file; every call site needs a flat-colour fallback of the same size and
  tint. Audio: with no device, playback is a no-op — no block, no per-frame
  allocation, no assert.

## 7. Document duties

Both agents keep the durable record current *in the same series as the code*;
each file has one owner-at-a-time, both append, nobody rewrites.

| Record | Animations writes | Audio writes |
|---|---|---|
| `docs/BUGS.md` | render/sprite/art findings | audio/menu/HUD findings |
| `docs/FEATURES.md` | **F15 — sprite animation** | **F16 — audio** |
| `docs/PENDING_VERIFICATION.md` | what a human must look at | what a human must listen to |
| `docs/PROGRESS.md` | standup rows | standup rows |
| plan topic | [animation-plan] | [audio-plan] |

Format: `symptom / root cause / fix / status + commit SHA`, added *after* the
commit exists. Chat is not the record.

## 8. Cadence

- **One commit per unit of work.** Never batch two unrelated fixes.
- **Rebase onto `main` on a schedule** — at minimum at the end of every phase and
  before any spike verdict is written down.
- **Commit the plan before implementing it.** The spike's verdict goes into the
  plan topic before the implementation branch starts.

## 9. Merge order

**Animations first. Audio rebases on top** — reasons in [animation-plan] §9, not
negotiable at merge time: animation is larger and owns the render path the I5 fix
just landed in; audio's only shared files are `main.c`, `systems.h`,
`CMakeLists.txt`. Each agent merges their own branch after their gate passes,
with a house-style merge commit; the second rebases before starting work.

## 10. The one-question check

Before committing something that crosses a file boundary, ask:

> **"Whose file is this, and would the other agent be surprised to find my code
> in it?"**

Yes to the second half → you are in the wrong bucket. Move the change or say so
in your plan topic first.

## 11. Talking to another agent

Coordination is through the `intercom_*` tools only (`intercom_agents`,
`intercom_ask`, `intercom_inbox`/`intercom_reply`, `intercom_whoami`). **You no
longer poll** — you are woken when a message arrives; an unanswered request
escalates to the manager automatically. The old file channel (`agents/*.md`,
§11 below, `tools/agent_channel.sh`, `scripts/watch_agents.sh`) is retired; see
[worktree-layout]. `agents/*.md` is read-only history.

The principle survives: **the channel is how you reach an answer; it is not where
the answer lives.** Durable decisions go into `docs/knowledge/<topic>.md` via
`intercom_save_knowledge`. Messages carry coordination (ownership, timing,
sequencing, "may I touch your file"), not decisions.

**Why:** net.h and the codec test are the two files the agents would collide in,
and there is no merge to fall back on for a shared constant — so ownership and
insertion windows are fixed in advance rather than discovered at review.
