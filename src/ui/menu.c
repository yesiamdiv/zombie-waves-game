#include "ui/menu.h"
#include "core/log.h"
#include "world/map_registry.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdarg.h>

static void shop_set_message(ShopMenu *menu, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(menu->message, sizeof(menu->message), fmt, args);
    va_end(args);
    menu->msg_timer = 2.0f;
}

void menu_init(MainMenu *menu) {
    menu->selected_option = 0;
    menu->option_count = 2;
    menu->title_pulse = 0;
    menu->menu_timer = 0;
    menu->quit_requested = false;
    LOG_DEBUG("Main menu initialized");
}

static void map_select_load_thumb(MapSelectMenu *menu) {
    world_free(&menu->thumb);
    const MapDef *def = map_registry_get(menu->selected_option);
    if (def && world_load_map(&menu->thumb, def)) {
        LOG_DEBUG("Map preview loaded: %s", def->name);
    }
}

void map_select_init(MapSelectMenu *menu) {
    menu->selected_option = 0;
    menu->option_count = map_registry_count();
    menu->title_pulse = 0;
    menu->thumb = (GameWorld){0};
    map_select_load_thumb(menu);
    LOG_DEBUG("Map select initialized with %d maps", menu->option_count);
}

void map_select_free(MapSelectMenu *menu) {
    world_free(&menu->thumb);
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
        if (menu->selected_option == 0) return GAME_STATE_MAP_SELECT;
        if (menu->selected_option == 1) {
            menu->quit_requested = true;
            return GAME_STATE_MENU;
        }
    }

    return GAME_STATE_MENU;
}

GameState map_select_update(MapSelectMenu *menu, InputState *input) {
    menu->title_pulse += 1.0f / 60.0f * 2.0f;

    int prev = menu->selected_option;
    if (input_key_pressed(input, SDL_SCANCODE_UP) || input_key_pressed(input, SDL_SCANCODE_W)) {
        menu->selected_option--;
        if (menu->selected_option < 0) menu->selected_option = menu->option_count - 1;
    }
    if (input_key_pressed(input, SDL_SCANCODE_DOWN) || input_key_pressed(input, SDL_SCANCODE_S)) {
        menu->selected_option++;
        if (menu->selected_option >= menu->option_count) menu->selected_option = 0;
    }
    if (menu->selected_option != prev) {
        map_select_load_thumb(menu);
    }

    if (input_key_pressed(input, SDL_SCANCODE_ESCAPE) ||
        input_key_pressed(input, SDL_SCANCODE_B)) {
        return GAME_STATE_MENU;
    }

    if (input_key_pressed(input, SDL_SCANCODE_RETURN) ||
        input_key_pressed(input, SDL_SCANCODE_SPACE)) {
        return GAME_STATE_PLAYING;
    }

    return GAME_STATE_MAP_SELECT;
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

    const float lh = (float)TTF_GetFontHeight(font);

    /* Title */
    float pulse = 0.8f + sinf(menu->title_pulse) * 0.2f;
    SDL_FColor title_color = {pulse, 0.15f * pulse, 0.15f * pulse, 1.0f};
    draw_text_centered(renderer, font, "OPEN WORLD ZOMBIE WAVES",
                       screen_w * 0.5f, screen_h * 0.18f, title_color);

    /* Subtitle */
    SDL_FColor sub_color = {0.5f, 0.55f, 0.6f, 0.8f};
    draw_text_centered(renderer, font, "Survive the Horde",
                       screen_w * 0.5f, screen_h * 0.18f + lh + 16.0f, sub_color);

    /* Options: vertically centered as a block, spaced by font height so they
     * never overlap regardless of window size. */
    const char *options[] = {"Start Game", "Quit"};
    float row_h = lh + 18.0f;
    float block_h = (float)menu->option_count * row_h;
    float first_y = screen_h * 0.5f - block_h * 0.5f;
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
        char buf[96];
        snprintf(buf, sizeof(buf), "%s%s", prefix, options[i]);
        draw_text_centered(renderer, font, buf,
                           screen_w * 0.5f, first_y + i * row_h, opt_color);
    }

    /* Controls hints: stacked bottom-right block with breathing room. */
    SDL_FColor hint = {0.3f, 0.3f, 0.35f, 0.6f};
    float hint_y = screen_h - lh * 3.0f - 28.0f;
    draw_text_centered(renderer, font, "WASD/Arrows: Move  |  Mouse: Aim & Shoot  |  ESC: Pause",
                       screen_w * 0.5f, hint_y, hint);

    draw_text_centered(renderer, font, "Weapon purchases reset each run",
                       screen_w * 0.5f, hint_y + lh + 10.0f, hint);
}

