#ifndef MENU_H
#define MENU_H

#include <SDL3/SDL.h>
#include <stdarg.h>
#include <SDL3_ttf/SDL_ttf.h>
#include "core/input.h"
#include "weapons/weapons.h"
#include "world/world.h"

typedef enum {
    GAME_STATE_MENU,
    GAME_STATE_MAP_SELECT,   /* map picker with mini-map preview (assets F8) */
    GAME_STATE_CONNECTING,   /* client: transport/join handshake in flight */
    GAME_STATE_LOBBY,        /* pre-match: roster shown, waiting for host start */
    GAME_STATE_PLAYING,
    GAME_STATE_PAUSED,
    GAME_STATE_SHOP,
    GAME_STATE_GAME_OVER
} GameState;

typedef struct {
    int selected_option;
    int option_count;
    float title_pulse;
    float menu_timer;
    bool quit_requested;
    /* Co-op entries (P1e). "Join Co-op" opens a minimal address field; the
     * confirmed target is handed to main.c to start the net session. */
    bool editing_address;
    char join_address[64];
    bool host_requested;
    bool join_requested;
    /* Set when the player picks "Host Co-op", cleared for "Solo". The map
     * picker reads it to decide whether to resume into GAME_STATE_LOBBY or
     * straight into play, so the world is always chosen before the lobby
     * exists. */
    bool host_pending;
    /* Human-readable notice shown under the title. This exists because a
     * refused connection used to be LOG_ERROR-only: the player was dropped back
     * to the menu with nothing on screen and no way to tell a version mismatch
     * from a crash (sprint N2). */
    char message[160];
} MainMenu;

typedef struct {
    int selected_option;
    int option_count;
    float title_pulse;
    GameWorld thumb;   /* parsed preview of the highlighted map */
} MapSelectMenu;

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
/* Set/clear the main-menu notice. Safe to call in any state. */
void menu_set_message(MainMenu *menu, const char *fmt, ...);
void menu_clear_message(MainMenu *menu);
void map_select_init(MapSelectMenu *menu);
void map_select_free(MapSelectMenu *menu);
void pause_menu_init(PauseMenu *menu);
void gameover_init(GameOverScreen *go, int score, int wave, int kills);
void shop_menu_init(ShopMenu *menu);
/* Same as the internal setter the shop's own options use; exposed so the host's
 * verdict on a networked purchase can be written into the menu. Takes a
 * va_list because that verdict is usually assembled by the caller. */
void shop_menu_vset_message(ShopMenu *menu, const char *fmt, ...);

GameState menu_update(MainMenu *menu, InputState *input, float dt);
GameState map_select_update(MapSelectMenu *menu, InputState *input);
GameState pause_menu_update(PauseMenu *menu, InputState *input);
GameState gameover_update(GameOverScreen *go, InputState *input);
/* When `request_out` is NULL the caller is authoritative and the shop applies
 * purchases straight to `inv` (single player and the host). When it is
 * non-NULL the caller is a co-op client: nothing local is mutated, the pressed
 * option is written out as a NET_SHOP_* id for the host to resolve, and a
 * "requesting..." message is shown. `*request_out` must be initialised to
 * NET_SHOP_NONE by the caller; it is only written when a request is raised. */
GameState shop_menu_update(ShopMenu *menu, InputState *input, PlayerInventory *inv,
                           uint8_t *request_out);

void menu_draw(SDL_Renderer *renderer, MainMenu *menu, int screen_w, int screen_h, TTF_Font *font);
void map_select_draw(SDL_Renderer *renderer, MapSelectMenu *menu, int screen_w, int screen_h, TTF_Font *font);
void pause_menu_draw(SDL_Renderer *renderer, PauseMenu *menu, int screen_w, int screen_h, TTF_Font *font);
void gameover_draw(SDL_Renderer *renderer, GameOverScreen *go, int screen_w, int screen_h, TTF_Font *font);
void shop_menu_draw(SDL_Renderer *renderer, ShopMenu *menu, const PlayerInventory *inv,
                    int screen_w, int screen_h, TTF_Font *font);

/* Centered text helper shared with main.c lobby overlays. */
void menu_draw_text_centered(SDL_Renderer *renderer, TTF_Font *font,
                             const char *text, float x, float y,
                             SDL_FColor color);

#endif
