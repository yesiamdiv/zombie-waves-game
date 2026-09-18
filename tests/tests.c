#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "core/log.h"
#include "ecs/ecs.h"
#include "world/world.h"
#include "world/waves.h"
#include "systems/systems.h"
#include "items/items.h"
#include "events/event_bus.h"
#include "ai/ai_driver.h"
#include "ai/ai_types.h"
#include "config.h"
#include "net/net.h"
#include "net/net_codec.h"

/* Normal-play default; the game binary overrides via debug flags. */
float g_zombie_damage_mult = 1.0f;
float g_zombie_speed_mult = 1.0f;

static int tests_passed = 0;
static int tests_failed = 0;

#define CHECK(expr)                                                      \
    do {                                                                 \
        if (expr) {                                                      \
            tests_passed++;                                              \
            LOG_INFO("PASS: %s (%s:%d)", #expr, __FILE__, __LINE__);     \
        } else {                                                         \
            tests_failed++;                                              \
            LOG_ERROR("FAIL: %s (%s:%d)", #expr, __FILE__, __LINE__);    \
        }                                                                \
    } while (0)

static Entity spawn_player(World *ecs, Vec2 pos) {
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
    return e;
}

static Entity spawn_test_bullet(World *ecs, Vec2 pos, Entity owner) {
    Entity e = ecs_create_entity(ecs);
    if (e == ECS_NULL_ENTITY) return e;

    ecs_add_component(ecs, e, COMP_POSITION);
    ecs_add_component(ecs, e, COMP_VELOCITY);
    ecs_add_component(ecs, e, COMP_SPRITE);
    ecs_add_component(ecs, e, COMP_COLLIDER);
    ecs_add_component(ecs, e, COMP_BULLET_TAG);

    *ecs_get_position(ecs, e) = (CPosition){{pos.x, pos.y}};
    *ecs_get_velocity(ecs, e) = (CVelocity){{0, 0}, 400.0f};
    *ecs_get_collider(ecs, e) = (CCollider){4.0f, true};
    *ecs_get_bullet_tag(ecs, e) = (CBulletTag){
        .lifetime = 2.0f, .max_lifetime = 2.0f, .owner = owner};
    *ecs_get_sprite(ecs, e) = (CSprite){
        .sprite = sprite_circle(4.0f, COLOR_YELLOW),
        .scale = 1.0f, .base_alpha = 1.0f};
    return e;
}

/* Build a lone in-use slot around an already-spawned entity for direct
 * per-player system calls (weapon tests). */
static void player_slot_wrap(Player *p, Entity e) {
    memset(p, 0, sizeof(Player));
    p->in_use = true;
    p->alive = true;
    p->entity = e;
    input_init(&p->input);
    weapons_inventory_init(&p->inventory);
}

static void test_kill_credit(void) {
    LOG_INFO("--- Test: Per-player kill credit via damage owner ---");
    World ecs;
    ecs_init(&ecs);

    Player players[MAX_PLAYERS];
    players_reset(players, MAX_PLAYERS);
    player_respawn(players, &ecs, 0, "P1", &COLOR_RED, vec2(400, 400));
    player_respawn(players, &ecs, 1, "P2", &COLOR_BLUE, vec2(600, 400));
    CHECK(players[0].entity != ECS_NULL_ENTITY);
    CHECK(players[1].entity != ECS_NULL_ENTITY);
    CHECK(players[0].inventory.points == 0);
    CHECK(players[0].kills == 0);

    /* Zombie standing between the two players; P1 (slot 0) shoots it dead. */
    Entity z = waves_spawn_zombie(&ecs, vec2(500, 400));
    CHECK(z != ECS_NULL_ENTITY);
    int safety = 0;
    while (ecs_is_alive(&ecs, z) && ecs_get_health(&ecs, z)->current > 0 && safety < 20) {
        Entity b = spawn_test_bullet(&ecs, ecs_get_position(&ecs, z)->pos,
                                     players[0].entity);
        CHECK(b != ECS_NULL_ENTITY);
        system_collision(&ecs, NULL);
        safety++;
    }
    CHECK(ecs_get_health(&ecs, z)->current <= 0);
    CHECK(safety < 20);
    CHECK(ecs_get_zombie_tag(&ecs, z)->last_hit_by == players[0].entity);

    /* Cleanup with the slot table: the kill must land in P1's slot. */
    system_cleanup(&ecs, NULL, players, MAX_PLAYERS);
    CHECK(players[0].kills == 1);
    CHECK(players[0].inventory.points == POINTS_PER_KILL);
    CHECK(players[1].kills == 0);
    CHECK(players[1].inventory.points == 0);
}

static void test_nearest_alive_target(void) {
    LOG_INFO("--- Test: zombies target nearest alive player, retarget on death ---");
    World ecs;
    ecs_init(&ecs);

    Player players[MAX_PLAYERS];
    players_reset(players, MAX_PLAYERS);
    player_respawn(players, &ecs, 0, "P1", &COLOR_RED, vec2(700, 500));
    player_respawn(players, &ecs, 1, "P2", &COLOR_BLUE, vec2(500, 500));
    CHECK(players[0].entity != ECS_NULL_ENTITY);
    CHECK(players[1].entity != ECS_NULL_ENTITY);

    Entity z = waves_spawn_zombie(&ecs, vec2(500, 600));
    CHECK(z != ECS_NULL_ENTITY);
    CHECK(ecs_get_zombie_tag(&ecs, z)->state == ZOMBIE_CHASE);

    /* P2 (500,500) is 100px away, P1 (700,500) is ~224px: nearest = P2, which
     * is straight up from the zombie. It must NOT pick the first player in the
     * table (slot 0 = P1). */
    system_zombie_ai(&ecs, players, MAX_PLAYERS, 1.0f / 60.0f);
    CVelocity *v = ecs_get_velocity(&ecs, z);
    CHECK(v->vel.y < 0.0f);
    CHECK(v->vel.x > -0.001f && v->vel.x < 0.001f);

    /* P2 dies: the zombie must retarget the only survivor, P1 (north-east). */
    players[1].alive = false;
    system_zombie_ai(&ecs, players, MAX_PLAYERS, 1.0f / 60.0f);
    CHECK(ecs_get_velocity(&ecs, z)->vel.x > 0.0f);
    CHECK(ecs_get_velocity(&ecs, z)->vel.y < 0.0f);
}

static void test_beacon_anchor(void) {
    LOG_INFO("--- Test: spawn beacon anchor stored per slot (P0.5) ---");
    World ecs;
    ecs_init(&ecs);

    Player players[MAX_PLAYERS];
    players_reset(players, MAX_PLAYERS);

    Vec2 spawn = vec2(333.0f, 777.0f);
    int slot = player_respawn(players, &ecs, 0, "P1", &COLOR_RED, spawn);
    CHECK(slot == 0);
    CHECK(players[0].beacon_pos.x == spawn.x);
    CHECK(players[0].beacon_pos.y == spawn.y);
    CHECK(players[0].color.r == COLOR_RED.r);
}

/* Kill every player slot currently alive. */
static void kill_player_slots(World *ecs, Player *players, int player_count) {
    for (int s = 0; s < player_count; s++) {
        if (!players[s].in_use || !players[s].alive) continue;
        if (players[s].entity == ECS_NULL_ENTITY) continue;
        if (ecs_is_alive(ecs, players[s].entity)) {
            ecs_get_health(ecs, players[s].entity)->current = 0.0f;
        }
    }
}

static void test_tdm_respawn(void) {
    LOG_INFO("--- Test: TDM respawns at own beacon after MULTI_RESPAWN_TIME ---");
    World ecs;
    ecs_init(&ecs);

    Player players[MAX_PLAYERS];
    players_reset(players, MAX_PLAYERS);
    player_respawn(players, &ecs, 0, "P1", &COLOR_RED, vec2(300, 300));
    player_respawn(players, &ecs, 1, "P2", &COLOR_BLUE, vec2(900, 700));
    Entity e0 = players[0].entity;
    Entity e1 = players[1].entity;

    /* Kill both. */
    kill_player_slots(&ecs, players, MAX_PLAYERS);
    system_cleanup(&ecs, NULL, players, MAX_PLAYERS);
    CHECK(!ecs_is_alive(&ecs, e0));
    CHECK(!ecs_is_alive(&ecs, e1));

    /* First rule tick: both slots marked dead, TDM timers armed. */
    int alive = players_match_update(&ecs, players, MAX_PLAYERS,
                                     GAME_MODE_MULTI_TDM, 0.0f);
    CHECK(alive == 0);
    CHECK(!players[0].alive && !players[1].alive);
    CHECK(players[0].respawn_timer == MULTI_RESPAWN_TIME);
    CHECK(players[1].respawn_timer == MULTI_RESPAWN_TIME);
    CHECK(!players[0].eliminated);

    /* Gifts survive death (inventory KEPT across respawn). */
    players[0].inventory.points = 100;
    players[1].kills = 3;

    /* Tick nearly the whole timer on both; still dead. */
    players_match_update(&ecs, players, MAX_PLAYERS, GAME_MODE_MULTI_TDM,
                         MULTI_RESPAWN_TIME - 0.5f);
    CHECK(!players[0].alive);
    CHECK(players[0].respawn_timer > 0.0f);

    /* Finish the countdown: both respawn at their own beacons. */
    alive = players_match_update(&ecs, players, MAX_PLAYERS, GAME_MODE_MULTI_TDM,
                                 0.5f);
    CHECK(alive == 2);
    CHECK(players[0].alive && players[1].alive);
    /* The ECS reuses freed entity indices, so the new entity may be the same
     * numeric id but must be a FRESH, live entity at the beacon. */
    CHECK(ecs_is_alive(&ecs, players[0].entity));
    CHECK(ecs_is_alive(&ecs, players[1].entity));
    CHECK(players[0].entity != ECS_NULL_ENTITY);
    CHECK(players[1].entity != ECS_NULL_ENTITY);
    CHECK(ecs_get_position(&ecs, players[0].entity)->pos.x == 300.0f);
    CHECK(ecs_get_position(&ecs, players[1].entity)->pos.x == 900.0f);
    CHECK(players[0].respawn_timer == 0.0f);

    /* Persistent slot data was NOT wiped by the respawn. */
    CHECK(players[0].inventory.points == 100);
    CHECK(players[1].kills == 3);
    CHECK(strcmp(players[0].name, "P1") == 0);
    CHECK(players[0].color.r == COLOR_RED.r);
}

static void test_hardcore_elimination(void) {
    LOG_INFO("--- Test: HARDCORE eliminates permanently, game-over at 0 alive ---");
    World ecs;
    ecs_init(&ecs);

    Player players[MAX_PLAYERS];
    players_reset(players, MAX_PLAYERS);
    player_respawn(players, &ecs, 0, "P1", &COLOR_RED, vec2(300, 300));
    player_respawn(players, &ecs, 1, "P2", &COLOR_BLUE, vec2(900, 700));

    /* One player dies: the other keeps the match alive. */
    ecs_get_health(&ecs, players[0].entity)->current = 0.0f;
    system_cleanup(&ecs, NULL, players, MAX_PLAYERS);
    int alive = players_match_update(&ecs, players, MAX_PLAYERS,
                                     GAME_MODE_MULTI_HARDCORE, 0.0f);
    CHECK(alive == 1);
    CHECK(!players[0].alive);
    CHECK(players[0].eliminated);
    CHECK(players[0].respawn_timer == 0.0f);   /* no TDM timer in hardcore */
    CHECK(players[1].alive && !players[1].eliminated);

    /* Passing time must NOT un-eliminate or respawn a hardcore player. */
    alive = players_match_update(&ecs, players, MAX_PLAYERS,
                                 GAME_MODE_MULTI_HARDCORE, 999.0f);
    CHECK(alive == 1);
    CHECK(!players[0].alive);
    CHECK(players[0].eliminated);

    /* Everyone out -> 0 alive (host then ends the match). */
    ecs_get_health(&ecs, players[1].entity)->current = 0.0f;
    system_cleanup(&ecs, NULL, players, MAX_PLAYERS);
    alive = players_match_update(&ecs, players, MAX_PLAYERS,
                                 GAME_MODE_MULTI_HARDCORE, 0.0f);
    CHECK(alive == 0);
    CHECK(players[0].eliminated && players[1].eliminated);
}

static void test_ecs_basics(void) {
    LOG_INFO("--- Test: ECS basics ---");
    World ecs;
    ecs_init(&ecs);

    Entity player = ecs_create_entity(&ecs);
    CHECK(player != ECS_NULL_ENTITY);
    CHECK(ecs_is_alive(&ecs, player));

    ecs_add_component(&ecs, player, COMP_POSITION);
    CHECK(ecs_has_component(&ecs, player, COMP_POSITION));
    CHECK(!ecs_has_component(&ecs, player, COMP_VELOCITY));

    ecs_destroy_entity(&ecs, player);
    CHECK(!ecs_is_alive(&ecs, player));
    CHECK(ecs.alive_count == 0);
}

static void test_world_valid(void) {
    LOG_INFO("--- Test: World generation ---");
    GameWorld world;
    world_init(&world);

    CHECK(world.width == WORLD_TILES_X);
    CHECK(world.height == WORLD_TILES_Y);

    int walkable = 0;
    for (int y = 1; y < world.height - 1; y++) {
        for (int x = 1; x < world.width - 1; x++) {
            if (world_is_walkable(&world, x * WORLD_GRID_SIZE + 32,
                                           y * WORLD_GRID_SIZE + 32)) {
                walkable++;
            }
        }
    }
    CHECK(walkable > 0);

    CHECK(world_get_tile(&world, -1, -1) == TILE_WALL);
    CHECK(world_get_tile(&world, 999, 999) == TILE_WALL);

    Vec2 spawn = world_get_spawn_point(&world);
    CHECK(world_is_walkable(&world, spawn.x, spawn.y));
}

static void test_world_determinism(void) {
    LOG_INFO("--- Test: World gen determinism (multiplayer gate) ---");
    GameWorld a, b;
    world_init(&a);
    world_init(&b);

    CHECK(a.width == b.width && a.height == b.height);
    CHECK(a.world_pixel_w == b.world_pixel_w);
    CHECK(a.world_pixel_h == b.world_pixel_h);

    bool same_tiles = true;
    for (int y = 0; y < a.height; y++) {
        for (int x = 0; x < a.width; x++) {
            if (a.tiles[y][x] != b.tiles[y][x]) {
                same_tiles = false;
                break;
            }
        }
        if (!same_tiles) break;
    }
    CHECK(same_tiles);

    Vec2 sa = world_get_spawn_point(&a);
    Vec2 sb = world_get_spawn_point(&b);
    CHECK(sa.x == sb.x && sa.y == sb.y);
}

static void test_wave_system(void) {
    LOG_INFO("--- Test: Wave spawning ---");
    World ecs;
    GameWorld world;
    WaveSystem waves;

    ecs_init(&ecs);
    world_init(&world);
    waves_init(&waves, &world);

    Entity player = spawn_player(&ecs, vec2(world.world_pixel_w * 0.5f,
                                            world.world_pixel_h * 0.5f));
    CHECK(player != ECS_NULL_ENTITY);

    /* Simulate 5 seconds - wave 1 should start after 3s cooldown */
    float dt = 1.0f / 60.0f;
    for (int i = 0; i < 300; i++) {
        waves_update(&waves, &ecs, &world, NULL, 0, dt);
        system_zombie_ai(&ecs, NULL, 0, dt);
        system_movement(&ecs, &world, dt);
        system_collision(&ecs, &world);
        system_cleanup(&ecs, &waves, NULL, 0);
    }

    CHECK(waves.wave_number >= 1);
    CHECK(waves.zombies_spawned > 0);

    /* Spawn a zombie directly and test bullet damage */
    Entity zombie = waves_spawn_zombie(&ecs, vec2(500, 500));
    CHECK(zombie != ECS_NULL_ENTITY);

    /* Zombies must sense the player from anywhere in the 400-600 spawn ring,
     * otherwise the last zombie can soft-lock the wave (playtest B1). */
    CHECK(ecs_get_zombie_tag(&ecs, zombie)->detection_range >= 600.0f);
    float health_before = ecs_get_health(&ecs, zombie)->current;

    Entity bullet = spawn_test_bullet(&ecs, vec2(500, 500), player);
    CHECK(bullet != ECS_NULL_ENTITY);

    /* Move bullet so it stays overlapping the zombie, then collide */
    system_collision(&ecs, &world);

    CHECK(ecs_get_health(&ecs, zombie)->current < health_before);
    CHECK(!ecs_is_alive(&ecs, bullet));

    /* Kill the zombie with repeated shots */
    int safety = 0;
    while (ecs_is_alive(&ecs, zombie) && safety < 20) {
        Entity b = spawn_test_bullet(&ecs, ecs_get_position(&ecs, zombie)->pos, player);
        CHECK(b != ECS_NULL_ENTITY);
        system_collision(&ecs, &world);
        system_cleanup(&ecs, &waves, NULL, 0);
        safety++;
    }
    CHECK(!ecs_is_alive(&ecs, zombie));
}

static void test_items(void) {
    LOG_INFO("--- Test: Items ---");
    const char *path = "/tmp/opencode/test_items_events.log";
    remove(path);

    EventBus *bus = event_bus_init(path);
    CHECK(bus != NULL);

    World ecs;
    ecs_init(&ecs);

    Entity player = spawn_player(&ecs, vec2(100, 100));
    CHECK(player != ECS_NULL_ENTITY);

    CHealth *hp = ecs_get_health(&ecs, player);
    hp->current = 100.0f;

    Entity medkit = items_spawn(&ecs, vec2(110, 100), ITEM_HEALTH);
    CHECK(medkit != ECS_NULL_ENTITY);
    CHECK(ecs_get_item_tag(&ecs, medkit)->type == ITEM_HEALTH);

    items_check_pickup(&ecs, player);
    CHECK(!ecs_is_alive(&ecs, medkit));
    CHECK(ecs_get_health(&ecs, player)->current > 100.0f);

    event_bus_flush(bus);

    /* Heal uptake must be visible in the event log (QA B5 nit): the pickup
     * emits both ITEM_PICKUP and PLAYER_HEALTH with a=hp after, b=max,
     * ia=hp before. */
    FILE *f = fopen(path, "r");
    CHECK(f != NULL);
    bool saw_pickup = false, saw_health = false;
    char health_line[512] = "";
    if (f) {
        char line[512];
        while (fgets(line, sizeof(line), f)) {
            if (strstr(line, "EVT=ITEM_PICKUP")) saw_pickup = true;
            if (strstr(line, "EVT=PLAYER_HEALTH")) {
                saw_health = true;
                strcpy(health_line, line);
            }
        }
        fclose(f);
    }
    CHECK(saw_pickup);
    CHECK(saw_health);
    CHECK(strstr(health_line, "a=130.00") != NULL);  /* 100 + 30 */
    CHECK(strstr(health_line, "b=200.00") != NULL);  /* max */
    CHECK(strstr(health_line, "ia=100") != NULL);    /* before */

    event_bus_shutdown(bus);
    remove(path);
}

static void test_entity_limit(void) {
    LOG_INFO("--- Test: Entity limit ---");
    World ecs;
    ecs_init(&ecs);

    int created = 0;
    for (int i = 0; i < ECS_MAX_ENTITIES + 10; i++) {
        Entity e = ecs_create_entity(&ecs);
        if (e != ECS_NULL_ENTITY) created++;
    }

    CHECK(created == ECS_MAX_ENTITIES);

    /* Destroy a few and re-create */
    ecs_destroy_entity(&ecs, 0);
    ecs_destroy_entity(&ecs, 1);
    Entity e = ecs_create_entity(&ecs);
    CHECK(e != ECS_NULL_ENTITY);
    CHECK(e == 0 || e == 1);
}

static void test_wave_completion(void) {
    LOG_INFO("--- Test: Wave completion ---");
    World ecs;
    GameWorld world;
    WaveSystem waves;

    ecs_init(&ecs);
    world_init(&world);
    waves_init(&waves, &world);

    Entity player = spawn_player(&ecs, vec2(world.world_pixel_w * 0.5f,
                                            world.world_pixel_h * 0.5f));
    CHECK(player != ECS_NULL_ENTITY);

    /* Force a small, fast wave so the test is quick and deterministic. */
    waves_start_next_wave(&waves);
    waves.zombies_to_spawn = 4;
    waves.spawn_interval = 0.05f;

    float dt = 1.0f / 120.0f;
    int guard = 0;
    while (guard < 3000 && waves.wave_number == 1 && !waves.between_waves) {
        guard++;

        waves_update(&waves, &ecs, &world, NULL, 0, dt);

        /* Shoot every alive zombie every frame until it dies. */
        for (uint32_t i = 0; i < ECS_MAX_ENTITIES; i++) {
            if (!ecs.alive[i]) continue;
            if (!(ecs.component_masks[i] & (1u << COMP_ZOMBIE_TAG))) continue;
            Entity b = spawn_test_bullet(&ecs, ecs_get_position(&ecs, i)->pos, player);
            if (b != ECS_NULL_ENTITY) {
                system_collision(&ecs, &world);
            }
        }

        system_cleanup(&ecs, &waves, NULL, 0);
    }

    CHECK(guard < 3000);          /* wave completed in time */
    CHECK(waves.between_waves);   /* moved to inter-wave state */
    CHECK(waves.zombies_spawned == 4);
    CHECK(waves.total_kills >= 4);
    CHECK(waves.wave_active == false);
}

static void test_script_aim_shot(void) {
    LOG_INFO("--- Test: Scripted AI aim/shoot ---");
    const char *path = "/tmp/opencode/script_test_shoot.script";
    FILE *f = fopen(path, "w");
    if (!f) {
        LOG_ERROR("Cannot create temp script file");
        CHECK(0);
        return;
    }
    fprintf(f, "@0.0 aim 500 400\n");
    fprintf(f, "@0.1 click 1 down\n");
    fprintf(f, "@0.5 click 1 up\n");
    fclose(f);

    AIDriver drv;
    ai_driver_init(&drv);
    CHECK(ai_driver_load_script(&drv, path) == 0);
    CHECK(drv.mode == AI_MODE_SCRIPT);

    GameView view;
    AIControls c;
    bool got_aim = false;
    bool got_shoot = false;
    for (int i = 0; i < 200 && !drv.done; i++) {
        ai_driver_update(&drv, 0.05, &view, &c);
        if (!got_aim && c.has_aim && c.aim_x == 500.0f && c.aim_y == 400.0f) got_aim = true;
        if (!got_shoot && c.shoot) got_shoot = true;
    }

    CHECK(got_aim);
    CHECK(got_shoot);
    CHECK(drv.done);

    /* Quit action propagates through the driver. */
    const char *quit_path = "/tmp/opencode/script_test_quit.script";
    f = fopen(quit_path, "w");
    if (f) {
        fprintf(f, "@0.0 quit\n");
        fclose(f);
    }
    ai_driver_init(&drv);
    CHECK(ai_driver_load_script(&drv, quit_path) == 0);
    ai_driver_update(&drv, 0.1, &view, &c);
    CHECK(drv.quit_requested);
    CHECK(c.shoot == false);

    remove(path);
    remove(quit_path);
}

static void test_wave_timeout(void) {
    LOG_INFO("--- Test: Wave timeout force-end ---");
    World ecs;
    GameWorld world;
    WaveSystem waves;

    ecs_init(&ecs);
    world_init(&world);
    waves_init(&waves, &world);

    Entity player = spawn_player(&ecs, vec2(world.world_pixel_w * 0.5f,
                                            world.world_pixel_h * 0.5f));
    CHECK(player != ECS_NULL_ENTITY);

    /* A single zombie that is never killed (no collision/cleanup run) must
     * not stall the wave forever: the 60s WAVE_MAX_DURATION safety net
     * force-ends it (playtest B1). */
    waves_start_next_wave(&waves);
    waves.zombies_to_spawn = 1;
    waves.spawn_interval = 0.01f;

    float dt = 1.0f / 120.0f;
    int frames_ran = 0;
    int max_frames = (int)(75.0f / dt);
    while (!waves.between_waves && frames_ran < max_frames) {
        waves_update(&waves, &ecs, &world, NULL, 0, dt);
        frames_ran++;
    }

    CHECK(waves.between_waves);
    CHECK(waves.wave_active == false);
    CHECK(waves.zombies_alive == 0);
    CHECK(waves.total_kills == 0);          /* nothing was actually killed */
    CHECK(frames_ran < max_frames);         /* ended before the hard cap */
    CHECK(waves.wave_elapsed > 59.0f);      /* took ~the 60s timeout */
}

static void test_event_stream(void) {
    LOG_INFO("--- Test: Gameplay event stream ---");
    const char *path = "/tmp/opencode/test_events.log";
    remove(path);

    EventBus *bus = event_bus_init(path);
    CHECK(bus != NULL);
    CHECK(g_events == bus);

    World ecs;
    GameWorld world;
    WaveSystem waves;
    ecs_init(&ecs);
    world_init(&world);
    waves_init(&waves, &world);

    Entity player = spawn_player(&ecs, vec2(world.world_pixel_w * 0.5f,
                                            world.world_pixel_h * 0.5f));
    CHECK(player != ECS_NULL_ENTITY);

    waves_start_next_wave(&waves);
    waves.zombies_to_spawn = 4;
    waves.spawn_interval = 0.05f;

    float dt = 1.0f / 120.0f;
    int guard = 0;
    while (guard < 3000 && waves.wave_number == 1 && !waves.between_waves) {
        guard++;
        event_bus_tick(bus, dt);

        waves_update(&waves, &ecs, &world, NULL, 0, dt);

        for (uint32_t i = 0; i < ECS_MAX_ENTITIES; i++) {
            if (!ecs.alive[i]) continue;
            if (!(ecs.component_masks[i] & (1u << COMP_ZOMBIE_TAG))) continue;
            Entity b = spawn_test_bullet(&ecs, ecs_get_position(&ecs, i)->pos, player);
            if (b != ECS_NULL_ENTITY) {
                system_collision(&ecs, &world);
            }
        }

        system_cleanup(&ecs, &waves, NULL, 0);
        event_bus_flush(bus);
    }

    CHECK(guard < 3000);
    CHECK(waves.between_waves);
    CHECK(waves.total_kills >= 4);

    event_bus_shutdown(bus);
    CHECK(g_events == NULL);

    /* Verify the log file contains the expected event stream. */
    FILE *f = fopen(path, "r");
    CHECK(f != NULL);
    bool saw_wave_start = false, saw_spawn = false, saw_kill = false, saw_wave_end = false;
    bool saw_sid = false;
    size_t total = 0;
    if (f) {
        char line[512];
        while (fgets(line, sizeof(line), f)) {
            total += strlen(line);
            if (strstr(line, "EVT=WAVE_START")) saw_wave_start = true;
            if (strstr(line, "EVT=ENTITY_SPAWN")) saw_spawn = true;
            if (strstr(line, "EVT=KILL")) saw_kill = true;
            if (strstr(line, "EVT=WAVE_END")) saw_wave_end = true;
            if (strstr(line, "sid=")) saw_sid = true;
        }
        fclose(f);
    }
    CHECK(total > 0);
    CHECK(saw_wave_start);
    CHECK(saw_spawn);
    CHECK(saw_kill);
    CHECK(saw_wave_end);
    CHECK(saw_sid);

    remove(path);
}

#define DET_MAX_SPAWNS 64

static int run_spawn_sim(Vec2 *out, int max_out) {
    srand(2026);
    World ecs;
    GameWorld world;
    WaveSystem waves;
    ecs_init(&ecs);
    world_init(&world);
    waves_init(&waves, &world);

    Entity player = spawn_player(&ecs, vec2(world.world_pixel_w * 0.5f,
                                            world.world_pixel_h * 0.5f));
    if (player == ECS_NULL_ENTITY) return -1;

    waves_start_next_wave(&waves);
    waves.zombies_to_spawn = DET_MAX_SPAWNS;
    waves.spawn_interval = 0.01f;

    float dt = 1.0f / 120.0f;
    int n = 0;
    for (int i = 0; i < 2000 && n < max_out; i++) {
        waves_update(&waves, &ecs, &world, NULL, 0, dt);
        for (uint32_t e = 0; e < ECS_MAX_ENTITIES && n < max_out; e++) {
            if (!ecs.alive[e]) continue;
            if (!(ecs.component_masks[e] & (1u << COMP_ZOMBIE_TAG))) continue;
            out[n++] = ecs.positions[e].pos;
            ecs_destroy_entity(&ecs, e);
        }
    }
    return n;
}

static void test_deterministic_seed(void) {
    LOG_INFO("--- Test: Deterministic seed ---");
    Vec2 a[DET_MAX_SPAWNS];
    Vec2 b[DET_MAX_SPAWNS];
    int na = run_spawn_sim(a, DET_MAX_SPAWNS);
    int nb = run_spawn_sim(b, DET_MAX_SPAWNS);

    CHECK(na > 0);
    CHECK(na == nb);

    bool same = true;
    for (int i = 0; i < na; i++) {
        if (a[i].x != b[i].x || a[i].y != b[i].y) same = false;
    }
    CHECK(same);
}

/* ---------------------------------------------------------------- Weapons */

static void test_weapons_inventory(void) {
    LOG_INFO("--- Test: Weapons inventory & shop ---");
    PlayerInventory inv;
    weapons_inventory_init(&inv);

    CHECK(inv.current == WEAPON_PISTOL);
    CHECK(inv.unlocked[WEAPON_PISTOL]);
    CHECK(!inv.unlocked[WEAPON_SWORD]);
    CHECK(inv.points == 0);
    CHECK(inv.grenades == 0);

    /* Cannot select a locked weapon. */
    CHECK(!weapons_select(&inv, WEAPON_SWORD));

    /* Award kill points. */
    weapons_award_kill(&inv, 4);          /* 4 × 15 = 60 */
    CHECK(inv.points == 60);

    /* Not enough for sword (150). */
    CHECK(!weapons_buy_sword(&inv));
    CHECK(!inv.unlocked[WEAPON_SWORD]);

    /* Fill up and buy the sword. */
    inv.points = 200;
    CHECK(weapons_buy_sword(&inv));
    CHECK(inv.points == 50);
    CHECK(inv.unlocked[WEAPON_SWORD]);
    CHECK(weapons_select(&inv, WEAPON_SWORD));
    CHECK(inv.current == WEAPON_SWORD);

    /* Can't buy sword twice. */
    CHECK(!weapons_buy_sword(&inv));

    /* Grenade pack (75 pts, +5). */
    CHECK(!weapons_buy_grenade_pack(&inv));   /* only 50 pts */
    inv.points = 80;
    CHECK(weapons_buy_grenade_pack(&inv));
    CHECK(inv.points == 5);
    CHECK(inv.grenades == 5);
    CHECK(inv.unlocked[WEAPON_GRENADE]);

    /* Launcher (450). Fill up. */
    inv.points = 440;
    CHECK(!weapons_buy_launcher(&inv));
    inv.points = 500;
    CHECK(weapons_buy_launcher(&inv));
    CHECK(inv.points == 50);
    CHECK(inv.unlocked[WEAPON_LAUNCHER]);
    CHECK(inv.launcher_ammo == LAUNCHER_STARTER_ROCKETS);

    /* Launcher ammo (150). */
    CHECK(!weapons_buy_launcher_ammo(&inv));
    inv.points = 160;
    CHECK(weapons_buy_launcher_ammo(&inv));
    CHECK(inv.points == 10);
    CHECK(inv.launcher_ammo == LAUNCHER_STARTER_ROCKETS + ROCKETS_PER_PACK);

    CHECK(strcmp(weapons_name(WEAPON_PISTOL), "Pistol") == 0);
    CHECK(strcmp(weapons_name(WEAPON_SWORD), "Sword") == 0);
    CHECK(strcmp(weapons_name(WEAPON_GRENADE), "Grenade") == 0);
    CHECK(strcmp(weapons_name(WEAPON_LAUNCHER), "Launcher") == 0);
}

static void test_sword_spin(void) {
    LOG_INFO("--- Test: Sword spin system ---");
    World ecs;
    ecs_init(&ecs);
    GameWorld world;
    world_init(&world);

    Vec2 spawn = {world.world_pixel_w * 0.5f, world.world_pixel_h * 0.5f};
    Entity player = spawn_player(&ecs, spawn);
    CHECK(player != ECS_NULL_ENTITY);

    Player p;
    player_slot_wrap(&p, player);
    p.inventory.points = 1000;
    weapons_buy_sword(&p.inventory);
    weapons_select(&p.inventory, WEAPON_SWORD);

    /* Fake input: mouse held, aim straight ahead. */
    p.input.mouse_buttons[0] = true;
    p.input.mouse_world_x = spawn.x + 100.0f;
    p.input.mouse_world_y = spawn.y;

    /* First tick: the system should spawn a sword entity. */
    system_sword(&ecs, &p, 1.0f / 60.0f);

    Entity sword_entity = ECS_NULL_ENTITY;
    for (uint32_t i = 0; i < ECS_MAX_ENTITIES; i++) {
        if (!ecs.alive[i]) continue;
        if (ecs.component_masks[i] & (1u << COMP_SWORD_TAG)) { sword_entity = i; break; }
    }
    CHECK(sword_entity != ECS_NULL_ENTITY);
    CHECK(ecs_get_sword_tag(&ecs, sword_entity)->angle != 0.0f);

    /* Release mouse: the sword should be destroyed. */
    p.input.mouse_buttons[0] = false;
    system_sword(&ecs, &p, 1.0f / 60.0f);

    bool alive = false;
    for (uint32_t i = 0; i < ECS_MAX_ENTITIES; i++) {
        if (!ecs.alive[i]) continue;
        if (ecs.component_masks[i] & (1u << COMP_SWORD_TAG)) alive = true;
    }
    CHECK(!alive);
}

static void test_rocket_damage_and_destruction(void) {
    LOG_INFO("--- Test: Rocket pierce & out-of-bounds destruction ---");
    World ecs;
    ecs_init(&ecs);
    GameWorld world;
    world_init(&world);

    Vec2 ppos = {world.world_pixel_w * 0.5f, world.world_pixel_h * 0.5f};
    Entity player = spawn_player(&ecs, ppos);
    CHECK(player != ECS_NULL_ENTITY);

    /* Place a zombie directly in front of the rocket path. */
    Vec2 zpos = vec2_add(ppos, vec2(20.0f, 0.0f));
    Entity zombie = waves_spawn_zombie(&ecs, zpos);
    CHECK(zombie != ECS_NULL_ENTITY);
    float hp_before = ecs_get_health(&ecs, zombie)->current;

    Player p;
    player_slot_wrap(&p, player);
    p.inventory.points = 1000;
    weapons_buy_launcher(&p.inventory);
    weapons_select(&p.inventory, WEAPON_LAUNCHER);

    p.input.mouse_pressed[0] = true;      /* fire on the click edge */
    p.input.mouse_world_x = ppos.x + 200.0f;
    p.input.mouse_world_y = ppos.y;

    /* Fire a rocket. */
    system_rockets(&ecs, &p, &world, 0.0f);
    CHECK(p.inventory.launcher_ammo == LAUNCHER_STARTER_ROCKETS - 1);

    Entity rocket = ECS_NULL_ENTITY;
    for (uint32_t i = 0; i < ECS_MAX_ENTITIES; i++) {
        if (!ecs.alive[i]) continue;
        if (ecs.component_masks[i] & (1u << COMP_ROCKET_TAG)) { rocket = i; break; }
    }
    CHECK(rocket != ECS_NULL_ENTITY);

    /* Advance and collide: rocket pierces through the zombie. */
    p.input.mouse_pressed[0] = false;
    system_rockets(&ecs, &p, &world, 0.016f);
    CHECK(ecs_get_health(&ecs, zombie)->current < hp_before);

    /* Move rocket out of world bounds → should be destroyed. */
    *ecs_get_position(&ecs, rocket) = (CPosition){{-100.0f, -100.0f}};
    system_rockets(&ecs, &p, &world, 0.01f);
    CHECK(!ecs_is_alive(&ecs, rocket));
}

static void test_grenade_detonation(void) {
    LOG_INFO("--- Test: Grenade AoE detonation ---");
    World ecs;
    ecs_init(&ecs);
    GameWorld world;
    world_init(&world);

    Vec2 ppos = {world.world_pixel_w * 0.5f, world.world_pixel_h * 0.5f};
    Entity player = spawn_player(&ecs, ppos);
    CHECK(player != ECS_NULL_ENTITY);

    /* Place a zombie near the aim point. */
    Vec2 zpos = vec2_add(ppos, vec2(40.0f, 0.0f));
    Entity zombie = waves_spawn_zombie(&ecs, zpos);
    CHECK(zombie != ECS_NULL_ENTITY);
    float hp_before = ecs_get_health(&ecs, zombie)->current;

    Player p;
    player_slot_wrap(&p, player);
    p.inventory.points = 1000;
    weapons_buy_grenade_pack(&p.inventory);
    CHECK(p.inventory.grenades == GRENADES_PER_PACK);
    weapons_select(&p.inventory, WEAPON_GRENADE);

    p.input.mouse_pressed[0] = true;
    p.input.mouse_world_x = ppos.x + 200.0f;
    p.input.mouse_world_y = ppos.y;

    /* Throw a grenade. */
    system_grenades(&ecs, &p, 0.0f);
    CHECK(p.inventory.grenades == GRENADES_PER_PACK - 1);

    Entity grenade = ECS_NULL_ENTITY;
    for (uint32_t i = 0; i < ECS_MAX_ENTITIES; i++) {
        if (!ecs.alive[i]) continue;
        if (ecs.component_masks[i] & (1u << COMP_GRENADE_TAG)) { grenade = i; break; }
    }
    CHECK(grenade != ECS_NULL_ENTITY);

    /* Advance the fuse until it detonates. */
    p.input.mouse_pressed[0] = false;
    for (int frame = 0; frame < 120; frame++) {
        system_grenades(&ecs, &p, 1.0f / 60.0f);
        if (!ecs_is_alive(&ecs, grenade)) break;
    }
    CHECK(!ecs_is_alive(&ecs, grenade));
    CHECK(ecs_get_health(&ecs, zombie)->current < hp_before);
}

/* ---------------------------------------------------------------- End Weapons */

static void test_net_codec(void) {
    LOG_INFO("--- Test: wire codec byte-exact round trips ---");

    /* Header encode/decode + checksum. */
    {
        uint8_t wire[NET_HDR_SIZE];
        NetHeader in = {NET_WIRE_VERSION, NET_PKT_JOIN, 0x1234, 0, 0};
        CHECK(net_hdr_encode(wire, &in) == 0);
        NetHeader out;
        CHECK(net_hdr_decode(&out, wire) == 0);
        CHECK(out.version == NET_WIRE_VERSION);
        CHECK(out.kind == NET_PKT_JOIN);
        CHECK(out.seq == 0x1234);
        /* Corrupt a header byte -> checksum rejects it. */
        wire[3] ^= 0xFF;
        CHECK(net_hdr_decode(&out, wire) != 0);
    }

    /* JOIN: name round trip (bounded to NET_NAME_MAX). */
    {
        char long_name[96];
        memset(long_name, 'A', sizeof(long_name) - 1);
        long_name[sizeof(long_name) - 1] = '\0';
        uint8_t buf[NET_HDR_SIZE + 1 + NET_NAME_MAX + 32];
        NetHeader jh = {NET_WIRE_VERSION, NET_PKT_JOIN, 42, 0, 0};
        int len = net_encode_join(buf, (int)sizeof(buf), &jh, long_name);
        CHECK(len >= NET_HDR_SIZE);
        NetHeader h;
        char name[NET_NAME_CAP];
        CHECK(net_decode_join(buf, len, &h, name, NET_NAME_CAP) == 0);
        CHECK(h.kind == NET_PKT_JOIN);
        CHECK(h.seq == 42);
        CHECK((int)strlen(name) == NET_NAME_MAX); /* trimmed, not truncated wire */
        /* Normal name. */
        NetHeader sh = {NET_WIRE_VERSION, NET_PKT_JOIN, 7, 0, 0};
        len = net_encode_join(buf, (int)sizeof(buf), &sh, "Alice");
        CHECK(net_decode_join(buf, len, &h, name, NET_NAME_CAP) == 0);
        CHECK(strcmp(name, "Alice") == 0);
    }

    /* HELLO: slot + seed + world_gen + host name. */
    {
        uint8_t buf[NET_HDR_SIZE + 1 + 4 + 4 + 1 + NET_NAME_MAX];
        NetHeader h = {NET_WIRE_VERSION, NET_PKT_HELLO, 0, 255, 0};
        int len = net_encode_hello(buf, (int)sizeof(buf), &h, 3, 2026u,
                                   NET_WORLD_GEN_VERSION, "Host-1");
        CHECK(len > NET_HDR_SIZE);
        NetHeader out;
        uint8_t slot;
        uint32_t seed, world;
        char host_name[NET_NAME_CAP];
        CHECK(net_decode_hello(buf, len, &out, &slot, &seed, &world,
                               host_name, NET_NAME_CAP) == 0);
        CHECK(slot == 3);
        CHECK(seed == 2026u);
        CHECK(world == NET_WORLD_GEN_VERSION);
        CHECK(strcmp(host_name, "Host-1") == 0);
    }

    /* REJECT + LEAVE round trips. */
    {
        uint8_t buf[NET_HDR_SIZE + 1];
        NetHeader h = {NET_WIRE_VERSION, NET_PKT_REJECT, 0, 0,
                       NET_FLAG_VERSION_MISMATCH};
        int len = net_encode_reject(buf, (int)sizeof(buf), &h, NET_REJECT_VERSION);
        NetHeader out;
        uint8_t reason = 0;
        CHECK(net_decode_reject(buf, len, &out, &reason) == 0);
        CHECK(reason == NET_REJECT_VERSION);
        CHECK(out.flags == NET_FLAG_VERSION_MISMATCH);

        NetHeader lh = {NET_WIRE_VERSION, NET_PKT_LEAVE, 0, 0, 0};
        len = net_encode_leave(buf, (int)sizeof(buf), &lh, NET_LEAVE_HOST_STOPPED);
        CHECK(net_decode_leave(buf, len, &out, &reason) == 0);
        CHECK(reason == NET_LEAVE_HOST_STOPPED);
    }

    /* PLAYER_LIST: 4 entries round trip, byte-identical length. */
    {
        NetPlayerInfo roster[NET_MAX_PLAYERS] = {
            {0, "Host", 0}, {1, "A", 1}, {2, "BeeBee", 2}, {3, "C-3PO", 3}
        };
        uint8_t buf[NET_HDR_SIZE + 1 + NET_MAX_PLAYERS * (2 + NET_NAME_MAX)];
        int len = net_encode_player_list(buf, (int)sizeof(buf), roster,
                                         NET_MAX_PLAYERS);
        CHECK(len > NET_HDR_SIZE);
        NetHeader h;
        NetPlayerInfo out[NET_MAX_PLAYERS];
        int n = 0;
        CHECK(net_decode_player_list(buf, len, &h, out, NET_MAX_PLAYERS, &n) == 0);
        CHECK(n == NET_MAX_PLAYERS);
        for (int i = 0; i < n; i++) {
            CHECK(out[i].slot == roster[i].slot);
            CHECK(out[i].color == roster[i].color);
            CHECK(strcmp(out[i].name, roster[i].name) == 0);
        }
        /* Re-encode the decoded list -> must match the original bytes. */
        uint8_t again[sizeof(buf)];
        int len2 = net_encode_player_list(again, (int)sizeof(again), out, n);
        CHECK(len2 == len);
        CHECK(memcmp(buf, again, (size_t)len) == 0);
    }

    /* Robustness: truncated payloads must fail cleanly, not over-read. */
    {
        uint8_t buf[NET_HDR_SIZE + 1 + NET_NAME_MAX];
        NetHeader jh = {NET_WIRE_VERSION, NET_PKT_JOIN, 1, 0, 0};
        int len = net_encode_join(buf, (int)sizeof(buf), &jh, "Bob");
        NetHeader h;
        char name[NET_NAME_CAP];
        CHECK(net_decode_join(buf, NET_HDR_SIZE, &h, name, NET_NAME_CAP) != 0);
        CHECK(net_decode_join(buf, 0, &h, name, NET_NAME_CAP) != 0);
        CHECK(net_decode_join(buf, len - 1, &h, name, NET_NAME_CAP) != 0);
    }

    /* Convenience name helpers. */
    CHECK(strcmp(net_pkt_kind_name(NET_PKT_HELLO), "hello") == 0);
    CHECK(strcmp(net_reject_reason_name(NET_REJECT_VERSION), "version mismatch") == 0);
    CHECK(strcmp(net_leave_reason_name(NET_LEAVE_HOST_STOPPED), "host stopped") == 0);
    CHECK(net_slot_color(0).r == COLOR_BLUE.r); /* host color is blue */
}

/* ---------------------------------------------------------------- Net codec */

int tests_run_all(void) {
    tests_passed = 0;
    tests_failed = 0;

    LOG_INFO("========================================");
    LOG_INFO("  Running static test suite");
    LOG_INFO("========================================");

    test_ecs_basics();
    test_world_valid();
    test_world_determinism();
    test_kill_credit();
    test_nearest_alive_target();
    test_beacon_anchor();
    test_tdm_respawn();
    test_hardcore_elimination();
    test_wave_system();
    test_items();
    test_entity_limit();
    test_wave_completion();
    test_script_aim_shot();
    test_event_stream();
    test_deterministic_seed();
    test_wave_timeout();
    test_weapons_inventory();
    test_sword_spin();
    test_rocket_damage_and_destruction();
    test_grenade_detonation();
    test_net_codec();

    LOG_INFO("========================================");
    LOG_INFO("  Tests passed: %d  Failed: %d", tests_passed, tests_failed);
    LOG_INFO("========================================");

    return tests_failed > 0 ? 1 : 0;
}

int main(void) {
    log_init(NULL, LOG_DEBUG, LOG_TRACE);
    srand(42);
    int result = tests_run_all();
    log_shutdown();
    return result;
}