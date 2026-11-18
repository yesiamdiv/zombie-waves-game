#ifndef MENU_BACKDROP_H
#define MENU_BACKDROP_H

#include <SDL3/SDL.h>
#include "world/theme.h"

/* H5: render-only themed menu backdrop. Both helpers are pure drawing - they
 * read a Theme's palette and a caller-supplied menu timer, never the ECS or a
 * network state, so they cannot affect the simulation or the wire. No new
 * texture ships: the motif is drawn as bands/lines, so the texture count and
 * NET_ASSET_VERSION do not move.
 *
 * `red_wash` (0..1) keeps the game-over screen's danger tint while still
 * picking up the map theme. `t` is the menu's own animation clock in seconds;
 * it is passed in rather than read from SDL so the motion is deterministic. */
void menu_backdrop_draw(SDL_Renderer *renderer, ThemeID theme, float t,
                        int screen_w, int screen_h, float red_wash);

/* Same palette, semi-transparent: replaces the flat black dim over the frozen
 * world on the pause and shop screens. `alpha` is the base opacity (0..1). */
void menu_backdrop_overlay(SDL_Renderer *renderer, ThemeID theme,
                           int screen_w, int screen_h, float alpha);

#endif
