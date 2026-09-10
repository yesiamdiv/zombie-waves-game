#ifndef MENU_H
#define MENU_H

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include "core/input.h"

typedef enum {
    GAME_STATE_MENU,
    GAME_STATE_PLAYING,
    GAME_STATE_PAUSED,
    GAME_STATE_GAME_OVER
} GameState;

typedef struct {
    int selected_option;
    int option_count;
    float title_pulse;
    float menu_timer;
    bool quit_requested;
} MainMenu;

typedef struct {
    int selected_option;
    int option_count;
} PauseMenu;

typedef struct {
    float display_timer;
    int final_score;
    int final_wave;
    int final_kills;
} GameOverScreen;

void menu_init(MainMenu *menu);
void pause_menu_init(PauseMenu *menu);
void gameover_init(GameOverScreen *go, int score, int wave, int kills);

GameState menu_update(MainMenu *menu, InputState *input, float dt);
GameState pause_menu_update(PauseMenu *menu, InputState *input);
GameState gameover_update(GameOverScreen *go, InputState *input);

void menu_draw(SDL_Renderer *renderer, MainMenu *menu, int screen_w, int screen_h, TTF_Font *font);
void pause_menu_draw(SDL_Renderer *renderer, PauseMenu *menu, int screen_w, int screen_h, TTF_Font *font);
void gameover_draw(SDL_Renderer *renderer, GameOverScreen *go, int screen_w, int screen_h, TTF_Font *font);

#endif
