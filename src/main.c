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
#include "world/world.h"
#include "world/camera.h"
#include "world/waves.h"
#include "systems/systems.h"
#include "ui/menu.h"
#include "ui/hud.h"
#include "items/items.h"
#include "weapons/weapons.h"
#include "events/event_bus.h"
#include "ai/ai_driver.h"
#include "game_mode.h"
#include "players.h"
#include "net/net.h"
#include "net/net_server.h"
#include "net/net_client.h"
#include "net/net_mirror.h"

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
    TTF_Font *font;
    TTF_Font *font_large;
    bool running;

    bool headless;
    bool vsync_active;
    GameMode mode;
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
    /* Local raw input: SDL events are polled into this, the AI driver may
     * inject controls, and the menu/pause/shop states read it directly. During
     * gameplay the local slot's InputState is mirrored from this every frame
     * so the systems read through the per-player slot (multiplayer-safe). */
    InputState input;
    World ecs;
    Camera camera;
    GameWorld world;
    WaveSystem waves;
    MainMenu main_menu;
    PauseMenu pause_menu;
    GameOverScreen gameover_screen;
    ShopMenu shop_menu;
    Player players[MAX_PLAYERS];
    HUD hud;

    /* AI driver */
    AIDriver ai;
    GameView ai_view;
    AIControls ai_controls;

    float item_spawn_timer;
    int last_announced_wave;
    const char *event_log_path;

    /* Multiplayer session (P1). A session is either hosting a listen-server
     * or joining one; both are driven by CLI flags for now (menu wiring is
     * P1e). */
    bool net_host_mode;
    uint16_t net_port;
    char net_join_addr[64];
    char net_player_name[NET_NAME_CAP];
    uint32_t net_seed;          /* 0 = derive from RNG at host time */
    NetServer net_server;
    NetClient net_client;

    /* P2: net input cadence + client-side weapon selection. */
    float net_input_tx_accum;
    uint8_t net_weapon;
    float net_snap_accum;   /* host: 20 Hz snapshot cadence accumulator */
    NetMirror net_mirror;   /* client: interpolated world mirror */
    bool auto_start;        /* --auto-start: host starts immediately (headless) */
} Game;

/* Defaults to full-health start; -1 disables the HP override. */
static Game game = {
    .player_hp_pct = -1.0f,
    .mode = GAME_MODE_SINGLE,
    .net_port = NET_DEFAULT_PORT,
};

/* Set by the SIGINT/SIGTERM handler so headless runs can be stopped cleanly
 * (playtest finding B3). */
static volatile sig_atomic_t g_signal_stop = 0;

static void on_sigint(int sig) {
    (void)sig;
    g_signal_stop = 1;
}

