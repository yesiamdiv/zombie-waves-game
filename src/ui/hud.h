#ifndef HUD_H
#define HUD_H

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include "ecs/ecs.h"
#include "world/waves.h"

typedef struct {
    float damage_flash;
    float message_timer;
    char message[128];
} HUD;

void hud_init(HUD *hud);
void hud_update(HUD *hud, float dt);
void hud_show_message(HUD *hud, const char *msg, float duration);
void hud_draw(SDL_Renderer *renderer, HUD *hud, World *ecs, WaveSystem *waves,
              int screen_w, int screen_h, TTF_Font *font);

#endif
