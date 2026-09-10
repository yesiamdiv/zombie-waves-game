#include "ui/menu.h"
#include "core/log.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

void menu_init(MainMenu *menu) {
    menu->selected_option = 0;
    menu->option_count = 2;
    menu->title_pulse = 0;
    menu->menu_timer = 0;
    menu->quit_requested = false;
    LOG_DEBUG("Main menu initialized");
}

void pause_menu_init(PauseMenu *menu) {
    menu->selected_option = 0;
    menu->option_count = 2;
}

void gameover_init(GameOverScreen *go, int score, int wave, int kills) {
    go->display_timer = 0;
    go->final_score = score;
    go->final_wave = wave;
    go->final_kills = kills;
}

GameState menu_update(MainMenu *menu, InputState *input, float dt) {
    menu->title_pulse += dt * 2.0f;
    menu->menu_timer += dt;

    if (input_key_pressed(input, SDL_SCANCODE_UP) || input_key_pressed(input, SDL_SCANCODE_W)) {
        menu->selected_option--;
        if (menu->selected_option < 0) menu->selected_option = menu->option_count - 1;
    }
    if (input_key_pressed(input, SDL_SCANCODE_DOWN) || input_key_pressed(input, SDL_SCANCODE_S)) {
        menu->selected_option++;
        if (menu->selected_option >= menu->option_count) menu->selected_option = 0;
    }

    if (input_key_pressed(input, SDL_SCANCODE_RETURN) || input_key_pressed(input, SDL_SCANCODE_SPACE)) {
        if (menu->selected_option == 0) return GAME_STATE_PLAYING;
        if (menu->selected_option == 1) {
            menu->quit_requested = true;
            return GAME_STATE_MENU;
        }
    }

    return GAME_STATE_MENU;
}

GameState pause_menu_update(PauseMenu *menu, InputState *input) {
    if (input_key_pressed(input, SDL_SCANCODE_UP) || input_key_pressed(input, SDL_SCANCODE_W)) {
        menu->selected_option--;
        if (menu->selected_option < 0) menu->selected_option = menu->option_count - 1;
    }
    if (input_key_pressed(input, SDL_SCANCODE_DOWN) || input_key_pressed(input, SDL_SCANCODE_S)) {
        menu->selected_option++;
        if (menu->selected_option >= menu->option_count) menu->selected_option = 0;
    }

    if (input_key_pressed(input, SDL_SCANCODE_RETURN) || input_key_pressed(input, SDL_SCANCODE_SPACE)) {
        if (menu->selected_option == 0) return GAME_STATE_PLAYING;
        if (menu->selected_option == 1) return GAME_STATE_MENU;
    }

    if (input_key_pressed(input, SDL_SCANCODE_ESCAPE)) {
        return GAME_STATE_PLAYING;
    }

    return GAME_STATE_PAUSED;
}

GameState gameover_update(GameOverScreen *go, InputState *input) {
    go->display_timer += 1.0f / 60.0f;

    if (go->display_timer > 1.0f) {
        if (input_key_pressed(input, SDL_SCANCODE_RETURN) ||
            input_key_pressed(input, SDL_SCANCODE_SPACE)) {
            return GAME_STATE_MENU;
        }
    }

    return GAME_STATE_GAME_OVER;
}

