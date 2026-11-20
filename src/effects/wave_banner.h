#ifndef WAVE_BANNER_H
#define WAVE_BANNER_H

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <stdbool.h>

/* E6: the wave transition banner.
 *
 * Screen space, not world space -- a wave starting is a statement about the
 * match, not about a place, so it does not belong in the text popup system
 * even though it is also floating text.
 *
 * It does not fade out. A banner that fades looks like the game forgot it was
 * there; this one shrinks and flies into the HUD's wave readout, so the thing
 * the player just read is visibly the same thing that now lives in the corner,
 * and it twinkles apart as it goes. That was PM's call on the transition and
 * it is the reason this is its own module rather than another popup style.
 *
 * Cosmetic only, and driven from the host's GE_WAVE_START (relayed to clients
 * verbatim), so both screens announce the same wave at the same moment.
 * Nothing here draws from the global zw_rand() stream: the twinkle directions are
 * fixed by shard index, so they are also identical on every machine.
 */

/* Show "WAVE n" (plus the zombie count, when the host reported one). Replaces
 * any banner still on screen: wave n+1 starting while wave n's banner lingers
 * must not queue up. */
void wave_banner_show(int wave_number, int zombie_count);

/* Clears any live banner. Called on reset so a new match does not inherit the
 * previous one's announcement. */
void wave_banner_reset(void);

void wave_banner_update(float dt);

/* No-op without a renderer or font, and when no banner is live. */
void wave_banner_draw(SDL_Renderer *renderer, TTF_Font *font, int screen_w,
                      int screen_h);

/* Diagnostics for the tests. */
bool wave_banner_active(void);
/* 0 while entering/holding, rising to 1 as the banner flies to the HUD. */
float wave_banner_exit_progress(void);
const char *wave_banner_text(void);

#endif