static void reset_game(void) {
    LOG_INFO("=== RESETTING GAME ===");

    ecs_init(&game.ecs);
    world_init(&game.world);
    waves_init(&game.waves, &game.world);
    players_reset(game.players, MAX_PLAYERS);

    /* Single-player: the local player occupies slot 0. */
    Vec2 spawn = world_get_spawn_point(&game.world);
    const char *local_name = game.net_host_mode
                                 ? game.net_player_name
                                 : "Player";
    if (player_respawn(game.players, &game.ecs, 0, local_name,
                       &COLOR_BLUE, spawn) < 0) {
        LOG_FATAL("Failed to spawn local player");
        return;
    }
    /* Multiplayer host: spawn a player entity for every joined remote slot so
     * remote input, snapshots, and the client mirror all carry them. Each
     * remote spawn fans out around the local player's spawn point. */
    if (game.net_host_mode) {
        for (int s = 1; s < MAX_PLAYERS; s++) {
            if (!game.net_server.slot_used[s]) continue;
            SDL_FColor col = net_slot_color(s);
            Vec2 ps = vec2(spawn.x + (float)(s * 70), spawn.y + (float)(s * 30));
            if (player_respawn(game.players, &game.ecs, s,
                               game.net_server.slot_names[s][0]
                                   ? game.net_server.slot_names[s]
                                   : "Player",
                               &col, ps) < 0) {
                LOG_FATAL("Failed to spawn remote player (slot %d)", s);
                return;
            }
        }
    }
    if (game.start_points > 0) {
        game.players[0].inventory.points = game.start_points;
    }

    if (game.player_hp_pct >= 0.0f) {
        CHealth *hp = ecs_get_health(&game.ecs, game.players[0].entity);
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

/* R13-C2 fix: keep the game's player slots reconciled with the host's net
 * slot table. reset_game() only ever sees the peers present when the match
 * starts, so a peer that joins mid-match (or leaves) would otherwise never get
 * a player entity and the session would look like a "1-player" game. Call after
 * every net_server_update() on the host. */
static void sync_remote_player_slots(void) {
    if (!game.net_host_mode) return;

    for (int s = 1; s < MAX_PLAYERS; s++) {
        bool occupied = game.net_server.slot_used[s];
        if (occupied && !game.players[s].in_use) {
            SDL_FColor col = net_slot_color(s);
            Vec2 ps;
            if (game.players[0].entity != ECS_NULL_ENTITY) {
                const CPosition *p0 =
                    ecs_get_position(&game.ecs, game.players[0].entity);
                ps = vec2(p0->pos.x + (float)(s * 70), p0->pos.y + (float)(s * 30));
            } else {
                ps = vec2((float)(s * 70), (float)(s * 30));
            }
            if (player_respawn(game.players, &game.ecs, s,
                               game.net_server.slot_names[s][0]
                                   ? game.net_server.slot_names[s]
                                   : "Player",
                               &col, ps) >= 0) {
                LOG_INFO("Spawned remote player entity for slot %d ('%s')",
                         s, game.net_server.slot_names[s]);
            } else {
                LOG_ERROR("Failed to spawn remote player (slot %d)", s);
            }
        } else if (!occupied && game.players[s].in_use) {
            if (game.players[s].entity != ECS_NULL_ENTITY) {
                ecs_destroy_entity(&game.ecs, game.players[s].entity);
            }
            memset(&game.players[s], 0, sizeof(game.players[s]));
            LOG_INFO("Released remote player slot %d (peer left)", s);
        }
    }
}

static void set_game_over(void) {
    game.state = GAME_STATE_GAME_OVER;
    int score = game.waves.total_kills * 100 + (game.waves.wave_number - 1) * 500;
    gameover_init(&game.gameover_screen, score,
                  game.waves.wave_number, game.waves.total_kills);
    LOG_INFO("GAME OVER - Score: %d, Wave: %d, Kills: %d",
             score, game.waves.wave_number, game.waves.total_kills);
}

/* Start the multiplayer session requested on the CLI: --host listens and
 * enters the lobby immediately; --join connects and enters CONNECTING until
 * the handshake resolves. Returns false if the session cannot begin. */
static bool begin_net_session(void) {
    if (!game.net_host_mode && game.net_join_addr[0] == '\0') return true;
    if (game.mode == GAME_MODE_SINGLE) {
        LOG_INFO("Multiplayer requested; defaulting mode to multi-tdm");
        game.mode = GAME_MODE_MULTI_TDM;
    }
    if (game.net_player_name[0] == '\0') {
        snprintf(game.net_player_name, sizeof(game.net_player_name), "Player");
    }

    if (game.net_host_mode) {
        uint32_t seed = game.net_seed != 0 ? game.net_seed : (uint32_t)rand();
        LOG_INFO("NET: hosting lobby on port %u as '%s' (seed=%u)",
                 (unsigned)game.net_port, game.net_player_name, seed);
        if (net_server_host(&game.net_server, game.net_port,
                            game.net_player_name, seed,
                            NET_WORLD_GEN_VERSION) != 0) {
            LOG_ERROR("Failed to host lobby on port %u",
                      (unsigned)game.net_port);
            return false;
        }
        game.state = GAME_STATE_LOBBY;
    } else {
        char ip[64];
        uint16_t port;
        if (net_parse_host_port(game.net_join_addr, ip, sizeof(ip), &port) != 0) {
            LOG_ERROR("Bad join address '%s' (expected ip[:port])",
                      game.net_join_addr);
            return false;
        }
        if (net_client_init(&game.net_client) != 0 ||
            net_client_connect(&game.net_client, ip, port,
                               game.net_player_name) != 0) {
            LOG_ERROR("Failed to start join to %s", game.net_join_addr);
            return false;
        }
        game.state = GAME_STATE_CONNECTING;
    }
    return true;
}

/* Leave a live net session and drop back to the main menu (idempotent). */
static void leave_net_session(void) {
    net_mirror_reset(&game.net_mirror);
    if (game.net_host_mode) {
        if (game.net_server.state != NET_SERVER_OFFLINE) {
            LOG_INFO("NET: leaving lobby (stopping host)");
            net_server_shutdown(&game.net_server);
        }
    } else {
        if (game.net_client.state != NET_CLIENT_OFFLINE) {
            net_client_shutdown(&game.net_client);
        }
    }
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
    pause_menu_init(&game.pause_menu);

    game.state = GAME_STATE_MENU;
    game.running = true;

    LOG_INFO("Game initialized successfully (SDL3)");
    return true;
}

static void shutdown_game(void) {
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

/* True when this process is a render-only net client (joins the host's sim,
 * never runs one itself). */
static bool render_only_client(void);

/* Client: drain the newest received snapshot into the interpolation mirror
 * exactly once (by sequence), so frames between 20 Hz snapshots blend. */
static void drain_mirror(void);

/* Client: interpolated position of the local player's mirror entity (own
 * roster slot), driving the camera on render-only clients. */
static bool client_local_pos(Vec2 *out) {
    if (!out || !net_mirror_ready(&game.net_mirror)) return false;
    int slot = game.net_client.slot;
    if (slot < 0 || slot >= NET_MAX_PLAYERS) return false;
    uint16_t id = game.net_mirror.newer.slot_entities[slot];
    if (id == 0) return false;

    float t_new = net_mirror_newer_time(&game.net_mirror);
    float t_old = net_mirror_older_time(&game.net_mirror);
    float t = 1.0f;
    if (t_new > t_old) {
        t = ((float)game.elapsed_sim - t_old) / (t_new - t_old);
        if (t < 0.0f) t = 0.0f;
        else if (t > 1.0f) t = 1.0f;
    }
    NetEntitySnap e;
    if (!net_mirror_sample(&game.net_mirror, id, t, &e)) return false;
    *out = e.pos;
    return true;
}

static void update(float dt) {
    switch (game.state) {
        case GAME_STATE_MENU: {
            GameState next = menu_update(&game.main_menu, &game.input, dt);
            switch (next) {
                case GAME_STATE_PLAYING:
                    reset_game();
                    game.state = GAME_STATE_PLAYING;
                    LOG_INFO("Game started from menu");
                    break;
                case GAME_STATE_LOBBY:
                    if (game.main_menu.host_requested) {
                        game.main_menu.host_requested = false;
                        game.net_host_mode = true;
                        if (game.net_player_name[0] == '\0') {
                            snprintf(game.net_player_name, sizeof(game.net_player_name),
                                     "Host");
                        }
                        if (!begin_net_session()) {
                            game.state = GAME_STATE_MENU;
                        }
                    }
                    break;
                case GAME_STATE_CONNECTING:
                    if (game.main_menu.join_requested) {
                        game.main_menu.join_requested = false;
                        game.net_host_mode = false;
                        snprintf(game.net_join_addr, sizeof(game.net_join_addr),
                                 "%s", game.main_menu.join_address);
                        if (game.net_player_name[0] == '\0') {
                            snprintf(game.net_player_name, sizeof(game.net_player_name),
                                     "Player");
                        }
                        if (!begin_net_session()) {
                            game.state = GAME_STATE_MENU;
                        }
                    }
                    break;
                default:
                    break;
            }
            if (game.input.quit_requested || game.main_menu.quit_requested) {
                game.running = false;
            }
            break;
        }

        case GAME_STATE_CONNECTING: {
            net_client_update(&game.net_client);
            const NetClient *c = &game.net_client;
            if (c->state == NET_CLIENT_CONNECTED) {
                game.state = GAME_STATE_LOBBY;
                LOG_INFO("Connected to '%s' (%s) - entering lobby", c->host_name,
                         c->host_addr);
            } else if (c->state == NET_CLIENT_REJECTED) {
                LOG_ERROR("Join to %s rejected by host: %s",
                          c->host_addr, net_reject_reason_name(c->reject_reason));
                net_client_shutdown(&game.net_client);
                game.state = GAME_STATE_MENU;
            } else if (c->state == NET_CLIENT_OFFLINE) {
                LOG_ERROR("Join to %s failed", c->host_addr);
                game.state = GAME_STATE_MENU;
            }
            if (input_key_pressed(&game.input, SDL_SCANCODE_ESCAPE) ||
                game.input.quit_requested) {
                net_client_shutdown(&game.net_client);
                game.state = GAME_STATE_MENU;
                LOG_INFO("Join attempt cancelled");
            }
            break;
        }

        case GAME_STATE_LOBBY: {
            if (game.net_host_mode) {
                net_server_update(&game.net_server);
                sync_remote_player_slots();
                /* Host starts the match; clients must wait for the P2
                 * snapshot stream before gameplay can begin. */
                /* Host starts the match under the same semantic as the Enter/Space
         * shortcut; --auto-start exists for headless verification runs. */
        if (game.auto_start ||
            input_key_pressed(&game.input, SDL_SCANCODE_RETURN) ||
            input_key_pressed(&game.input, SDL_SCANCODE_SPACE)) {
                    reset_game();
                    game.state = GAME_STATE_PLAYING;
                    LOG_INFO("Match starting from lobby (%s)",
                             game_mode_name(game.mode));
                }
            } else {
                const NetClient *c = &game.net_client;
                if (c->state == NET_CLIENT_REJECTED) {
                    LOG_ERROR("Rejected by host: %s",
                              net_reject_reason_name(c->reject_reason));
                    net_client_shutdown(&game.net_client);
                    game.state = GAME_STATE_MENU;
                } else if (c->state == NET_CLIENT_OFFLINE || c->server_stopped) {
                    LOG_ERROR("Connection to host (%s) lost", c->host_addr);
                    net_client_shutdown(&game.net_client);
                    game.state = GAME_STATE_MENU;
                } else {
                    /* Non-destructive pump: ignore a handshake while already
                     * connected (re-broadcasts, late roster updates). */
                    net_client_update(&game.net_client);
                    const NetClient *pc = &game.net_client;
                    /* The host is authoritative: the match begins the moment
                     * the first 20 Hz snapshot arrives (render-only client). */
                    if (pc->state == NET_CLIENT_CONNECTED && pc->snap_valid) {
                        net_mirror_reset(&game.net_mirror);
                        reset_game();
                        game.state = GAME_STATE_PLAYING;
                        LOG_INFO("Match started (first snapshot from host)");
                    }
                }
            }
            if (input_key_pressed(&game.input, SDL_SCANCODE_ESCAPE) ||
                game.input.quit_requested) {
                leave_net_session();
                game.state = GAME_STATE_MENU;
                LOG_INFO("Left lobby");
            }
            break;
        }

        case GAME_STATE_PLAYING: {
            /* P1: keep the net layer serviced during play so leaves/disconnects
             * are observed (P2 adds real snapshot replication). */
            if (game.net_host_mode) {
                net_server_update(&game.net_server);
                sync_remote_player_slots();
            } else if (game.net_client.host) {
                net_client_update(&game.net_client);
            }

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

            /* Weapon switching (1-4). Locked weapons are rejected with a hint. A
             * render-only client has no inventory - it just queues the
             * selection so the host applies it. */
            {
                PlayerInventory *local_inv = &game.players[0].inventory;
                WeaponType w = WEAPON_PISTOL;
                SDL_Scancode key = SDL_SCANCODE_UNKNOWN;
                if (input_key_pressed(&game.input, SDL_SCANCODE_1)) { w = WEAPON_PISTOL;   key = SDL_SCANCODE_1; }
                if (input_key_pressed(&game.input, SDL_SCANCODE_2)) { w = WEAPON_SWORD;    key = SDL_SCANCODE_2; }
                if (input_key_pressed(&game.input, SDL_SCANCODE_3)) { w = WEAPON_GRENADE;  key = SDL_SCANCODE_3; }
                if (input_key_pressed(&game.input, SDL_SCANCODE_4)) { w = WEAPON_LAUNCHER; key = SDL_SCANCODE_4; }
                if (key != SDL_SCANCODE_UNKNOWN) {
                    if (game.net_host_mode) {
                        if (w != local_inv->current) {
                            if (!weapons_select(local_inv, w)) {
                                hud_show_message(&game.hud, "Weapon locked - buy it in the shop (B)", 2.0f);
                            } else {
                                LOG_INFO("Selected weapon: %s", weapons_name(local_inv->current));
                            }
                        }
                    } else {
                        game.net_weapon = (uint8_t)w;
                        LOG_INFO("Net client selected weapon: %s", weapons_name(w));
                    }
                }
            }

            /* Render-only client: no local simulation. The host's snapshot
             * mirror drives the camera; the world is drawn from the mirror. */
            if (render_only_client()) {
                Vec2 p;
                if (client_local_pos(&p)) {
                    camera_follow(&game.camera, p, dt);
                }
                hud_update(&game.hud, dt);
                break;
            }

            /* Systems update order - run once per resident player slot so
             * every player's input/inventory resolve against their own slot. */
            for (int s = 0; s < MAX_PLAYERS; s++) {
                if (!game.players[s].in_use) continue;
                system_player_input(&game.ecs, &game.players[s], &game.camera, dt);
                system_sword(&game.ecs, &game.players[s], dt);
                system_grenades(&game.ecs, &game.players[s], dt);
                system_rockets(&game.ecs, &game.players[s], &game.world, dt);
            }
            system_zombie_ai(&game.ecs, game.players, MAX_PLAYERS, dt);
            system_movement(&game.ecs, &game.world, dt);
            system_collision(&game.ecs, &game.world);
            system_bullets(&game.ecs, &game.world, dt);
            system_animation(&game.ecs, dt);
            system_particles(&game.ecs, dt);

            /* R13/D3: scale with the REAL occupied slot count, never MAX_PLAYERS,
             * so single-player stays byte-identical (player_count == 1). */
            waves_update(&game.waves, &game.ecs, &game.world,
                         game.players, players_active_count(game.players, MAX_PLAYERS), dt);

            if (game.waves.wave_number != game.last_announced_wave &&
                game.waves.wave_active) {
                game.last_announced_wave = game.waves.wave_number;
                char msg[64];
                snprintf(msg, sizeof(msg), "Wave %d - %d zombies incoming!",
                         game.waves.wave_number, game.waves.zombies_per_wave);
                hud_show_message(&game.hud, msg, 3.0f);
            }

            for (int s = 0; s < MAX_PLAYERS; s++) {
                if (!game.players[s].in_use || game.players[s].entity == ECS_NULL_ENTITY) continue;
                if (!ecs_is_alive(&game.ecs, game.players[s].entity)) continue;
                items_check_pickup(&game.ecs, game.players[s].entity);
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

            system_cleanup(&game.ecs, &game.waves, game.players, MAX_PLAYERS);

            int alive_count = players_match_update(&game.ecs, game.players,
                                                   MAX_PLAYERS, game.mode, dt);

            Player *local = &game.players[0];
            if (local->entity != ECS_NULL_ENTITY &&
                ecs_is_alive(&game.ecs, local->entity)) {
                camera_follow(&game.camera, ecs_get_position(&game.ecs, local->entity)->pos, dt);
            }

            hud_update(&game.hud, dt);

            bool game_over = false;
            switch (game.mode) {
                case GAME_MODE_SINGLE:
                    game_over = !game.players[0].in_use || !game.players[0].alive;
                    break;
                case GAME_MODE_MULTI_HARDCORE: {
                    /* Game ends only when every resident player is out. */
                    int resident = 0;
                    for (int s = 0; s < MAX_PLAYERS; s++) {
                        if (game.players[s].in_use) resident++;
                    }
                    game_over = resident > 0 && alive_count == 0;
                    break;
                }
                case GAME_MODE_MULTI_TDM:
                default:
                    /* TDM always respawns; the match ends when the host stops
                     * it (hosting/leave handling is P4). */
                    game_over = false;
                    break;
            }
            if (game_over) {
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
            GameState next = shop_menu_update(&game.shop_menu, &game.input, &game.players[0].inventory);
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

static void render_connecting(void) {
    SDL_SetRenderDrawColorFloat(game.renderer, 0.05f, 0.05f, 0.08f, 1.0f);
    SDL_RenderClear(game.renderer);

    int win_w, win_h;
    SDL_GetWindowSize(game.window, &win_w, &win_h);
    float cx = win_w * 0.5f;

    SDL_FColor title = {1.0f, 0.85f, 0.2f, 1.0f};
    menu_draw_text_centered(game.renderer, game.font_large, "CONNECTING",
                            cx, win_h * 0.35f, title);

    SDL_FColor sub = {0.6f, 0.65f, 0.7f, 0.9f};
    menu_draw_text_centered(game.renderer, game.font,
                            game.net_client.status, cx, win_h * 0.45f, sub);

    SDL_FColor hint = {0.35f, 0.35f, 0.4f, 0.7f};
    menu_draw_text_centered(game.renderer, game.font,
                            "ESC: Cancel", cx, win_h * 0.7f, hint);
}

static void render_lobby(void) {
    SDL_SetRenderDrawColorFloat(game.renderer, 0.05f, 0.05f, 0.08f, 1.0f);
    SDL_RenderClear(game.renderer);

    int win_w, win_h;
    SDL_GetWindowSize(game.window, &win_w, &win_h);
    float cx = win_w * 0.5f;

    SDL_FColor title_color = {1.0f, 0.85f, 0.2f, 1.0f};
    menu_draw_text_centered(game.renderer, game.font_large,
                            game.net_host_mode ? "LOBBY - HOSTING" : "LOBBY",
                            cx, win_h * 0.1f, title_color);

    char buf[192];
    SDL_FColor sub = {0.6f, 0.65f, 0.7f, 0.9f};

    if (game.net_host_mode) {
        snprintf(buf, sizeof(buf), "Mode: %s   |   Port: %u   |   Seed: %u",
                 game_mode_name(game.mode),
                 (unsigned)game.net_server.port, game.net_server.seed);
    } else {
        snprintf(buf, sizeof(buf),
                 "Mode: %s   |   Host: %s   |   Seed: %u",
                 game_mode_name(game.mode), game.net_client.host_name,
                 game.net_client.seed);
    }
    menu_draw_text_centered(game.renderer, game.font, buf, cx, win_h * 0.2f, sub);

    /* Roster rows: slot, color swatch (as text), name, rtt. */
    char row[192];
    float row_y = win_h * 0.32f;
    const float row_h = 32.0f;

    if (game.net_host_mode) {
        NetPlayerInfo roster[NET_MAX_PLAYERS];
        int n = net_server_build_player_list(&game.net_server, roster,
                                             NET_MAX_PLAYERS);
        for (int i = 0; i < n; i++) {
            SDL_FColor col = net_slot_color(roster[i].slot);
            char rtt[24];
            ENetPeer *peer = game.net_server.slot_peers[roster[i].slot];
            if (peer) {
                snprintf(rtt, sizeof(rtt), "%d ms",
                         (int)peer->roundTripTime);
            } else {
                snprintf(rtt, sizeof(rtt), "(local)");
            }
            snprintf(row, sizeof(row), "%u. %s", roster[i].slot,
                     roster[i].name);
            menu_draw_text_centered(game.renderer, game.font, row,
                                    cx - 60.0f, row_y, col);
            menu_draw_text_centered(game.renderer, game.font, rtt,
                                    cx + 120.0f, row_y, sub);
            row_y += row_h;
        }
    } else {
        for (int i = 0; i < game.net_client.roster_count; i++) {
            const NetPlayerInfo *p = &game.net_client.roster[i];
            SDL_FColor col = net_slot_color(p->slot);
            snprintf(row, sizeof(row), "%u. %s%s", p->slot, p->name,
                     p->slot == game.net_client.slot ? "  (you)" : "");
            menu_draw_text_centered(game.renderer, game.font, row,
                                    cx - 60.0f, row_y, col);
            row_y += row_h;
        }
        int rtt = net_client_rtt_ms(&game.net_client);
        if (rtt >= 0) {
            snprintf(row, sizeof(row), "Ping to host: %d ms", rtt);
            menu_draw_text_centered(game.renderer, game.font, row,
                                    cx, row_y + row_h * 0.5f, sub);
        }
    }

    SDL_FColor hint = {0.35f, 0.35f, 0.4f, 0.7f};
    if (game.net_host_mode) {
        menu_draw_text_centered(game.renderer, game.font,
                                "ENTER: Start match  |  ESC: Leave lobby",
                                cx, win_h * 0.85f, hint);
    } else {
        menu_draw_text_centered(game.renderer, game.font,
                                "Waiting for host to start the match  |  ESC: Leave",
                                cx, win_h * 0.85f, hint);
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

        case GAME_STATE_CONNECTING:
            render_connecting();
            break;

        case GAME_STATE_LOBBY:
            render_lobby();
            break;

        case GAME_STATE_PLAYING:
        case GAME_STATE_PAUSED:
        case GAME_STATE_SHOP:
            world_draw(game.renderer, &game.world, &game.camera);
            if (render_only_client()) {
                /* Render-only client: draw the interpolated snapshot mirror. */
                system_render_mirror(game.renderer, &game.camera,
                                     &game.net_mirror, (float)game.elapsed_sim);
            } else {
                system_render(&game.ecs, game.renderer, &game.camera);
                if (game_mode_is_multi(game.mode)) {
                    system_render_beacons(game.renderer, &game.camera,
                                          game.players, MAX_PLAYERS);
                }
            }
            hud_draw(game.renderer, &game.hud, &game.ecs, &game.waves,
                     &game.players[0], !render_only_client(),
                     win_w, win_h, game.font);

            if (game.state == GAME_STATE_PAUSED) {
                pause_menu_draw(game.renderer, &game.pause_menu, win_w, win_h, game.font_large);
            } else if (game.state == GAME_STATE_SHOP) {
                shop_menu_draw(game.renderer, &game.shop_menu, &game.players[0].inventory, win_w, win_h, game.font_large);
            }
            break;

        case GAME_STATE_GAME_OVER:
            gameover_draw(game.renderer, &game.gameover_screen, win_w, win_h, game.font_large);
            break;
    }

    SDL_RenderPresent(game.renderer);
}

/* Host: fold each remote player's latest wire input into their slot's
 * InputState before the systems run, so remote players ride the exact same
 * input-injection path as bots (net input -> InputState -> systems). */
static void apply_net_inputs(void) {
    if (!game.net_host_mode) return;
    for (int s = 1; s < MAX_PLAYERS; s++) {
        if (!game.players[s].in_use) continue;
        NetInput in;
        if (!net_server_get_input(&game.net_server, s, &in)) continue;

        Player *p = &game.players[s];
        memset(p->input.keys, 0, sizeof(p->input.keys));
        memset(p->input.mouse_buttons, 0, sizeof(p->input.mouse_buttons));
        if (in.move_flags & NET_INPUT_MOVE_UP) p->input.keys[SDL_SCANCODE_W] = true;
        if (in.move_flags & NET_INPUT_MOVE_DOWN) p->input.keys[SDL_SCANCODE_S] = true;
        if (in.move_flags & NET_INPUT_MOVE_LEFT) p->input.keys[SDL_SCANCODE_A] = true;
        if (in.move_flags & NET_INPUT_MOVE_RIGHT) p->input.keys[SDL_SCANCODE_D] = true;
        p->input.mouse_world_x = in.aim_x;
        p->input.mouse_world_y = in.aim_y;
        p->input.world_aim = true;
        if (in.buttons & NET_INPUT_BTN_SHOOT) p->input.mouse_buttons[0] = true;

        if (in.weapon >= WEAPON_PISTOL && (int)in.weapon <= WEAPON_LAUNCHER &&
            p->inventory.current != (WeaponType)in.weapon) {
            weapons_select(&p->inventory, (WeaponType)in.weapon);
        }
    }
}

/* Client: package the local raw input into a NetInput and push it to the host
 * at >= 30 Hz (rate-limited here; the transport is unreliable latest-wins). */
static void send_net_input(float dt) {
    if (game.net_host_mode || game.net_client.state != NET_CLIENT_CONNECTED) {
        game.net_input_tx_accum = 0;
        return;
    }
    game.net_input_tx_accum += dt;
    if (game.net_input_tx_accum < (1.0f / 30.0f)) return;
    game.net_input_tx_accum = 0;

    const InputState *i = &game.input;
    NetInput in;
    memset(&in, 0, sizeof(in));
    if (input_key_held(i, SDL_SCANCODE_W) || input_key_held(i, SDL_SCANCODE_UP))
        in.move_flags |= NET_INPUT_MOVE_UP;
    if (input_key_held(i, SDL_SCANCODE_S) || input_key_held(i, SDL_SCANCODE_DOWN))
        in.move_flags |= NET_INPUT_MOVE_DOWN;
    if (input_key_held(i, SDL_SCANCODE_A) || input_key_held(i, SDL_SCANCODE_LEFT))
        in.move_flags |= NET_INPUT_MOVE_LEFT;
    if (input_key_held(i, SDL_SCANCODE_D) || input_key_held(i, SDL_SCANCODE_RIGHT))
        in.move_flags |= NET_INPUT_MOVE_RIGHT;
    if (i->mouse_buttons[0]) in.buttons |= NET_INPUT_BTN_SHOOT;
    in.weapon = game.net_weapon;
    Vec2 aim = camera_screen_to_world(&game.camera, vec2(i->mouse_x, i->mouse_y));
    in.aim_x = aim.x;
    in.aim_y = aim.y;
    net_client_send_input(&game.net_client, &in);
}

/* True when this process is a render-only net client (joins the host's sim,
 * never runs one itself). */
static bool render_only_client(void) {
    return !game.net_host_mode && game.net_client.host &&
           game.net_client.state == NET_CLIENT_CONNECTED;
}

/* Client: drain the newest received snapshot into the interpolation mirror
 * exactly once (by sequence), so frames between 20 Hz snapshots blend. */
static void drain_mirror(void) {
    if (game.net_host_mode) return;
    if (game.net_client.state != NET_CLIENT_CONNECTED) return;
    if (!game.net_client.snap_valid) return;
    if (game.net_client.snap_seq == game.net_client.last_mirror_seq) return;
    game.net_client.last_mirror_seq = game.net_client.snap_seq;
    net_mirror_push(&game.net_mirror, &game.net_client.snap);
    /* Headless verification: prove replication cheaply (~1 log/s). */
    if (game.headless && (game.net_client.snap_seq % NET_SNAP_HZ) == 0) {
        LOG_INFO("CLIENT mirror seq=%u host_sim=%.2f wave=%u ents=%d",
                 (unsigned)game.net_client.snap_seq,
                 game.net_mirror.newer.sim_time,
                 (unsigned)game.net_mirror.newer.wave_number,
                 game.net_mirror.newer.count);
    }
}

/* Host: broadcast a 20 Hz world snapshot to every remote client. Called once
 * per fixed sim step (after the sim update) so the wire sim_time stays exact. */
static void broadcast_snapshots(float dt) {
    if (!game.net_host_mode) {
        game.net_snap_accum = 0;
        return;
    }
    game.net_snap_accum += dt;
    if (game.net_snap_accum < (1.0f / NET_SNAP_HZ)) return;
    game.net_snap_accum = 0;
    NetSnapshot snap;
    int n = net_snapshot_build(&game.ecs, game.players, MAX_PLAYERS,
                               (float)game.elapsed_sim, &game.waves, &snap);
    if (n > 0) net_server_broadcast_snapshot(&game.net_server, &snap);
}

/* Host: relay a curated subset of gameplay events (§6.3) every frame on ch0
 * (reliable/ordered). Entity deaths let clients tidy the mirror ahead of the
 * next snapshot; wave starts feed the client HUD. Runs BEFORE the event bus
 * flush drains the ring. */
static void relay_net_events(void) {
    if (!game.net_host_mode || !g_events) return;
    static Uint64 last_relay_sid = 0;
    NetRelayedEvent batch[NET_EVENTS_MAX_BATCH];
    int n = 0;
    int count = g_events->count;
    for (int i = 0; i < count && n < NET_EVENTS_MAX_BATCH; i++) {
        const GameEvent *ev = &g_events->ring[(g_events->head + i) % EV_MAX_EVENTS];
        if (ev->sid <= last_relay_sid) continue;
        if (ev->sid > last_relay_sid) last_relay_sid = ev->sid;
        /* R13-D1: GE_POINTS/GE_DAMAGE/GE_ITEM_PICKUP must cross the wire too,
         * or a joined client can never evidence scoring/credit in its log. */
        if (ev->type == GE_ENTITY_DEATH || ev->type == GE_WAVE_START ||
            ev->type == GE_PLAYER_HEALTH || ev->type == GE_KILL ||
            ev->type == GE_POINTS || ev->type == GE_DAMAGE ||
            ev->type == GE_ITEM_PICKUP) {
            batch[n++] = (NetRelayedEvent){
                (uint8_t)ev->type, (uint8_t)ev->kind,
                (uint16_t)ev->entity,
                ev->x, ev->y, ev->a, ev->b
            };
        }
    }
    if (n > 0) {
        net_server_broadcast_events(&game.net_server, batch, n);
    }
}

/* Client: pull the pending death list + wave flag into the mirror/HUD, then
 * reset the transient queues for the next frame's drain. */
static void drain_net_events(void) {
    NetClient *c = &game.net_client;
    if (game.net_host_mode) return;
    if (c->state != NET_CLIENT_CONNECTED) return;

    if (c->dead_count > 0) {
        net_mirror_push_removing(&game.net_mirror, NULL, c->dead_ids,
                                 c->dead_count);
        c->dead_count = 0;
    }
    if (c->has_pending_wave) {
        char msg[64];
        snprintf(msg, sizeof(msg), "Wave %d - %d zombies incoming!",
                 c->pending_wave, c->pending_wave_count);
        hud_show_message(&game.hud, msg, 3.0f);
        c->has_pending_wave = false;
    }
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

    /* Mirror the local raw input into the local player's slot so the systems
     * read through the per-player table (single-player == slot 0). */
    if (game.players[0].in_use) {
        game.players[0].input = game.input;
    }

    /* Fold remote players' wire input into their slots (host), and stream the
     * local input up (client). */
    apply_net_inputs();
    send_net_input(dt);

    update(dt);
    drain_mirror();
    drain_net_events();
    broadcast_snapshots(dt);

    if (g_events) {
        event_bus_tick(g_events, dt);
        if (game.state == GAME_STATE_PLAYING) {
            event_bus_samples(g_events, &game.ecs, 0, 0);
        }
        /* Relay to remote clients BEFORE the flush drains the ring. */
        relay_net_events();
        event_bus_flush(g_events);
    }

    input_update(&game.input);
}

/* Built before the driver updates so it sees last frame's positions. */
static void update_ai_view(float dt) {
    if (game.ai.mode == AI_MODE_NONE) return;

    ai_build_view(&game.ai_view, &game.ecs, &game.waves, game.players[0].entity);
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
        } else if (strncmp(arg, "--mode=", 7) == 0) {
            const char *m = arg + 7;
            if (strcmp(m, "single") == 0) {
                game.mode = GAME_MODE_SINGLE;
            } else if (strcmp(m, "multi-tdm") == 0) {
                game.mode = GAME_MODE_MULTI_TDM;
            } else if (strcmp(m, "multi-hardcore") == 0) {
                game.mode = GAME_MODE_MULTI_HARDCORE;
            } else {
                LOG_WARN("Unknown --mode '%s' (expected single|multi-tdm|multi-hardcore)", m);
            }
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
        } else if (strcmp(arg, "--host") == 0) {
            game.net_host_mode = true;
        } else if (strcmp(arg, "--auto-start") == 0) {
            game.auto_start = true;
        } else if (strncmp(arg, "--port=", 7) == 0) {
            long p = strtol(arg + 7, NULL, 10);
            if (p <= 0 || p > 65535) {
                LOG_WARN("Bad --port value '%s'; using default %u",
                         arg + 7, (unsigned)NET_DEFAULT_PORT);
                game.net_port = NET_DEFAULT_PORT;
            } else {
                game.net_port = (uint16_t)p;
            }
        } else if (strcmp(arg, "--join") == 0 && i + 1 < argc) {
            snprintf(game.net_join_addr, sizeof(game.net_join_addr), "%s",
                     argv[++i]);
            game.net_host_mode = false;
        } else if (strncmp(arg, "--join=", 7) == 0) {
            snprintf(game.net_join_addr, sizeof(game.net_join_addr), "%s",
                     arg + 7);
            game.net_host_mode = false;
        } else if (strncmp(arg, "--name=", 7) == 0) {
            snprintf(game.net_player_name, sizeof(game.net_player_name), "%s",
                     arg + 7);
        } else if (strcmp(arg, "--help") == 0) {
            printf("%s\n", "Usage: open_world_zombie_waves [options]");
            printf("%s\n", "  --headless            run with no window/renderer (fast, deterministic)");
            printf("%s\n", "  --ai=none|script|bot  AI control mode (default none)");
            printf("%s\n", "  --script <file>       scripted playback (also --script-loop=N)");
            printf("%s\n", "  --events=<file>       gameplay event log path (default game_events.log)");
            printf("%s\n", "  --seed=<n>            deterministic RNG seed");
            printf("%s\n", "  --mode=<m>            single | multi-tdm | multi-hardcore (default single)");
            printf("%s\n", "  --run-seconds=<s>     auto-exit after s simulated seconds");
            printf("%s\n", "  --player-hp=<pct>     start player at pct%% HP (0-100; debug/playtest)");
            printf("%s\n", "  --points=<n>          start with n shop points (debug/playtest)");
            printf("%s\n", "  --player-damage-mult=<f>  zombie melee damage multiplier (debug/playtest)");
            printf("%s\n", "  --zombie-speed-mult=<f>   zombie move-speed multiplier (debug/playtest)");
            printf("%s\n", "  --host                    host a LAN lobby (multiplayer)");
            printf("%s\n", "  --join <ip>[:port]        join a hosted lobby (multiplayer)");
            printf("%s\n", "  --port=<n>                host listen port (default 5123)");
            printf("%s\n", "  --name=<name>             player name when hosting/joining");
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
    LOG_INFO("Game mode: %s", game_mode_name(game.mode));

    if (!game.event_log_path) game.event_log_path = "game_events.log";
    event_bus_init(game.event_log_path);

    if (!init()) {
        LOG_FATAL("Initialization failed!");
        return 1;
    }

    /* Kick off any requested multiplayer session (--host/--join). Falls back to
     * the main menu when neither flag is present. */
    if (!begin_net_session()) {
        LOG_FATAL("Could not start the multiplayer session");
        shutdown_game();
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

    shutdown_game();
    return 0;
}