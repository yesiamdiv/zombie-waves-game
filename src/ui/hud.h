#ifndef HUD_H
#define HUD_H

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include "ecs/ecs.h"
#include "core/input.h"
#include "world/waves.h"
#include "weapons/weapons.h"

typedef struct {
    float damage_flash;
    float message_timer;
    char message[128];
    float last_player_hp;   /* previous frame's player HP, for hurt-flash */
} HUD;

void hud_init(HUD *hud);
void hud_update(HUD *hud, float dt);
void hud_show_message(HUD *hud, const char *msg, float duration);
void hud_track_player_hp(HUD *hud, float current_hp);
void hud_draw(SDL_Renderer *renderer, HUD *hud, World *ecs, WaveSystem *waves,
              InputState *input, const PlayerInventory *inv,
              int screen_w, int screen_h, TTF_Font *font);

#endif