static SDL_FColor tile_preview_color(TileType t, ThemeID theme) {
    const Theme *th = theme_get(theme);
    switch (t) {
        case TILE_WALL:  return th->wall_color;
        case TILE_WATER: return th->water_color;
        case TILE_ROAD:  return th->road_color;
        case TILE_SPAWN: return (SDL_FColor){0.3f, 0.9f, 0.3f, 1.0f};
        case TILE_GROUND:
        default:         return th->ground_color;
    }
}

void map_select_draw(SDL_Renderer *renderer, MapSelectMenu *menu, int screen_w, int screen_h, TTF_Font *font) {
    /* Dark background */
    SDL_SetRenderDrawColorFloat(renderer, 0.05f, 0.05f, 0.08f, 1.0f);
    SDL_RenderClear(renderer);

    const float lh = (float)TTF_GetFontHeight(font);

    float pulse = 0.8f + sinf(menu->title_pulse) * 0.2f;
    SDL_FColor title_color = {pulse, pulse, 0.9f, 1.0f};
    draw_text_centered(renderer, font, "SELECT MAP",
                       screen_w * 0.5f, screen_h * 0.08f, title_color);

    /* Map list (left): name row + detail row, spaced by font height. */
    float list_x = screen_w * 0.30f;
    float row_y = screen_h * 0.20f;
    const float row_h = lh * 2.2f;
    for (int i = 0; i < menu->option_count; i++) {
        const MapDef *def = map_registry_get(i);
        if (!def) continue;
        SDL_FColor opt_color = (i == menu->selected_option)
            ? (SDL_FColor){1.0f, 1.0f, 0.3f, 1.0f}
            : (SDL_FColor){0.55f, 0.55f, 0.6f, 0.8f};
        char buf[96];
        const char *arrow = (i == menu->selected_option) ? "> " : "  ";
        snprintf(buf, sizeof(buf), "%s%s", arrow, def->name);
        draw_text_centered(renderer, font, buf,
                           list_x, row_y + i * row_h, opt_color);

        if (i == menu->selected_option) {
            char detail[128];
            const Theme *defth = theme_get(def->theme);
            snprintf(detail, sizeof(detail), "Theme: %s  |  Size: %dx%d tiles",
                     defth->name, menu->thumb.width, menu->thumb.height);
            draw_text_centered(renderer, font, detail,
                               list_x, row_y + lh + 6.0f + i * row_h,
                               (SDL_FColor){0.4f, 0.4f, 0.5f, 0.7f});
        }
    }

    SDL_FColor hint = {0.3f, 0.3f, 0.35f, 0.6f};
    draw_text_centered(renderer, font,
                       "W/S or Arrows: Select map  |  ENTER: Play  |  ESC: Back",
                       screen_w * 0.5f, screen_h - lh - 16.0f, hint);

    /* Mini-map preview of the highlighted map (right panel). */
    GameWorld *w = &menu->thumb;
    if (w->width > 0 && w->height > 0 && w->tiles) {
        float panel_w = screen_w * 0.28f;
        float panel_h = screen_h * 0.52f;
        float panel_x = screen_w * 0.64f;
        float panel_y = screen_h * 0.22f;

        SDL_SetRenderDrawColorFloat(renderer, 0.12f, 0.12f, 0.16f, 1.0f);
        SDL_RenderFillRect(renderer, &(SDL_FRect){panel_x - 10, panel_y - 10,
                                                  panel_w + 20, panel_h + 20});

        float scale = fminf(panel_w / (float)w->width,
                            panel_h / (float)w->height);
        float px = panel_x + (panel_w - w->width * scale) * 0.5f;
        float py = panel_y + (panel_h - w->height * scale) * 0.5f;

        for (int ty = 0; ty < w->height; ty++) {
            for (int tx = 0; tx < w->width; tx++) {
                SDL_FColor c = tile_preview_color(world_get_tile(w, tx, ty),
                                                  w->theme);
                SDL_SetRenderDrawColorFloat(renderer, c.r, c.g, c.b, 1.0f);
                SDL_FRect cell = {px + tx * scale, py + ty * scale,
                                  scale * 1.02f, scale * 1.02f};
                SDL_RenderFillRect(renderer, &cell);
            }
        }
    }
}

