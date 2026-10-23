#ifndef AUDIO_H
#define AUDIO_H

#include <stdbool.h>

#include "events/event_bus.h"

/* Sound slots, one per row of the trigger audit (audio-plan §6). The enum is
 * the contract: Phase C fills the slot -> file table, Phase D fills the
 * event -> slot mapping. A slot with no file is silent, never an error.
 *
 * Deliberately absent: reload (the game has no reload mechanic), out-of-ammo
 * (no event is emitted), shop/spawn (those events are host-only, audio-plan
 * §5). Do not add slots for triggers that do not exist. */
typedef enum {
    AUDIO_SFX_NONE = 0,
    AUDIO_SFX_SHOT_BULLET,
    AUDIO_SFX_SHOT_GRENADE,
    AUDIO_SFX_SHOT_ROCKET,
    AUDIO_SFX_HIT_ZOMBIE,
    AUDIO_SFX_HIT_PLAYER,
    AUDIO_SFX_EXPLOSION,
    AUDIO_SFX_DEATH_ZOMBIE,
    AUDIO_SFX_DEATH_PLAYER,
    AUDIO_SFX_WAVE_START,
    AUDIO_SFX_WAVE_END,
    AUDIO_SFX_PLAYER_HURT,
    AUDIO_SFX_PICKUP,
    AUDIO_SFX_POINTS,
    AUDIO_SFX_COUNT
} AudioSfx;

/* Open the device and load the sound slots. Headless never opens a device and
 * never allocates: it returns false and every call below becomes a no-op.
 * A missing device or missing file leaves the game silent and running. */
bool audio_init(bool headless, bool test_cue);
void audio_shutdown(void);

/* Read the newly-emitted gameplay events and queue their sounds. Must run
 * before event_bus_flush() drains the ring (main.c step_frame): after the
 * flush the ring is empty, and GE_KILL is emitted from system_cleanup.
 * `listener_*` is the camera centre, used for distance falloff and pan, and
 * `view_*` is the visible world size, which sets the audible radius (so a
 * resized window does not change the mix). */
void audio_tick(EventBus *events, float listener_x, float listener_y,
                float view_w, float view_h);

bool audio_available(void);
const char *audio_sfx_name(AudioSfx sfx);

#endif
