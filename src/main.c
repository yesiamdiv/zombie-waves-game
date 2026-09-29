#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <signal.h>

#include "core/log.h"
#include "core/input.h"
#include "config.h"
#include "ecs/ecs.h"
#include "graphics/sprite.h"
#include "assets/asset_manager.h"
#include "world/world.h"
#include "world/camera.h"
#include "world/waves.h"
#include "world/map_registry.h"
#include "systems/systems.h"
#include "ui/menu.h"
#include "ui/hud.h"
#include "items/items.h"
#include "weapons/weapons.h"
#include "events/event_bus.h"
#include "ai/ai_driver.h"

#define WINDOW_W 1280
#define WINDOW_H 720
#define FIXED_STEP (1.0f / 120.0f)
/* Fallback frame budget (ms) when the renderer cannot do vsync: caps the
 * windowed loop so it never pegs a core flat. */
#define FRAME_BUDGET_MS 16.0

/* Zombie melee damage multiplier; 1.0 is normal play. Raise it (or lower player
 * HP) in scripted playtests to deterministically force damage, heal seeking,
 * and the death/game-over path (B5). */
float g_zombie_damage_mult = 1.0f;

/* Zombie movement-speed multiplier; 1.0 is normal play. Raise it so the horde
 * closes to melee even against an elite bot, making the melee->death->game-over
 * pipeline reachable in scripted playtests. */
float g_zombie_speed_mult = 1.0f;

typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    AssetManager assets;
    TTF_Font *font;
    TTF_Font *font_large;
    bool running;

    bool headless;
    bool vsync_active;
    float run_seconds;
    double elapsed_sim;

    /* Optional starting shop points, for exercising the shop / upgrade
     * pipeline in scripted playtests (like --player-hp). Applied after the
     * inventory is reset. */
    int start_points;

    /* Optional starting HP as a 0-100 percentage of max, for exercising the
     * heal economy and the death/game-over path in scripted playtests (B5).
     * -1 means "spawn with full health". */
    float player_hp_pct;

    GameState state;
    InputState input;
    World ecs;
    Camera camera;
    GameWorld world;
    WaveSystem waves;
    MainMenu main_menu;
    PauseMenu pause_menu;
    GameOverScreen gameover_screen;
    ShopMenu shop_menu;
    MapSelectMenu map_select;
    PlayerInventory inventory;
    HUD hud;

    /* AI driver */
    AIDriver ai;
    GameView ai_view;
    AIControls ai_controls;

    Entity player_entity;

    float item_spawn_timer;
    int last_announced_wave;
    const char *event_log_path;
} Game;

/* Defaults to full-health start; -1 disables the HP override. */
static Game game = {.player_hp_pct = -1.0f};

/* Set by the SIGINT/SIGTERM handler so headless runs can be stopped cleanly
 * (playtest finding B3). */
static volatile sig_atomic_t g_signal_stop = 0;

static void on_sigint(int sig) {
    (void)sig;
    g_signal_stop = 1;
}

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

    SDL_FColor player_color = theme_get(game.world.theme)->player_color;
    Sprite ps = sprite_rect(16.0f, 16.0f, player_color);
    SDL_Texture *player_tex = sprite_tex("textures/entities/player.png");
    if (player_tex) {
        /* 32px art at scale 0.5 renders as the original 16x16 world unit. */
        ps = sprite_texture(player_tex);
        ps.color = player_color;
    }
    *ecs_get_sprite(ecs, e) = (CSprite){
        .sprite = ps,
        .scale = 0.5f,
        .base_alpha = 1.0f
    };

    LOG_INFO("Player created at (%.0f, %.0f)", pos.x, pos.y);
    return e;
}