void pause_menu_draw(SDL_Renderer *renderer, PauseMenu *menu, int screen_w, int screen_h, TTF_Font *font) {
    /* Dim overlay */
    SDL_SetRenderDrawColorFloat(renderer, 0.0f, 0.0f, 0.0f, 0.6f);
    SDL_RenderFillRect(renderer, &(SDL_FRect){0, 0, (float)screen_w, (float)screen_h});

    const float lh = (float)TTF_GetFontHeight(font);

    SDL_FColor title_color = {1.0f, 1.0f, 1.0f, 1.0f};
    draw_text_centered(renderer, font, "PAUSED",
                       screen_w * 0.5f, screen_h * 0.30f, title_color);

    const char *options[] = {"Resume", "Quit to Menu"};
    float row_h = lh + 18.0f;
    float block_h = (float)menu->option_count * row_h;
    float first_y = screen_h * 0.5f - block_h * 0.5f;
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
                           screen_w * 0.5f, first_y + i * row_h, opt_color);
    }
}

void gameover_draw(SDL_Renderer *renderer, GameOverScreen *go, int screen_w, int screen_h, TTF_Font *font) {
    SDL_SetRenderDrawColorFloat(renderer, 0.1f, 0.02f, 0.02f, 1.0f);
    SDL_RenderClear(renderer);

    const float lh = (float)TTF_GetFontHeight(font);
    const float row_h = lh + 14.0f;

    SDL_FColor title = {0.9f, 0.15f, 0.15f, 1.0f};
    draw_text_centered(renderer, font, "GAME OVER",
                       screen_w * 0.5f, screen_h * 0.18f, title);

    char buf[128];
    SDL_FColor info = {0.8f, 0.8f, 0.8f, 1.0f};
    float stat_y = screen_h * 0.38f;

    snprintf(buf, sizeof(buf), "Wave Reached: %d", go->final_wave);
    draw_text_centered(renderer, font, buf, screen_w * 0.5f, stat_y, info);

    snprintf(buf, sizeof(buf), "Zombies Killed: %d", go->final_kills);
    draw_text_centered(renderer, font, buf, screen_w * 0.5f, stat_y + row_h, info);

    snprintf(buf, sizeof(buf), "Score: %d", go->final_score);
    draw_text_centered(renderer, font, buf, screen_w * 0.5f, stat_y + row_h * 2.0f, info);

    SDL_FColor note = {0.7f, 0.65f, 0.45f, 1.0f};
    draw_text_centered(renderer, font, "New run resets weapon purchases and points",
                       screen_w * 0.5f, stat_y + row_h * 3.5f, note);

    if (go->display_timer > 1.0f) {
        float blink = 0.5f + sinf(go->display_timer * 3.0f) * 0.5f;
        SDL_FColor prompt = {blink, blink, blink, 0.8f};
        draw_text_centered(renderer, font, "Press ENTER to return to menu",
                           screen_w * 0.5f, screen_h - lh - 24.0f, prompt);
    }
}

/* ---------------------------------------------------------------- Shop --- */

/* Shop rows: 0 Pistol | 1 Sword | 2 Grenades x5 | 3 Launcher | 4 Launcher
 * Ammo x5 | 5 Close. */
#define SHOP_OPTION_COUNT 6

