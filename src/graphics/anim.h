#ifndef ANIM_H
#define ANIM_H

#include <stdint.h>

#include <SDL3/SDL.h>

#include "net/net_art.h"

/* Animation frames travel to a client as ART IDS - not as paths, and not as a
 * new wire field (docs/knowledge/animation-transport.md). An art id is
 * resolved from a texture's path, so a frame needs a file of its own to be
 * namable at all; that is why the walk cycle is eight PNGs and not a sheet.
 *
 * There is ONE function here that maps a frame number to what the world
 * shows. The host renderer and the snapshot builder both go through it, so
 * there is exactly one answer to "which frame is this entity in" - a second
 * copy of that mapping is exactly how two peers end up disagreeing. */

/* How many frames the zombie walk cycle has. Taken from the art table rather
 * than written down again: the ids and the frame count must not drift apart. */
#define ANIM_WALK_FRAMES NET_ART_ZOMBIE_WALK_COUNT

/* How long one frame lasts, in seconds. 0.1 s = 10 fps.
 *
 * THE CEILING IS 15 fps, and it is not negotiable: the snapshot stream is
 * 20 Hz, so a frame period shorter than the snapshot interval makes the
 * client skip frames, and the skip pattern is what the spike measured
 * (docs/knowledge/animation-transport.md). AN4 pins that rule with a test so
 * nobody can quietly speed the cycle up. */
#define ANIM_FRAME_PERIOD 0.1f

/* THE RULE, as a number: the frame period must be at least 1/ANIM_MAX_FPS
 * seconds. 15 fps is the measured ceiling - at 15 fps and slower every cycle
 * reaches the client with all 8 frames, at 24 fps a quarter of the steps are
 * skipped and the walk pops. `test_animation_rate_rule` checks the rule
 * behaves, not just that the constant is big enough. */
#define ANIM_MAX_FPS 15
#define ANIM_FRAME_PERIOD_MIN (1.0f / (float)ANIM_MAX_FPS)

/* The art id of frame `frame` of the zombie walk cycle. Wraps around. */
uint8_t anim_walk_art_id(int frame);

/* The texture path of that frame, or NULL when the id is unknown. */
const char *anim_walk_path(int frame);

/* The texture of that frame, or NULL in headless builds and when the art is
 * missing. Resolved through the asset manager, so it is cached. */
SDL_Texture *anim_walk_texture(int frame);

#endif /* ANIM_H */
