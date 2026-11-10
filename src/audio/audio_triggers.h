#ifndef AUDIO_TRIGGERS_H
#define AUDIO_TRIGGERS_H

#include <stdbool.h>

#include "audio/audio.h"
#include "events/event_bus.h"

/* Which sound a gameplay event makes, from the trigger audit in
 * docs/knowledge/audio-plan.md §6. Deliberately stateless except for
 * GE_PLAYER_HEALTH (see below), so the whole table can be unit-tested: the
 * relay is what silently changes these payloads, and a table that drifts from
 * it is a silent defect. */
AudioSfx audio_sfx_for_event(const GameEvent *ev);

/* Sounds with no world position (wave start/end, score) play centred at full
 * volume; everything else is placed by `ev->x, ev->y`. */
bool audio_sfx_is_2d(AudioSfx sfx);

/* Forget the cached health used to tell hurt from heal. Called once per
 * session; entity ids are reused across maps, so a fresh game must not inherit
 * an old value. */
void audio_triggers_reset(void);

#endif
