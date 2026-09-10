#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "core/log.h"
#include "core/input.h"
#include "ecs/ecs.h"
#include "graphics/sprite.h"
#include "world/world.h"
#include "world/camera.h"
#include "world/waves.h"
#include "systems/systems.h"
#include "ui/menu.h"
#include "ui/hud.h"
#include "items/items.h"
#include "events/event_bus.h"

#define WINDOW_W 1280
#define WINDOW_H 720
#define FIXED_STEP (1.0f / 120.0f)

typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    TTF_Font *font;
    TTF_Font *font_large;
    bool running;

    bool headless;
    float run_seconds;
    double elapsed_sim;

    GameState state;
    InputState input;
    World ecs;
    Camera camera;
    GameWorld world;
    WaveSystem waves;
    MainMenu main_menu;
    PauseMenu pause_menu;
    GameOverScreen gameover_screen;
    HUD hud;

    Entity player_entity;

    float item_spawn_timer;
    int last_announced_wave;
    const char *event_log_path;
} Game;

static Game game = {0};

static Entity create_player(World *ecs, Vec2 pos) {
    Entity e = ecs_create_entity(ecs);
    if (e == ECS_NULL_ENTITY) return e;

    ecs_add_component(ecs, e, COMP_POSITION);
    ecs_add_component(ecs, e, COMP_VELOCITY);
    ecs_add_component(ecs, e, COMP_HEALTH);
    ecs_add_component(ecs, e, COMP_SPRITE);
    ecs_add_component(ecs, e, COMP_COLLIDER);
    ecs_add_component(ecs, e, COMP_PLAYER_TAG);

    *ecs_get_position(ecs, e) = (CPosition){{pos.x, pos.y}};
    *ecs_get_velocity(ecs, e) = (CVelocity){{0, 0}, 200.0f};
    *ecs_get_health(ecs, e) = (CHealth){200.0f, 200.0f};
    *ecs_get_collider(ecs, e) = (CCollider){14.0f, false};

    *ecs_get_sprite(ecs, e) = (CSprite){
        .sprite = sprite_rect(16.0f, 16.0f, COLOR_BLUE),
        .scale = 1.0f,
        .base_alpha = 1.0f
    };

    LOG_INFO("Player created at (%.0f, %.0f)", pos.x, pos.y);
    return e;
}

static void reset_game(void) {
    LOG_INFO("=== RESETTING GAME ===");

    ecs_init(&game.ecs);
    world_init(&game.world);
    waves_init(&game.waves, &game.world);

    Vec2 spawn = world_get_spawn_point(&game.world);
    game.player_entity = create_player(&game.ecs, spawn);

    camera_set_position(&game.camera, spawn);

    hud_init(&game.hud);
    hud_show_message(&game.hud, "Survive the zombie horde!", 3.0f);

    game.item_spawn_timer = 0;
    game.last_announced_wave = 0;
}

static void set_game_over(void) {
    game.state = GAME_STATE_GAME_OVER;
    int score = game.waves.total_kills * 100 + (game.waves.wave_number - 1) * 500;
    gameover_init(&game.gameover_screen, score,
                  game.waves.wave_number, game.waves.total_kills);
    LOG_INFO("GAME OVER - Score: %d, Wave: %d, Kills: %d",
             score, game.waves.wave_number, game.waves.total_kills);
}

static bool init(void) {
    Uint32 sdl_flags = game.headless ? SDL_INIT_EVENTS : SDL_INIT_VIDEO;
    if (!SDL_Init(sdl_flags)) {
        LOG_FATAL("SDL_Init failed: %s", SDL_GetError());
        return false;
    }

    if (game.headless) {
        LOG_INFO("Running HEADLESS (no window/renderer)");
    } else {
        if (!TTF_Init()) {
            LOG_FATAL("TTF_Init failed: %s", SDL_GetError());
            return false;
        }

        game.window = SDL_CreateWindow(
            "Open World Zombie Waves",
            WINDOW_W, WINDOW_H,
            SDL_WINDOW_RESIZABLE
        );
        if (!game.window) {
            LOG_FATAL("SDL_CreateWindow failed: %s", SDL_GetError());
            return false;
        }

        game.renderer = SDL_CreateRenderer(game.window, NULL);
        if (!game.renderer) {
            LOG_FATAL("SDL_CreateRenderer failed: %s", SDL_GetError());
            return false;
        }

        game.font = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 18);
        game.font_large = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 36);
        if (!game.font) {
            LOG_WARN("Failed to load DejaVu font, trying fallback...");
            game.font = TTF_OpenFont("/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf", 18);
            game.font_large = TTF_OpenFont("/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf", 36);
        }
        if (!game.font) {
            LOG_WARN("No font loaded, text will not render");
        }
    }

    if (g_events == NULL) {
        event_bus_init(game.event_log_path);
    }

    input_init(&game.input);
    camera_init(&game.camera, WINDOW_W, WINDOW_H);
    menu_init(&game.main_menu);
    pause_menu_init(&game.pause_menu);

    game.state = GAME_STATE_MENU;
    game.running = true;

    LOG_INFO("Game initialized successfully (SDL3)");
    return true;
}