static void reset_game(void) {
    LOG_INFO("=== RESETTING GAME ===");

    ecs_init(&game.ecs);
    if (game.headless) {
        /* Headless runs keep the fixed classic world so scripts/tests stay
         * layout-stable; the map picker only applies to real gameplay. */
        world_init(&game.world);
    } else {
        const MapDef *m = map_registry_get(game.map_select.selected_option);
        if (!world_load_map(&game.world, m)) {
            LOG_WARN("map '%s' failed to load; using default world", m->name);
            world_init(&game.world);
        }
        LOG_INFO("Starting map: %s (%dx%d, %s)", m->name,
                 game.world.width, game.world.height, theme_name(game.world.theme));
    }
    waves_init(&game.waves, &game.world);
    weapons_inventory_init(&game.inventory);
    game.inventory.points = game.start_points > 0 ? game.start_points : 0;

    Vec2 spawn = world_get_spawn_point(&game.world);
    game.player_entity = create_player(&game.ecs, spawn);

    if (game.player_hp_pct >= 0.0f) {
        CHealth *hp = ecs_get_health(&game.ecs, game.player_entity);
        hp->current = hp->max * (game.player_hp_pct / 100.0f);
        if (hp->current <= 0.0f) hp->current = 1.0f;
        LOG_INFO("Player HP overridden to %.0f/%.0f (%.0f%%)",
                 hp->current, hp->max, game.player_hp_pct);
    }

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

        asset_manager_init(&game.assets, game.renderer);

        /* Synchronize present with the display refresh. Without vsync (and no
         * frame cap) the render loop spins a core flat at 100% CPU, which
         * starves the desktop compositor's input dispatch under Wayland - the
         * OS cursor freezes while movement keys are held (playtest finding).
         * If vsync is unsupported, fall back to a delay-based frame cap. */
        if (SDL_SetRenderVSync(game.renderer, 1)) {
            game.vsync_active = true;
            LOG_INFO("Renderer vsync enabled");
        } else {
            game.vsync_active = false;
            LOG_WARN("Renderer vsync unavailable (%s); using 60 FPS frame cap",
                     SDL_GetError());
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
    map_select_init(&game.map_select);
    pause_menu_init(&game.pause_menu);

    game.state = GAME_STATE_MENU;
    game.running = true;

    LOG_INFO("Game initialized successfully (SDL3)");
    return true;
}

static void shutdown(void) {
    LOG_INFO("Shutting down...");

    event_bus_shutdown(g_events);

    world_free(&game.world);
    map_select_free(&game.map_select);

    if (game.font) TTF_CloseFont(game.font);
    if (game.font_large) TTF_CloseFont(game.font_large);
    asset_manager_shutdown(&game.assets);
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
            if (next == GAME_STATE_MAP_SELECT) {
                game.state = GAME_STATE_MAP_SELECT;
                LOG_INFO("Opening map select");
            }
            if (game.input.quit_requested || game.main_menu.quit_requested) {
                game.running = false;
            }
            break;
        }

        case GAME_STATE_MAP_SELECT: {
            GameState next = map_select_update(&game.map_select, &game.input);
            if (next == GAME_STATE_PLAYING) {
                const MapDef *m = map_registry_get(game.map_select.selected_option);
                if (m) LOG_INFO("Map chosen: %s", m->name);
                reset_game();
                game.state = GAME_STATE_PLAYING;
                LOG_INFO("Game started from map select");
            } else if (next == GAME_STATE_MENU) {
                game.state = GAME_STATE_MENU;
                LOG_INFO("Back to menu from map select");
            }
            if (game.input.quit_requested) {
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

            /* Open the weapon shop (freezes gameplay until closed). */
            if (input_key_pressed(&game.input, SDL_SCANCODE_B)) {
                shop_menu_init(&game.shop_menu);
                game.state = GAME_STATE_SHOP;
                LOG_INFO("Shop opened");
                break;
            }

            /* Weapon switching (1-4). Locked weapons are rejected with a hint. */
            {
                WeaponType w = WEAPON_PISTOL;
                SDL_Scancode key = SDL_SCANCODE_UNKNOWN;
                if (input_key_pressed(&game.input, SDL_SCANCODE_1)) { w = WEAPON_PISTOL;   key = SDL_SCANCODE_1; }
                if (input_key_pressed(&game.input, SDL_SCANCODE_2)) { w = WEAPON_SWORD;    key = SDL_SCANCODE_2; }
                if (input_key_pressed(&game.input, SDL_SCANCODE_3)) { w = WEAPON_GRENADE;  key = SDL_SCANCODE_3; }
                if (input_key_pressed(&game.input, SDL_SCANCODE_4)) { w = WEAPON_LAUNCHER; key = SDL_SCANCODE_4; }
                if (key != SDL_SCANCODE_UNKNOWN && w != game.inventory.current) {
                    if (!weapons_select(&game.inventory, w)) {
                        hud_show_message(&game.hud, "Weapon locked - buy it in the shop (B)", 2.0f);
                    } else {
                        LOG_INFO("Selected weapon: %s", weapons_name(game.inventory.current));
                    }
                }
            }

            /* Systems update order */
            system_player_input(&game.ecs, &game.input, &game.camera, dt, &game.inventory);
            system_sword(&game.ecs, &game.input, &game.inventory, dt);
            system_zombie_ai(&game.ecs, dt);
            system_movement(&game.ecs, &game.world, dt);
            system_collision(&game.ecs, &game.world);
            system_bullets(&game.ecs, &game.world, dt);
            system_grenades(&game.ecs, &game.input, &game.inventory, dt);
            system_rockets(&game.ecs, &game.input, &game.inventory, &game.world, dt);
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
                items_check_pickup(&game.ecs, game.player_entity, &game.inventory);
            }

            game.item_spawn_timer += dt;
            if (game.item_spawn_timer > 15.0f &&
                items_count_alive(&game.ecs) < MAX_ALIVE_ITEMS) {
                game.item_spawn_timer = 0;
                Vec2 item_pos;
                item_pos.x = 200.0f + (float)(rand() % (int)(game.world.world_pixel_w - 400));
                item_pos.y = 200.0f + (float)(rand() % (int)(game.world.world_pixel_h - 400));
                if (world_is_walkable(&game.world, item_pos.x, item_pos.y)) {
                    items_spawn_random(&game.ecs, item_pos);
                }
            }

            system_cleanup(&game.ecs, &game.waves, &game.inventory);

            if (game.player_entity != ECS_NULL_ENTITY &&
                ecs_is_alive(&game.ecs, game.player_entity)) {
                camera_follow(&game.camera, ecs_get_position(&game.ecs, game.player_entity)->pos, dt);
                camera_clamp_world(&game.camera,
                                   game.world.world_pixel_w, game.world.world_pixel_h);
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

        case GAME_STATE_SHOP: {
            GameState next = shop_menu_update(&game.shop_menu, &game.input, &game.inventory);
            if (next == GAME_STATE_PLAYING) {
                game.state = GAME_STATE_PLAYING;
                LOG_INFO("Shop closed");
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
            if (game.input.quit_requested || game.ai.quit_requested) {
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

        case GAME_STATE_MAP_SELECT:
            map_select_draw(game.renderer, &game.map_select, win_w, win_h, game.font_large);
            break;

        case GAME_STATE_PLAYING:
        case GAME_STATE_PAUSED:
        case GAME_STATE_SHOP:
            world_draw(game.renderer, &game.world, &game.camera);
            system_render(&game.ecs, game.renderer, &game.camera);
            hud_draw(game.renderer, &game.hud, &game.ecs, &game.waves,
                     &game.input, &game.inventory, win_w, win_h, game.font);

            if (game.state == GAME_STATE_PAUSED) {
                pause_menu_draw(game.renderer, &game.pause_menu, win_w, win_h, game.font_large);
            } else if (game.state == GAME_STATE_SHOP) {
                shop_menu_draw(game.renderer, &game.shop_menu, &game.inventory, win_w, win_h, game.font_large);
            }
            break;

        case GAME_STATE_GAME_OVER:
            gameover_draw(game.renderer, &game.gameover_screen, win_w, win_h, game.font_large);
            break;
    }

    SDL_RenderPresent(game.renderer);
}

/* One simulation step. Handles input (real + injected), the AI driver,
 * game update, event log flushing, and input edge clearing. */
static void step_frame(float dt) {
    if (!game.headless) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            input_process_event(&game.input, &event);
        }
    }

    /* AI driver produces controls before systems read input */
    if (game.ai.mode != AI_MODE_NONE) {
        ai_apply_controls(&game.input, &game.ai_controls, &game.camera);
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

/* Built before the driver updates so it sees last frame's positions. */
static void update_ai_view(float dt) {
    if (game.ai.mode == AI_MODE_NONE) return;

    ai_build_view(&game.ai_view, &game.ecs, &game.waves);
    ai_driver_update(&game.ai, dt, &game.ai_view, &game.ai_controls);

    if (game.ai.quit_requested) {
        game.running = false;
    }
}

static void parse_args(int argc, char *argv[]) {
    bool ai_script_requested = false;

    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];

        if (strcmp(arg, "--headless") == 0) {
            game.headless = true;
        } else if (strncmp(arg, "--ai=", 5) == 0) {
            const char *mode = arg + 5;
            if (strcmp(mode, "bot") == 0) {
                ai_driver_set_bot(&game.ai);
            } else if (strcmp(mode, "script") == 0) {
                /* Decoupled from the script path so options can appear in
                 * any order (playtest finding B2). The actual path arrives
                 * via --script <file> or a trailing bare argument. */
                ai_script_requested = true;
            } else if (strcmp(mode, "none") == 0) {
                game.ai.mode = AI_MODE_NONE;
            } else {
                LOG_WARN("Unknown --ai mode '%s' (expected none|script|bot)", mode);
            }
        } else if (strcmp(arg, "--script") == 0 && i + 1 < argc) {
            const char *path = argv[++i];
            if (ai_driver_load_script(&game.ai, path) != 0) {
                LOG_WARN("Could not load script '%s'; falling back to no AI", path);
                game.ai.mode = AI_MODE_NONE;
            }
        } else if (strncmp(arg, "--script-loop=", 14) == 0) {
            ai_driver_set_script_loops(&game.ai, atoi(arg + 14));
        } else if (strncmp(arg, "--events=", 9) == 0) {
            game.event_log_path = arg + 9;
        } else if (strncmp(arg, "--seed=", 7) == 0) {
            srand((unsigned int)atoi(arg + 7));
        } else if (strcmp(arg, "--seed") == 0 && i + 1 < argc) {
            srand((unsigned int)atoi(argv[++i]));
        } else if (strncmp(arg, "--run-seconds=", 14) == 0) {
            game.run_seconds = (float)atof(arg + 14);
        } else if (strncmp(arg, "--points=", 9) == 0) {
            game.start_points = atoi(arg + 9);
            if (game.start_points < 0) game.start_points = 0;
        } else if (strncmp(arg, "--player-hp=", 12) == 0) {
            game.player_hp_pct = (float)atof(arg + 12);
            if (game.player_hp_pct < 0.0f) game.player_hp_pct = 0.0f;
            if (game.player_hp_pct > 100.0f) game.player_hp_pct = 100.0f;
        } else if (strncmp(arg, "--player-damage-mult=", 21) == 0) {
            float m = (float)atof(arg + 21);
            if (m < 0.0f) m = 0.0f;
            g_zombie_damage_mult = m;
        } else if (strncmp(arg, "--zombie-speed-mult=", 20) == 0) {
            float m = (float)atof(arg + 20);
            if (m < 0.0f) m = 0.0f;
            g_zombie_speed_mult = m;
        } else if (strcmp(arg, "--help") == 0) {
            printf("%s\n", "Usage: open_world_zombie_waves [options]");
            printf("%s\n", "  --headless            run with no window/renderer (fast, deterministic)");
            printf("%s\n", "  --ai=none|script|bot  AI control mode (default none)");
            printf("%s\n", "  --script <file>       scripted playback (also --script-loop=N)");
            printf("%s\n", "  --events=<file>       gameplay event log path (default game_events.log)");
            printf("%s\n", "  --seed=<n>            deterministic RNG seed");
            printf("%s\n", "  --run-seconds=<s>     auto-exit after s simulated seconds");
            printf("%s\n", "  --player-hp=<pct>     start player at pct%% HP (0-100; debug/playtest)");
            printf("%s\n", "  --points=<n>          start with n shop points (debug/playtest)");
            printf("%s\n", "  --player-damage-mult=<f>  zombie melee damage multiplier (debug/playtest)");
            printf("%s\n", "  --zombie-speed-mult=<f>   zombie move-speed multiplier (debug/playtest)");
            exit(0);
        } else if (ai_script_requested && game.ai.mode != AI_MODE_SCRIPT &&
                   arg[0] != '-') {
            /* Bare trailing path accepted when script mode was requested and
             * no script has been bound yet (order-independent). */
            if (ai_driver_load_script(&game.ai, arg) != 0) {
                LOG_WARN("Could not load script '%s'; falling back to no AI", arg);
                game.ai.mode = AI_MODE_NONE;
            }
        } else {
            LOG_WARN("Unknown argument: %s", arg);
        }
    }

    if (ai_script_requested && game.ai.mode != AI_MODE_SCRIPT) {
        LOG_WARN("--ai=script requested but no script could be loaded; running with no AI");
        game.ai.mode = AI_MODE_NONE;
    }
}

int main(int argc, char *argv[]) {
    srand((unsigned int)time(NULL));

    log_init("game.log", LOG_DEBUG, LOG_TRACE);
    LOG_INFO("========================================");
    LOG_INFO("  Open World Zombie Waves - Starting");
    LOG_INFO("========================================");

    ai_driver_init(&game.ai);

    signal(SIGINT, on_sigint);
    signal(SIGTERM, on_sigint);

    parse_args(argc, argv);

    if (!game.event_log_path) game.event_log_path = "game_events.log";
    event_bus_init(game.event_log_path);

    if (!init()) {
        LOG_FATAL("Initialization failed!");
        return 1;
    }

    /* With an AI driver, skip straight into a game so it can play. */
    if (game.ai.mode != AI_MODE_NONE) {
        reset_game();
        game.state = GAME_STATE_PLAYING;
        LOG_INFO("AI driver active - starting game directly");
    }

    Uint64 last_time = SDL_GetPerformanceCounter();
    Uint64 freq = SDL_GetPerformanceFrequency();
    double accumulator = 0.0;

    while (game.running && !g_signal_stop) {
        Uint64 current_time = SDL_GetPerformanceCounter();
        float frame_dt = (float)(current_time - last_time) / (float)freq;
        last_time = current_time;
        if (frame_dt > 0.25f) frame_dt = 0.25f;

        if (game.headless) {
            /* Fixed-timestep headless simulation, as fast as possible */
            accumulator += frame_dt;
            while (accumulator >= FIXED_STEP && game.running && !g_signal_stop) {
                update_ai_view(FIXED_STEP);
                step_frame(FIXED_STEP);
                render();
                accumulator -= FIXED_STEP;

                game.elapsed_sim += FIXED_STEP;
                if (game.run_seconds > 0 && game.elapsed_sim >= game.run_seconds) {
                    game.running = false;
                }
            }
        } else {
            float dt = frame_dt;
            if (dt > 0.05f) dt = 0.05f;

            update_ai_view(dt);
            step_frame(dt);
            render();

            if (!game.vsync_active) {
                /* No vsync: cap the frame rate so a single rendering loop
                 * cannot peg the CPU and stall compositor input. */
                Uint64 now = SDL_GetPerformanceCounter();
                double frame_ms = (double)(now - current_time) * 1000.0 / (double)freq;
                if (frame_ms < FRAME_BUDGET_MS) {
                    SDL_Delay((Uint32)(FRAME_BUDGET_MS - frame_ms));
                }
            }

            game.elapsed_sim += dt;
            if (game.run_seconds > 0 && game.elapsed_sim >= game.run_seconds) {
                game.running = false;
            }
        }
    }

    if (g_signal_stop) {
        LOG_INFO("Interrupt received; shutting down cleanly");
    }

    shutdown();
    return 0;
}