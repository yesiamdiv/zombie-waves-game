#ifndef TEXT_POPUP_H
#define TEXT_POPUP_H

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <stdbool.h>
#include <stdint.h>

#include "world/camera.h"

/* E1: the shared floating-text system. Damage numbers (E2) and points/pickup
 * popups (E3) are both consumers of it; nothing here is specific to either.
 *
 * Positions are WORLD space. text_popup_draw() does the camera transform, so a
 * caller only needs to know where in the world the thing happened -- the same
 * coordinate it already has from the ECS.
 *
 * This system is COSMETIC ONLY and must stay that way (see
 * docs/EFFECTS_PLAN.md, docs/SYSTEM_DESIGN_RULES.md):
 *
 *   - It reads simulation state and never writes it.
 *   - It never draws from the global zw_rand() stream. waves.c, items.c and
 *     bot.c all draw from that stream, so a single cosmetic zw_rand() call here
 *     would shift every later sim draw and break single-player byte-identity.
 *     The spread angle for TEXT_POPUP_RANDOM comes from a private LCG owned by
 *     this module, reset by text_popup_init().
 *   - It is safe on a render-only net client: a popup is spawned from an event
 *     the host already decided, never from a value the client invented.
 */

#define MAX_TEXT_POPUPS 256

typedef enum {
    TEXT_POPUP_UP,          /* straight up -- points, pickups */
    TEXT_POPUP_UP_LEFT,
    TEXT_POPUP_UP_RIGHT,
    TEXT_POPUP_RANDOM,      /* upward fan, private-RNG angle -- damage numbers */
    TEXT_POPUP_RADIAL       /* full circle, deterministic per slot */
} TextPopupStyle;

typedef struct {
    float x, y;             /* world space */
    float vx, vy;           /* world units/sec */
    char text[64];
    float lifetime;
    float age;
    float scale;
    float alpha;
    SDL_Color color;
    TextPopupStyle style;
    bool active;
    /* Rendered lazily on first draw and cached for the popup's whole life, so
     * a popup costs one TTF rasterisation rather than one per frame. NULL in
     * headless, where draw never runs. */
    SDL_Texture *tex;
    float tex_w, tex_h;
} TextPopup;

void text_popup_init(void);
void text_popup_shutdown(void);

/* Ages, moves and retires popups. Pure with respect to the simulation. */
void text_popup_update(float dt);

/* Draws every live popup in world space. No-op without a renderer or font. */
void text_popup_draw(SDL_Renderer *renderer, Camera *cam, TTF_Font *font);

/* Spawn at a world position. When the buffer is full the popup closest to
 * expiring is recycled, so the newest feedback is never the one dropped. */
void text_popup_spawn(float x, float y, const char *text, SDL_Color color,
                      TextPopupStyle style);

/* Convenience wrappers used by the E2/E3 trigger sites. */
void text_popup_spawn_pickup(float x, float y, const char *text);
void text_popup_spawn_damage(float x, float y, int damage);
void text_popup_spawn_points(float x, float y, int points);

/* Live popup count. For tests and diagnostics. */
int text_popup_active_count(void);

/* Read-only view of a slot, or NULL if the index is out of range or the slot
 * is idle. For tests: what a popup SAYS and WHERE it sits is the whole
 * contract of the spawn wrappers, and it is otherwise only observable by
 * looking at the screen. */
const TextPopup *text_popup_at(int index);

#endif