static void shutdown(void) {
    LOG_INFO("Shutting down...");

    event_bus_shutdown(g_events);

    if (game.font) TTF_CloseFont(game.font);
    if (game.font_large) TTF_CloseFont(game.font_large);
    if (game.renderer) SDL_DestroyRenderer(game.renderer);
    if (game.window) SDL_DestroyWindow(game.window);
    if (!game.headless) TTF_Quit();

    SDL_Quit();
    log_shutdown();
}

static void update(float dt) {
    switch (game.state) {
        case GAME_STATE_MENU: {
            GameState next = menu_update(&game.main_menu, &game.input, dt);
            if (next == GAME_STATE_PLAYING) {
                reset_game();
                game.state = GAME_STATE_PLAYING;
                LOG_INFO("Game started from menu");
            }
            if (game.input.quit_requested || game.main_menu.quit_requested) {
                game.running = false;
            }
            break;
        }

        case GAME_STATE_PLAYING: {
            if (input_key_pressed(&game.input, SDL_SCANCODE_ESCAPE)) {
                game.state = GAME_STATE_PAUSED;
                pause_menu_init(&game.pause_menu);
                LOG_INFO("Game paused");
                break;
            }

            /* Systems update order */
            system_player_input(&game.ecs, &game.input, &game.camera, dt);
            system_zombie_ai(&game.ecs, dt);
            system_movement(&game.ecs, &game.world, dt);
            system_collision(&game.ecs, &game.world);
            system_bullets(&game.ecs, &game.world, dt);
            system_animation(&game.ecs, dt);
            system_particles(&game.ecs, dt);

            waves_update(&game.waves, &game.ecs, &game.world, dt);

            if (game.waves.wave_number != game.last_announced_wave &&
                game.waves.wave_active) {
                game.last_announced_wave = game.waves.wave_number;
                char msg[64];
                snprintf(msg, sizeof(msg), "Wave %d - %d zombies incoming!",
                         game.waves.wave_number, game.waves.zombies_per_wave);
                hud_show_message(&game.hud, msg, 3.0f);
            }

            if (game.player_entity != ECS_NULL_ENTITY) {
                items_check_pickup(&game.ecs, game.player_entity);
            }

            game.item_spawn_timer += dt;
            if (game.item_spawn_timer > 15.0f) {
                game.item_spawn_timer = 0;
                Vec2 item_pos;
                item_pos.x = 200.0f + (float)(rand() % (int)(game.world.world_pixel_w - 400));
                item_pos.y = 200.0f + (float)(rand() % (int)(game.world.world_pixel_h - 400));
                if (world_is_walkable(&game.world, item_pos.x, item_pos.y)) {
                    items_spawn_random(&game.ecs, item_pos);
                }
            }

            system_cleanup(&game.ecs, &game.waves);

            if (game.player_entity != ECS_NULL_ENTITY &&
                ecs_is_alive(&game.ecs, game.player_entity)) {
                camera_follow(&game.camera, ecs_get_position(&game.ecs, game.player_entity)->pos, dt);
            }

            hud_update(&game.hud, dt);

            if (game.player_entity == ECS_NULL_ENTITY ||
                !ecs_is_alive(&game.ecs, game.player_entity)) {
                set_game_over();
            }

            if (game.input.quit_requested) {
                game.running = false;
            }
            break;
        }

        case GAME_STATE_PAUSED: {
            GameState next = pause_menu_update(&game.pause_menu, &game.input);
            if (next == GAME_STATE_PLAYING) {
                game.state = GAME_STATE_PLAYING;
                LOG_INFO("Game resumed");
            } else if (next == GAME_STATE_MENU) {
                game.state = GAME_STATE_MENU;
                LOG_INFO("Quit to menu from pause");
            }
            if (game.input.quit_requested) {
                game.running = false;
            }
            break;
        }

        case GAME_STATE_GAME_OVER: {
            GameState next = gameover_update(&game.gameover_screen, &game.input);
            if (next == GAME_STATE_MENU) {
                game.state = GAME_STATE_MENU;
            }
            if (game.input.quit_requested) {
                game.running = false;
            }
            break;
        }
    }
}

