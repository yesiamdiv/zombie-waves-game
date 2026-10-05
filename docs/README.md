# docs/ — index

**Two documents here are mandatory reading before you touch this codebase:**

| Read this | Before you |
|---|---|
| [`SYSTEM_DESIGN_RULES.md`](SYSTEM_DESIGN_RULES.md) | touch gameplay, rendering, ECS, or anything networked |
| [`VERSIONING.md`](VERSIONING.md) | add a system, wire field, asset, or map format |

They are short, they are not optional, and every rule in them exists because
something in this repo was already broken by breaking it once.

## Where things are

### Plans and current work
| File | What |
|---|---|
| `NET_SPRINT_PLAN.md` | sprints N1–N4 (net protocol) and their acceptance criteria |
| `NET_PROTOCOL_DESIGN.md` | the host-authoritative design, decision log (ADR-1..6), art table |
| `MULTIPLAYER_PLAN.md` | the original multiplayer design |
| `MULTIPLAYER_PROGRESS.md` | multiplayer status log |
| `DEV_PLAN.md` | developer-facing plan |
| `SPRINT_PLAN.md` | assets/maps sprint 3 (closed) |
| `ASSET_MAP_PLAN.md` | asset and map plan |
| `FUTURE_IDEAS.md` | backlog, including the zombie speed/size variants |

### The durable record
| File | What |
|---|---|
| `BUGS.md` | B1–B23+ with symptom / root cause / fix / commit SHA |
| `I5_FIX.md` | **how the "client renders dots, not sprites" fix works** — the rejected obvious fix, and why guessing on the client is an authority violation |
| `R13_BUGFIX_TRACKER.md` | the multiplayer fix series and its gates |
| `PENDING_VERIFICATION.md` | **believed working but unproven** — read before claiming a visual fix |
| `PROGRESS.md` | session log: what was found and done |
| `MERGE_DECISIONS.md` | cross-branch merge decisions |
| `FEATURES.md` | feature sheet (F1–F12) |

### Reference
| File | What |
|---|---|
| `EVENT_FORMAT.md` | `GameEvent` ring buffer and wire encoding |
| `EXTENDING.md` | how to add content |