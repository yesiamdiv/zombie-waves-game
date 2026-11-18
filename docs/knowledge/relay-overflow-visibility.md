# Relay batch overflow: making the silent loss visible

<!-- Project knowledge (Intercom-AI). Agents append entries; edit freely. -->

## 2026-11-18 - decision by net: S6-2 / N3 phase 1 is local visibility only, no wire field

`relay_net_events()` (`src/main.c`) sends at most `NET_EVENTS_MAX_BATCH` (48)
curated events per frame. Anything relayable left in the event ring at that
point used to be dropped with no trace when `event_bus_flush()` drained the
ring later in the same frame. That silent loss is what N3 was opened for.

**Phase 1 decision (approved by pm 2026-11-18): report, do not fix.** The cap
stays at 48, the wire is unchanged, and the host is still the single source of
truth. A count is a local diagnostic on whichever side overflows; it is never
replicated and a client never infers the host's backlog.

WHAT LANDED:
- **Host** — after the batch loop, `relay_net_events()` scans the remaining
  ring for relayable events with `sid > last_relay_sid`, adds them to
  `game.net_relay_overflow` (local `uint32_t`), and emits a rate-limited
  (2 s) `LOG_WARN` plus a host-HUD line `Net: relay batch overflow`.
  `last_relay_sid` is deliberately **not** advanced past those events: the
  existing retry behaviour is unchanged. The warning says which fate applies:
  with `--events` the frame's flush drops them; without a log file they stay
  in the ring and are retried next frame (deferred, not lost).
- **Client** — `NetClient.dead_dropped` counts death ids its own
  `dead_ids[NET_EVENTS_MAX_BATCH]` staging array had to drop, with the same
  2 s rate-limited warning. This is genuinely client-local: the next snapshot
  prunes those entities anyway, so it is a timing loss only, never state.

WHY NOT FIX THE CAP HERE: raising `NET_EVENTS_MAX_BATCH` changes a codec
limit and needs a wire/version decision (`docs/VERSIONING.md`), and it is not
free — the client stages a batch-worth of events too. Phase 2 options are to
raise it deliberately, split a frame across several packets, or coalesce
`GE_DAMAGE`; each is its own decision.

TRIGGER: more than 48 relayable events in one frame. MEASURED economics: a
zombie kill contributes 3 relayable events (GE_DAMAGE + GE_KILL + GE_POINTS;
zombie GE_ENTITY_DEATH is NOT in relay_event_type), plus GE_EXPLOSION and the
grenade's own GE_ENTITY_DEATH, so a blast needs **>=16 kills in one frame**
(16*3+2=50). A 12-kill blast is 38 and correctly stays silent. Grenades explode
at the first zombie contact, i.e. on the ring's edge, so a 17-zombie wave only
reaches ~13 kills; the trigger needs wave 5 (20) or 6 (23) fully packed.

SINGLE-PLAYER SAFETY: the first statement of `relay_net_events()` is
`if (!game.net_host_mode || !g_events) return;`, so none of this runs in SP.

VERIFIED on the merged Sprint-6 tree (2075/0 baseline):
- `zombie_tests` 2075 passed / 0 failed; `ctest` 3/3.
- Canonical unlimited SP `125938 bytes / 1603 lines`, two runs byte-identical;
  default limited SP `129504 bytes / 1648 lines`, two runs byte-identical.
- `gen_assets.py --verify` 50 textures; `gen_audio.py --verify` 13 sounds;
  `tools/net_coop_check.py` PASS.
- FORCED TRIGGER (S6-5, 2026-11-18, after B47): a grenade blast into a packed
  wave-5 horde produced exactly one host warning -
  `NET: relay overflow - 10 event(s) dropped at this frame's flush (max 48/frame, 10 total)`;
  the `--events` frame 35039 holds 58 relayable events (18 KILL) => 48 kept, 10
  dropped, counter total 10, no assertion, run continued into wave 6.
  Evidence: `build/logs/qa_n3_burst9.{stdout,events.log}`.
- Recipe that works (do not re-derive): buy 8 grenade packs, let waves 1-4 TIME
  OUT untouched (leftovers are force-destroyed, so waves 5/6 spawn fresh and
  fully), `--player-damage-mult=0.00001` so the thrower never dies, then a
  burst of throws at the wave-5 tail (~t=290-310, 20 zombies pressed tight).
  Clearing earlier waves with grenades shrinks the pack and misses the cap.
- Remaining `unproven`: the HUD text on a real display. It is queued in the same
  branch as the log line (`src/main.c:1536-1547`), so only rendering is unseen.
