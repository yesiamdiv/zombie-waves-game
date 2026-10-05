#include "ui/hud.h"
#include "core/log.h"
#include "players.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

void hud_init(HUD *hud) {
    hud->damage_flash = 0;
    hud->message_timer = 0;
    memset(hud->message, 0, sizeof(hud->message));
    hud->last_player_hp = -1.0f;
    LOG_DEBUG("HUD initialized");
}

void hud_update(HUD *hud, float dt) {
    if (hud->damage_flash > 0) {
        hud->damage_flash -= dt * 3.0f;
        if (hud->damage_flash < 0) hud->damage_flash = 0;
    }
    if (hud->message_timer > 0) {
        hud->message_timer -= dt;
    }
}

void hud_show_message(HUD *hud, const char *msg, float duration) {
    strncpy(hud->message, msg, sizeof(hud->message) - 1);
    hud->message_timer = duration;
}

/* Hurt feedback (B4): track the player's current HP each frame and arm the red
 * flash when HP drops. The old code only ever decayed/drew `damage_flash`
 * without setting it, so the overlay was dead. Healing (medkit) raises HP, so
 * the flash only fires on actual damage; the first call initializes the
 * baseline without flashing. */
void hud_track_player_hp(HUD *hud, float current_hp) {
    if (hud->last_player_hp >= 0.0f && current_hp < hud->last_player_hp) {
        hud->damage_flash = 1.0f;
    }
    hud->last_player_hp = current_hp;
}

static void draw_text(SDL_Renderer *renderer, TTF_Font *font,
                       const char *text, float x, float y,
                       SDL_FColor color) {
    if (!font || !text || text[0] == '\0') return;
    SDL_Color c = {
        (Uint8)(color.r * 255), (Uint8)(color.g * 255),
        (Uint8)(color.b * 255), (Uint8)(color.a * 255)
    };
    SDL_Surface *surface = TTF_RenderText_Blended(font, text, SDL_strlen(text), c);
    if (!surface) return;
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!texture) {
        SDL_DestroySurface(surface);
        return;
    }
    SDL_FRect dst = {x, y, (float)surface->w, (float)surface->h};
    SDL_RenderTexture(renderer, texture, NULL, &dst);
    SDL_DestroyTexture(texture);
    SDL_DestroySurface(surface);
}

static void draw_text_right(SDL_Renderer *renderer, TTF_Font *font,
                              const char *text, float right_x, float y,
                              SDL_FColor color) {
    if (!font || !text || text[0] == '\0') return;
    SDL_Color c = {
        (Uint8)(color.r * 255), (Uint8)(color.g * 255),
        (Uint8)(color.b * 255), (Uint8)(color.a * 255)
    };
    SDL_Surface *surface = TTF_RenderText_Blended(font, text, SDL_strlen(text), c);
    if (!surface) return;
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!texture) {
        SDL_DestroySurface(surface);
        return;
    }
    SDL_FRect dst = {right_x - (float)surface->w, y, (float)surface->w, (float)surface->h};
    SDL_RenderTexture(renderer, texture, NULL, &dst);
    SDL_DestroyTexture(texture);
    SDL_DestroySurface(surface);
}

static void draw_text_centered(SDL_Renderer *renderer, TTF_Font *font,
                                const char *text, float x, float y,
                                SDL_FColor color) {
    if (!font || !text || text[0] == '\0') return;
    SDL_Color c = {
        (Uint8)(color.r * 255), (Uint8)(color.g * 255),
        (Uint8)(color.b * 255), (Uint8)(color.a * 255)
    };
    SDL_Surface *surface = TTF_RenderText_Blended(font, text, SDL_strlen(text), c);
    if (!surface) return;
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!texture) {
        SDL_DestroySurface(surface);
        return;
    }
    SDL_FRect dst = {x - surface->w * 0.5f, y, (float)surface->w, (float)surface->h};
    SDL_RenderTexture(renderer, texture, NULL, &dst);
    SDL_DestroyTexture(texture);
    SDL_DestroySurface(surface);
}

