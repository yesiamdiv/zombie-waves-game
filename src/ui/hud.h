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

/* Authoritative display state for the HUD.
 *
 * This struct exists because the HUD used to read HP and the inventory straight
 * out of `local` - the client's OWN Player/ECS. On a render-only client those
 * are never simulated: they froze at whatever they were at join and then
 * disagreed with the host forever (B22/B23). Single-player and the host fill
 * this from their real ECS; a client fills it from the host snapshot.
 *
 * `valid == false` means "no authoritative source yet"; the HUD then falls back
 * to `local`, which is the correct thing for single-player. */
typedef struct {
    bool  valid;             /* false => fall back to the local ECS */
    /* --- this slot --- */
    bool  alive;
    bool  eliminated;
    float hp;
    float hp_max;
    float respawn_timer;
    int   points;
    int   grenades;
    int   launcher_ammo;
    int   weapon;            /* WeaponType */
    /* --- shared wave state (N4/B26) ---
     * waves_update() never runs on a render-only client, so the HUD used to
     * read a WaveSystem full of permanent zeros: "Wave: 0", "Kills: 0", and no
     * zombie count or countdown at all. */
    int   wave_number;
    bool  wave_active;
    int   zombies_alive;
    bool  between_waves;
    float wave_cooldown_remaining;
    int   total_kills;
} HudPlayerState;

/* Draws the HUD for ONE player slot (the local/displayed player): health and
 * inventory from `auth` when `auth->valid`, else from `local->entity` /
 * `local->inventory`; crosshair from `local->input`. No more
 * first-player-in-ECS guessing - in multiplayer the renderer targets its own
 * slot. When `multi` is set the player's colored name tag is shown
 * (beacon/HUD color coding). */
void hud_draw(SDL_Renderer *renderer, HUD *hud, World *ecs, WaveSystem *waves,
              const struct Player *local, const HudPlayerState *auth, bool multi,
              int screen_w, int screen_h, TTF_Font *font);

#endif