static void render(void) {
    if (game.headless) return;

    int win_w, win_h;
    SDL_GetWindowSize(game.window, &win_w, &win_h);
    game.camera.viewport_w = win_w;
    game.camera.viewport_h = win_h;

    SDL_SetRenderDrawColorFloat(game.renderer, 0.08f, 0.08f, 0.12f, 1.0f);
    SDL_RenderClear(game.renderer);

    switch (game.state) {
        case GAME_STATE_MENU:
            menu_draw(game.renderer, &game.main_menu, win_w, win_h, game.font_large);
            break;

        case GAME_STATE_PLAYING:
        case GAME_STATE_PAUSED:
            world_draw(game.renderer, &game.world, &game.camera);
            system_render(&game.ecs, game.renderer, &game.camera);
            hud_draw(game.renderer, &game.hud, &game.ecs, &game.waves,
                     &game.input, win_w, win_h, game.font);

            if (game.state == GAME_STATE_PAUSED) {
                pause_menu_draw(game.renderer, &game.pause_menu, win_w, win_h, game.font_large);
            }
            break;

        case GAME_STATE_GAME_OVER:
            gameover_draw(game.renderer, &game.gameover_screen, win_w, win_h, game.font_large);
            break;
    }

    SDL_RenderPresent(game.renderer);
}

/* One simulation step. Handles input, game update, and event log flushing. */
static void step_frame(float dt) {
    if (!game.headless) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            input_process_event(&game.input, &event);
        }
    }

    update(dt);

    if (g_events) {
        event_bus_tick(g_events, dt);
        if (game.state == GAME_STATE_PLAYING) {
            event_bus_samples(g_events, &game.ecs, 0, 0);
        }
        event_bus_flush(g_events);
    }

    input_update(&game.input);
}

static void parse_args(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];

        if (strcmp(arg, "--headless") == 0) {
            game.headless = true;
        } else if (strncmp(arg, "--events=", 9) == 0) {
            game.event_log_path = arg + 9;
        } else if (strncmp(arg, "--seed=", 7) == 0) {
            srand((unsigned int)atoi(arg + 7));
        } else if (strcmp(arg, "--seed") == 0 && i + 1 < argc) {
            srand((unsigned int)atoi(argv[++i]));
        } else if (strncmp(arg, "--run-seconds=", 14) == 0) {
            game.run_seconds = (float)atof(arg + 14);
        } else {
            LOG_WARN("Unknown argument: %s", arg);
        }
    }
}

int main(int argc, char *argv[]) {
    srand((unsigned int)time(NULL));

    log_init("game.log", LOG_DEBUG, LOG_TRACE);
    LOG_INFO("========================================");
    LOG_INFO("  Open World Zombie Waves - Starting");
    LOG_INFO("========================================");

    parse_args(argc, argv);

    if (!game.event_log_path) game.event_log_path = "game_events.log";
    event_bus_init(game.event_log_path);

    if (!init()) {
        LOG_FATAL("Initialization failed!");
        return 1;
    }

    Uint64 last_time = SDL_GetPerformanceCounter();
    Uint64 freq = SDL_GetPerformanceFrequency();
    double accumulator = 0.0;

    while (game.running) {
        Uint64 current_time = SDL_GetPerformanceCounter();
        float frame_dt = (float)(current_time - last_time) / (float)freq;
        last_time = current_time;
        if (frame_dt > 0.25f) frame_dt = 0.25f;

        if (game.headless) {
            /* Fixed-timestep headless simulation, as fast as possible */
            accumulator += frame_dt;
            while (accumulator >= FIXED_STEP && game.running) {
                step_frame(FIXED_STEP);
                accumulator -= FIXED_STEP;

                game.elapsed_sim += FIXED_STEP;
                if (game.run_seconds > 0 && game.elapsed_sim >= game.run_seconds) {
                    game.running = false;
                }
            }
        } else {
            float dt = frame_dt;
            if (dt > 0.05f) dt = 0.05f;

            step_frame(dt);

            game.elapsed_sim += dt;
            if (game.run_seconds > 0 && game.elapsed_sim >= game.run_seconds) {
                game.running = false;
            }
        }
    }

    shutdown();
    return 0;
}