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
    float last_player_hp;   /* previous frame's player HP, for hurt-flash */
} HUD;

void hud_init(HUD *hud);
void hud_update(HUD *hud, float dt);
void hud_show_message(HUD *hud, const char *msg, float duration);
/* Records the displayed player's HP each frame and arms the red damage flash
 * on any drop. The first call only establishes a baseline (no flash), and
 * healing raises HP so it never false-triggers. Call once per frame from
 * hud_draw, which is why it is declared here rather than kept private. */
void hud_track_player_hp(HUD *hud, float current_hp);

/* Draws the HUD for ONE player slot (the local/displayed player): health from
 * `local->entity`, crosshair from `local->input`, shop info from
 * `local->inventory`. No more first-player-in-ECS guessing - in multiplayer the
 * renderer targets its own slot. When `multi` is set the player's colored
 * name tag is shown (beacon/HUD color coding). */
void hud_draw(SDL_Renderer *renderer, HUD *hud, World *ecs, WaveSystem *waves,
              const struct Player *local, bool multi,
              int screen_w, int screen_h, TTF_Font *font);

#endif
