# docs/ — index

**Two documents here are mandatory reading before you touch this codebase:**

| Read this | Before you |
|---|---|
| [`SYSTEM_DESIGN_RULES.md`](SYSTEM_DESIGN_RULES.md) | touch gameplay, rendering, ECS, or anything networked |
| [`VERSIONING.md`](VERSIONING.md) | add a system, wire field, asset, or map format |

They are short, they are not optional, and every rule in them exists because
something in this repo was already broken by breaking it once.

**For the animation and audio work, a third document is also mandatory:**
[`AGENT_RULES.md`](AGENT_RULES.md) — how two agents build in parallel without
stepping on each other, with the file-ownership matrix. It *defers* to the two
above rather than restating them.

**If you are an agent working alongside another agent**, you coordinate through
the `intercom_*` tools, not through files. `intercom_whoami` tells you who you
are and which worktree you own; `intercom_ask` reaches the other agent;
`intercom_inbox` shows what is waiting. The old `agents/` directory and
`AGENT_RULES.md` §11's handle-and-question-number protocol are **retired** —
`agents/*.md` is kept as read-only history only.

## Durable decisions — `knowledge/`

**Start here if you need to know *why* something is the way it is.** Each topic
is one subject, and each is the single place that fact is declared — cite the
topic rather than copying its contents into another doc.

| Topic | What it settles |
|---|---|
| `knowledge/worktree-layout.md` | where the worktrees are, and how agents talk now |
| `knowledge/gate-baseline.md` | **the exact numbers a green gate produces** |
| `knowledge/versioning-decisions.md` | which version moves when — and the "no wire bump is not no version bump" trap |
| `knowledge/relayed-events.md` | what crosses the wire, and what adding an event costs |
| `knowledge/animation-transport.md` | animation design A vs B — parked, spike-gated |
| `knowledge/netfile-ownership-and-freeze.md` | who may touch `net.h`, and the freeze protocol |
| `knowledge/i5-unverified-on-screen.md` | what is fixed in code but **unproven visually** |
| `knowledge/client-cannot-double-fire-events.md` | why a client cannot double-emit a relayed event |
| `knowledge/build-shared-dep-cache.md` | the shared dep cache, and why a feature branch re-downloads 1.5G |
| `knowledge/open-decisions-awaiting-pm.md` | **what is blocked, and on whom** |

Read them with `intercom_knowledge` (agents) or `intercom knowledge show
<topic>` (humans). Append with `intercom_save_knowledge` / `intercom --as user
knowledge save`. They are ordinary Markdown: edit and commit them like any doc.

## Where things are

### The two-agent parallel work (animation + audio)
| File | What |
|---|---|
| `AGENT_RULES.md` | **process rules for both agents**: file ownership, shared files, version contention, frame windows, merge order |
| `ANIMATIONS_AGENT.md` | **prompt file for the animations agent** — read this first if you are that agent |
| `AUDIO_AGENT.md` | **prompt file for the audio agent** — read this first if you are that agent |
| `ANIMATION_PLAN.md` | the sprite-animation plan: spike-first, the open frame-ownership question, phases |
| `AUDIO_PLAN.md` | the sound plan: vendored miniaudio, event-driven triggers, the gunfire gap |

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