void shop_menu_init(ShopMenu *menu) {
    menu->selected_option = 0;
    menu->option_count = SHOP_OPTION_COUNT;
    menu->msg_timer = 0;
    menu->message[0] = '\0';
}

GameState shop_menu_update(ShopMenu *menu, InputState *input, PlayerInventory *inv) {
    if (menu->msg_timer > 0) menu->msg_timer -= 1.0f / 60.0f;

    if (input_key_pressed(input, SDL_SCANCODE_UP) || input_key_pressed(input, SDL_SCANCODE_W)) {
        menu->selected_option--;
        if (menu->selected_option < 0) menu->selected_option = menu->option_count - 1;
    }
    if (input_key_pressed(input, SDL_SCANCODE_DOWN) || input_key_pressed(input, SDL_SCANCODE_S)) {
        menu->selected_option++;
        if (menu->selected_option >= menu->option_count) menu->selected_option = 0;
    }

    /* Close on B or ESC. */
    if (input_key_pressed(input, SDL_SCANCODE_B) ||
        input_key_pressed(input, SDL_SCANCODE_ESCAPE)) {
        return GAME_STATE_PLAYING;
    }

    if (input_key_pressed(input, SDL_SCANCODE_RETURN) ||
        input_key_pressed(input, SDL_SCANCODE_SPACE)) {
        switch (menu->selected_option) {
            case 0: /* Pistol */
                weapons_select(inv, WEAPON_PISTOL);
                shop_set_message(menu, "Firearm selected: Pistol");
                break;

            case 1: /* Sword */
                if (inv->unlocked[WEAPON_SWORD]) {
                    weapons_select(inv, WEAPON_SWORD);
                    shop_set_message(menu, "Sword selected (hold click to spin)");
                } else if (weapons_buy_sword(inv)) {
                    weapons_select(inv, WEAPON_SWORD);
                    shop_set_message(menu, "Sword purchased (%d pts left)", inv->points);
                } else {
                    shop_set_message(menu, "Not enough points (need %d)", SWORD_COST);
                }
                break;

            case 2: /* Grenades x5 */
                if (weapons_buy_grenade_pack(inv)) {
                    weapons_select(inv, WEAPON_GRENADE);
                    shop_set_message(menu, "Grenades +%d (owned %d)", GRENADES_PER_PACK, inv->grenades);
                } else {
                    shop_set_message(menu, "Not enough points (need %d)", GRENADE_PACK_COST);
                }
                break;

            case 3: /* Launcher */
                if (inv->unlocked[WEAPON_LAUNCHER]) {
                    weapons_select(inv, WEAPON_LAUNCHER);
                    shop_set_message(menu, "Launcher selected (rockets: %d)", inv->launcher_ammo);
                } else if (weapons_buy_launcher(inv)) {
                    weapons_select(inv, WEAPON_LAUNCHER);
                    shop_set_message(menu, "Launcher purchased! +%d starter rockets",
                                     LAUNCHER_STARTER_ROCKETS);
                } else {
                    shop_set_message(menu, "Not enough points (need %d)", LAUNCHER_COST);
                }
                break;

            case 4: /* Launcher ammo x5 */
                if (!inv->unlocked[WEAPON_LAUNCHER]) {
                    shop_set_message(menu, "Buy the launcher first");
                } else if (weapons_buy_launcher_ammo(inv)) {
                    shop_set_message(menu, "Rockets +%d (owned %d)", ROCKETS_PER_PACK, inv->launcher_ammo);
                } else {
                    shop_set_message(menu, "Not enough points (need %d)", LAUNCHER_AMMO_COST);
                }
                break;

            case 5: /* Close */
            default:
                return GAME_STATE_PLAYING;
        }
    }

    return GAME_STATE_SHOP;
}

