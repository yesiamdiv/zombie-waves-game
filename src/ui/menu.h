#ifndef MENU_H
#define MENU_H

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include "core/input.h"
#include "weapons/weapons.h"

typedef enum {
    GAME_STATE_MENU,
    GAME_STATE_PLAYING,
    GAME_STATE_PAUSED,
    GAME_STATE_SHOP,
    GAME_STATE_GAME_OVER
} GameState;

typedef struct {
    int selected_option;
    int option_count;
    int selected_map;   /* index into the map registry */
    float title_pulse;
    float menu_timer;
    bool quit_requested;
} MainMenu;

typedef struct {
    int selected_option;
    int option_count;
} PauseMenu;

/* Weapon upgrade shop. Rows:
 *  0 Pistol (info) | 1 Sword | 2 Grenades x5 | 3 Launcher | 4 Launcher Ammo x5 | 5 Close */
typedef struct {
    int selected_option;
    int option_count;
    float msg_timer;
    char message[160];
} ShopMenu;

typedef struct {
    float display_timer;
    int final_score;
    int final_wave;
    int final_kills;
} GameOverScreen;

void menu_init(MainMenu *menu);
void pause_menu_init(PauseMenu *menu);
void gameover_init(GameOverScreen *go, int score, int wave, int kills);
void shop_menu_init(ShopMenu *menu);

GameState menu_update(MainMenu *menu, InputState *input, float dt);
GameState pause_menu_update(PauseMenu *menu, InputState *input);
GameState gameover_update(GameOverScreen *go, InputState *input);
GameState shop_menu_update(ShopMenu *menu, InputState *input, PlayerInventory *inv);

void menu_draw(SDL_Renderer *renderer, MainMenu *menu, int screen_w, int screen_h, TTF_Font *font);
void pause_menu_draw(SDL_Renderer *renderer, PauseMenu *menu, int screen_w, int screen_h, TTF_Font *font);
void gameover_draw(SDL_Renderer *renderer, GameOverScreen *go, int screen_w, int screen_h, TTF_Font *font);
void shop_menu_draw(SDL_Renderer *renderer, ShopMenu *menu, const PlayerInventory *inv,
                    int screen_w, int screen_h, TTF_Font *font);

#endif