void hud_draw(SDL_Renderer *renderer, HUD *hud, World *ecs, WaveSystem *waves,
              const struct Player *local, const HudPlayerState *auth, bool multi,
              int screen_w, int screen_h, TTF_Font *font) {
    char buf[128];

    if (!local) return;

    /* Authority: a client displays what the host says (B22/B23). Single-player
     * and the host have no snapshot to consult, so `auth->valid` is false and
     * the local ECS - the real thing - is used instead. */
    bool use_auth = auth && auth->valid;

    bool alive = use_auth ? auth->alive
                          : (local->entity != ECS_NULL_ENTITY &&
                             ecs_is_alive(ecs, local->entity));

    /* Dead local player: multiplayer shows the post-death overlay (respawn
     * countdown in TDM, elimination mark in HARDCORE). Single-player hands off
     * to the game-over screen instead. */
    if (!alive) {
        if (multi) {
            SDL_FColor dead_color = {1.0f, 0.3f, 0.3f, 1.0f};
            bool eliminated = use_auth ? auth->eliminated : local->eliminated;
            float respawn_timer = use_auth ? auth->respawn_timer
                                           : local->respawn_timer;
            if (eliminated) {
                draw_text_centered(renderer, font, "ELIMINATED",
                                   (float)screen_w * 0.5f, (float)screen_h * 0.3f,
                                   dead_color);
            } else if (respawn_timer > 0.0f) {
                snprintf(buf, sizeof(buf), "Respawning at beacon in %.1fs",
                         respawn_timer);
                draw_text_centered(renderer, font, buf,
                                   (float)screen_w * 0.5f, (float)screen_h * 0.3f,
                                   dead_color);
            }
        }
        return;
    }

    const InputState *input = &local->input;
    const PlayerInventory *inv = &local->inventory;

    /* Health is display state, not simulation state: read it from the host when
     * one is answering, otherwise from the local ECS. */
    float hp_current;
    float hp_max;
    if (use_auth) {
        hp_current = auth->hp;
        hp_max = auth->hp_max > 0.0f ? auth->hp_max : 1.0f;
    } else {
        Entity player = local->entity;
        CHealth *hp = ecs_get_health(ecs, player);
        hp_current = hp->current;
        hp_max = hp->max;
    }
    hud_track_player_hp(hud, hp_current);

    /* Health bar */
    float bar_x = 20.0f;
    float bar_y = 20.0f;
    float bar_w = 200.0f;
    float bar_h = 20.0f;

    SDL_SetRenderDrawColorFloat(renderer, 0.1f, 0.1f, 0.1f, 0.8f);
    SDL_RenderFillRect(renderer, &(SDL_FRect){bar_x, bar_y, bar_w, bar_h});

    float ratio = hp_current / hp_max;
    if (ratio < 0) ratio = 0;
    float r = (1.0f - ratio) * 0.9f + 0.1f;
    float g = ratio * 0.9f + 0.1f;
    SDL_SetRenderDrawColorFloat(renderer, r, g, 0.2f, 0.9f);
    SDL_RenderFillRect(renderer, &(SDL_FRect){bar_x, bar_y, bar_w * ratio, bar_h});

    /* Health text */
    SDL_FColor white = {1.0f, 1.0f, 1.0f, 0.9f};
    snprintf(buf, sizeof(buf), "HP: %.0f / %.0f", hp_current, hp_max);
    draw_text(renderer, font, buf, bar_x + 5.0f, bar_y + 2.0f, white);

    /* Wave info. Authoritative on a client (B26); the local WaveSystem
     * otherwise, which is the real thing for single-player and the host. */
    const int wave_number   = use_auth ? auth->wave_number   : waves->wave_number;
    const bool wave_active  = use_auth ? auth->wave_active  : (waves->wave_active != 0);
    const int zombies_alive = use_auth ? auth->zombies_alive : waves->zombies_alive;
    const bool between_waves= use_auth ? auth->between_waves: (waves->between_waves != 0);
    const float next_in     = use_auth ? auth->wave_cooldown_remaining
                                        : (waves->wave_cooldown - waves->wave_cooldown_timer);
    const int total_kills   = use_auth ? auth->total_kills   : waves->total_kills;

    SDL_FColor wave_color = {1.0f, 0.9f, 0.3f, 1.0f};
    snprintf(buf, sizeof(buf), "Wave: %d", wave_number);
    draw_text(renderer, font, buf, 20.0f, 50.0f, wave_color);

    /* Player-colored name tag (multiplayer): ties this HUD to the slot's
     * beacon color so spectators/players can tell whose view they are on. */
    if (multi && local && local->name[0] != '\0') {
        SDL_FColor name_color = local->color;
        name_color.a = 0.9f;
        snprintf(buf, sizeof(buf), "%s", local->name);
        draw_text(renderer, font, buf, 20.0f, 76.0f, name_color);
    }

    /* Zombies remaining */
    if (wave_active) {
        SDL_FColor zombie_color = {0.9f, 0.3f, 0.3f, 1.0f};
        snprintf(buf, sizeof(buf), "Zombies: %d", zombies_alive);
        draw_text(renderer, font, buf, 20.0f, 75.0f, zombie_color);
    } else if (between_waves && wave_number > 0) {
        SDL_FColor next_color = {0.3f, 0.9f, 0.3f, 1.0f};
        snprintf(buf, sizeof(buf), "Next wave in %.1fs", next_in);
        draw_text(renderer, font, buf, 20.0f, 75.0f, next_color);
    }

    /* Kills */
    SDL_FColor kill_color = {0.8f, 0.8f, 0.8f, 0.9f};
    snprintf(buf, sizeof(buf), "Kills: %d", total_kills);
    draw_text_right(renderer, font, buf, (float)screen_w - 20.0f, 20.0f, kill_color);

    /* Score */
    int score = total_kills * 100 + (wave_number - 1) * 500;
    snprintf(buf, sizeof(buf), "Score: %d", score);
    draw_text_right(renderer, font, buf, (float)screen_w - 20.0f, 45.0f, wave_color);

    /* Points (shop currency) */
    if (inv) {
        SDL_FColor pts_color = {0.6f, 1.0f, 0.4f, 0.95f};
        snprintf(buf, sizeof(buf), "Points: %d",
                 use_auth ? auth->points : inv->points);
        draw_text_right(renderer, font, buf, (float)screen_w - 20.0f, 70.0f, pts_color);

        /* Current weapon + consumable stocks */
        char wbuf[96];
        snprintf(wbuf, sizeof(wbuf), "Weapon: %s%s",
                 weapons_name((WeaponType)(use_auth ? auth->weapon
                                                     : (int)inv->current)),
                 (use_auth ? auth->weapon : (int)inv->current) != WEAPON_PISTOL
                     ? " [1-4 to switch]" : " [1-4 weapons]");
        SDL_FColor wcol = {0.9f, 0.9f, 0.95f, 0.95f};
        draw_text_right(renderer, font, wbuf, (float)screen_w - 20.0f, 95.0f, wcol);

        snprintf(buf, sizeof(buf), "Grenades: %d | Rockets: %d",
                 use_auth ? auth->grenades : inv->grenades,
                 use_auth ? auth->launcher_ammo : inv->launcher_ammo);
        draw_text_right(renderer, font, buf, (float)screen_w - 20.0f, 120.0f, wcol);
    }

    /* Crosshair at the actual mouse position (tracks the cursor exactly). */
    float cx = input ? input->mouse_x : (float)screen_w * 0.5f;
    float cy = input ? input->mouse_y : (float)screen_h * 0.5f;
    SDL_SetRenderDrawColorFloat(renderer, 1.0f, 1.0f, 1.0f, 0.6f);
    SDL_RenderLine(renderer, cx - 10, cy, cx + 10, cy);
    SDL_RenderLine(renderer, cx, cy - 10, cx, cy + 10);

    /* Center dot */
    SDL_SetRenderDrawColorFloat(renderer, 1.0f, 0.3f, 0.3f, 0.8f);
    SDL_RenderFillRect(renderer, &(SDL_FRect){cx - 1.5f, cy - 1.5f, 3.0f, 3.0f});

    /* Damage flash overlay */
    if (hud->damage_flash > 0) {
        SDL_SetRenderDrawColorFloat(renderer, 1.0f, 0.0f, 0.0f, hud->damage_flash * 0.3f);
        SDL_RenderFillRect(renderer, &(SDL_FRect){0, 0, (float)screen_w, (float)screen_h});
    }

    /* Center message */
    if (hud->message_timer > 0) {
        float alpha = hud->message_timer > 0.5f ? 1.0f : hud->message_timer * 2.0f;
        SDL_FColor msg_color = {1.0f, 1.0f, 0.3f, alpha};
        draw_text_centered(renderer, font, hud->message,
                           (float)screen_w * 0.5f, (float)screen_h * 0.3f, msg_color);
    }

    /* Pause / shop hints */
    SDL_FColor pause_hint = {0.3f, 0.3f, 0.35f, 0.5f};
    draw_text(renderer, font, "ESC: Pause | B: Shop | 1-4: Weapon",
              20.0f, (float)screen_h - 30.0f, pause_hint);
}