static void draw_text_centered(SDL_Renderer *renderer, TTF_Font *font,
                                const char *text, float x, float y,
                                SDL_FColor color) {
    if (!font || !text) return;
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

void menu_draw(SDL_Renderer *renderer, MainMenu *menu, int screen_w, int screen_h, TTF_Font *font) {
    /* Dark background */
    SDL_SetRenderDrawColorFloat(renderer, 0.05f, 0.05f, 0.08f, 1.0f);
    SDL_RenderClear(renderer);

    /* Title */
    float pulse = 0.8f + sinf(menu->title_pulse) * 0.2f;
    SDL_FColor title_color = {pulse, 0.15f * pulse, 0.15f * pulse, 1.0f};
    draw_text_centered(renderer, font, "OPEN WORLD ZOMBIE WAVES",
                       screen_w * 0.5f, screen_h * 0.2f, title_color);

    /* Subtitle */
    SDL_FColor sub_color = {0.5f, 0.55f, 0.6f, 0.8f};
    draw_text_centered(renderer, font, "Survive the Horde",
                       screen_w * 0.5f, screen_h * 0.3f, sub_color);

    /* Options */
    const char *options[] = {"Start Game", "Quit"};
    for (int i = 0; i < menu->option_count; i++) {
        SDL_FColor opt_color;
        if (i == menu->selected_option) {
            float sel = 0.8f + sinf(menu->title_pulse * 3.0f) * 0.2f;
            opt_color = (SDL_FColor){sel, sel, 0.2f, 1.0f};
        } else {
            opt_color = (SDL_FColor){0.5f, 0.5f, 0.5f, 0.7f};
        }
        char prefix[4] = "";
        if (i == menu->selected_option) snprintf(prefix, sizeof(prefix), "> ");
        char buf[64];
        snprintf(buf, sizeof(buf), "%s%s", prefix, options[i]);
        draw_text_centered(renderer, font, buf,
                           screen_w * 0.5f, screen_h * 0.5f + i * 50.0f, opt_color);
    }

    /* Controls hint */
    SDL_FColor hint = {0.3f, 0.3f, 0.35f, 0.6f};
    draw_text_centered(renderer, font, "WASD/Arrows: Move  |  Mouse: Aim & Shoot  |  ESC: Pause",
                       screen_w * 0.5f, screen_h * 0.85f, hint);
}

void pause_menu_draw(SDL_Renderer *renderer, PauseMenu *menu, int screen_w, int screen_h, TTF_Font *font) {
    /* Dim overlay */
    SDL_SetRenderDrawColorFloat(renderer, 0.0f, 0.0f, 0.0f, 0.6f);
    SDL_RenderFillRect(renderer, &(SDL_FRect){0, 0, (float)screen_w, (float)screen_h});

    SDL_FColor title_color = {1.0f, 1.0f, 1.0f, 1.0f};
    draw_text_centered(renderer, font, "PAUSED",
                       screen_w * 0.5f, screen_h * 0.3f, title_color);

    const char *options[] = {"Resume", "Quit to Menu"};
    for (int i = 0; i < menu->option_count; i++) {
        SDL_FColor opt_color;
        if (i == menu->selected_option) {
            opt_color = (SDL_FColor){1.0f, 1.0f, 0.3f, 1.0f};
        } else {
            opt_color = (SDL_FColor){0.5f, 0.5f, 0.5f, 0.7f};
        }
        char prefix[4] = "";
        if (i == menu->selected_option) snprintf(prefix, sizeof(prefix), "> ");
        char buf[64];
        snprintf(buf, sizeof(buf), "%s%s", prefix, options[i]);
        draw_text_centered(renderer, font, buf,
                           screen_w * 0.5f, screen_h * 0.5f + i * 50.0f, opt_color);
    }
}

void gameover_draw(SDL_Renderer *renderer, GameOverScreen *go, int screen_w, int screen_h, TTF_Font *font) {
    SDL_SetRenderDrawColorFloat(renderer, 0.1f, 0.02f, 0.02f, 1.0f);
    SDL_RenderClear(renderer);

    SDL_FColor title = {0.9f, 0.15f, 0.15f, 1.0f};
    draw_text_centered(renderer, font, "GAME OVER",
                       screen_w * 0.5f, screen_h * 0.15f, title);

    char buf[128];
    SDL_FColor info = {0.8f, 0.8f, 0.8f, 1.0f};

    snprintf(buf, sizeof(buf), "Wave Reached: %d", go->final_wave);
    draw_text_centered(renderer, font, buf, screen_w * 0.5f, screen_h * 0.35f, info);

    snprintf(buf, sizeof(buf), "Zombies Killed: %d", go->final_kills);
    draw_text_centered(renderer, font, buf, screen_w * 0.5f, screen_h * 0.42f, info);

    snprintf(buf, sizeof(buf), "Score: %d", go->final_score);
    draw_text_centered(renderer, font, buf, screen_w * 0.5f, screen_h * 0.49f, info);

    if (go->display_timer > 1.0f) {
        float blink = 0.5f + sinf(go->display_timer * 3.0f) * 0.5f;
        SDL_FColor prompt = {blink, blink, blink, 0.8f};
        draw_text_centered(renderer, font, "Press ENTER to return to menu",
                           screen_w * 0.5f, screen_h * 0.7f, prompt);
    }
}
