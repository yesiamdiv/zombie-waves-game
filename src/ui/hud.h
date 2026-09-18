#ifndef HUD_H
#define HUD_H

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include "ecs/ecs.h"
#include "core/input.h"
#include "world/waves.h"
#include "weapons/weapons.h"

struct Player; /* forward decl; hud.c includes players.h */

typedef struct {
    float damage_flash;
    float message_timer;
    char message[128];
} HUD;

void hud_init(HUD *hud);
void hud_update(HUD *hud, float dt);
void hud_show_message(HUD *hud, const char *msg, float duration);
/* Draws the HUD for ONE player slot (the local/displayed player): health from
 * `local->entity`, crosshair from `local->input`, shop info from
 * `local->inventory`. No more first-player-in-ECS guessing - in multiplayer the
 * renderer targets its own slot. */
void hud_draw(SDL_Renderer *renderer, HUD *hud, World *ecs, WaveSystem *waves,
              const struct Player *local,
              int screen_w, int screen_h, TTF_Font *font);

#endif