void shop_menu_draw(SDL_Renderer *renderer, ShopMenu *menu, const PlayerInventory *inv,
                    int screen_w, int screen_h, TTF_Font *font) {
    /* Dim overlay so the frozen world reads as "menu mode". */
    SDL_SetRenderDrawColorFloat(renderer, 0.0f, 0.0f, 0.05f, 0.72f);
    SDL_RenderFillRect(renderer, &(SDL_FRect){0, 0, (float)screen_w, (float)screen_h});

    const float lh = (float)TTF_GetFontHeight(font);

    SDL_FColor title_color = {1.0f, 0.85f, 0.2f, 1.0f};
    draw_text_centered(renderer, font, "WEAPON SHOP",
                       screen_w * 0.5f, screen_h * 0.10f, title_color);

    char buf[192];
    SDL_FColor pts = {0.6f, 1.0f, 0.4f, 1.0f};
    snprintf(buf, sizeof(buf), "Points: %d", inv->points);
    draw_text_centered(renderer, font, buf, screen_w * 0.5f, screen_h * 0.10f + lh + 14.0f, pts);

    const float row_h = lh + 14.0f;
    float first_y = screen_h * 0.38f - ((float)SHOP_OPTION_COUNT * row_h) * 0.5f;

    const char *rows[SHOP_OPTION_COUNT] = {
        "Pistol",
        "Sword",
        "Grenades x5",
        "Launcher",
        "Launcher Ammo x5",
        "Close Shop"
    };

    for (int i = 0; i < menu->option_count; i++) {
        SDL_FColor opt_color;
        if (i == menu->selected_option) {
            opt_color = (SDL_FColor){1.0f, 1.0f, 0.3f, 1.0f};
        } else {
            opt_color = (SDL_FColor){0.6f, 0.6f, 0.65f, 0.85f};
        }

        char prefix[4] = "";
        if (i == menu->selected_option) snprintf(prefix, sizeof(prefix), "> ");

        char status[96] = "";
        switch (i) {
            case 0:
                if (inv->current == WEAPON_PISTOL) snprintf(status, sizeof(status), " [active]");
                break;
            case 1:
                if (inv->unlocked[WEAPON_SWORD]) {
                    snprintf(status, sizeof(status), " [owned%s]",
                             inv->current == WEAPON_SWORD ? ", active" : "");
                } else {
                    snprintf(status, sizeof(status), " [%d pts]", SWORD_COST);
                }
                break;
            case 2:
                if (inv->grenades > 0) {
                    snprintf(status, sizeof(status), " [owned %d] (%d pts)",
                             inv->grenades, GRENADE_PACK_COST);
                } else {
                    snprintf(status, sizeof(status), " [%d pts]", GRENADE_PACK_COST);
                }
                break;
            case 3:
                if (inv->unlocked[WEAPON_LAUNCHER]) {
                    snprintf(status, sizeof(status), " [owned%s, %d rkt]",
                             inv->current == WEAPON_LAUNCHER ? ", active" : "",
                             inv->launcher_ammo);
                } else {
                    snprintf(status, sizeof(status), " [%d pts]", LAUNCHER_COST);
                }
                break;
            case 4:
                if (!inv->unlocked[WEAPON_LAUNCHER]) {
                    snprintf(status, sizeof(status), " [need launcher]");
                } else {
                    snprintf(status, sizeof(status), " [owned %d] (%d pts)",
                             inv->launcher_ammo, LAUNCHER_AMMO_COST);
                }
                break;
            case 5:
                break;
        }

        snprintf(buf, sizeof(buf), "%s%s%s", prefix, rows[i], status);
        draw_text_centered(renderer, font, buf,
                           screen_w * 0.5f, first_y + i * row_h, opt_color);
    }

    if (menu->msg_timer > 0) {
        float blink = menu->msg_timer > 1.0f ? 1.0f : menu->msg_timer;
        SDL_FColor msg_color = {1.0f, 0.9f, 0.4f, blink};
        draw_text_centered(renderer, font, menu->message,
                           screen_w * 0.5f, screen_h * 0.80f, msg_color);
    }

    SDL_FColor hint = {0.35f, 0.35f, 0.4f, 0.7f};
    draw_text_centered(renderer, font,
                       "ENTER/SPACE: Buy/Switch  |  W/S: Navigate  |  B/ESC: Close",
                       screen_w * 0.5f, screen_h - lh - 16.0f, hint);
}
