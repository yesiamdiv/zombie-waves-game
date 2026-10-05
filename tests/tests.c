#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "core/log.h"
#include "ecs/ecs.h"
#include "world/theme.h"
#include "world/world.h"
#include "world/waves.h"
#include "world/map_registry.h"
#include "systems/systems.h"
#include "items/items.h"
#include "events/event_bus.h"
#include "ai/ai_driver.h"
#include "ai/ai_types.h"
#include "ui/hud.h"
#include "world/camera.h"
#include "config.h"
#include "net/net.h"
#include "net/net_codec.h"
#include "net/net_mirror.h"

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
    Entity z = waves_spawn_zombie(&ecs, vec2(500, 400), THEME_GRASSLAND);
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

    Entity z = waves_spawn_zombie(&ecs, vec2(500, 600), THEME_GRASSLAND);
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
    GameWorld world = {0};
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
    GameWorld a = {0}, b = {0};
    world_init(&a);
    world_init(&b);

    CHECK(a.width == b.width && a.height == b.height);
    CHECK(a.world_pixel_w == b.world_pixel_w);
    CHECK(a.world_pixel_h == b.world_pixel_h);

    bool same_tiles = true;
    for (int y = 0; y < a.height; y++) {
        for (int x = 0; x < a.width; x++) {
            if (a.tiles[y * a.width + x] != b.tiles[y * b.width + x]) {
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
    GameWorld world = {0};
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
    Entity zombie = waves_spawn_zombie(&ecs, vec2(500, 500), THEME_GRASSLAND);
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

    items_check_pickup(&ecs, player, NULL);
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

static void test_hud_damage_flash(void) {
    LOG_INFO("--- Test: HUD damage flash ---");
    HUD hud;
    hud_init(&hud);

    /* First call should establish the baseline without flashing. */
    hud_track_player_hp(&hud, 200.0f);
    CHECK(hud.damage_flash == 0.0f);

    /* Same HP → no flash. */
    hud_track_player_hp(&hud, 200.0f);
    CHECK(hud.damage_flash == 0.0f);

    /* HP drop → flash armed. */
    hud_track_player_hp(&hud, 170.0f);
    CHECK(hud.damage_flash == 1.0f);

    /* Flash decays, then re-arms on another drop. */
    hud_update(&hud, 0.1f);
    CHECK(hud.damage_flash < 1.0f);
    hud_track_player_hp(&hud, 140.0f);
    CHECK(hud.damage_flash == 1.0f);

    /* Healing raises HP and must not re-fire the flash. */
    hud.damage_flash = 0.0f;
    hud_track_player_hp(&hud, 185.0f);
    CHECK(hud.damage_flash == 0.0f);
}

static void test_item_ammo_pickup(void) {
    LOG_INFO("--- Test: Ammo pickup feeds owned weapons (B5) ---");
    World ecs;
    ecs_init(&ecs);

    Entity player = spawn_player(&ecs, vec2(100, 100));
    CHECK(player != ECS_NULL_ENTITY);

    PlayerInventory inv;
    weapons_inventory_init(&inv);

    /* Fresh run: only pistol unlocked → pickup grants nothing but is safe. */
    Entity ammo0 = items_spawn(&ecs, vec2(110, 100), ITEM_AMMO);
    CHECK(ammo0 != ECS_NULL_ENTITY);
    items_check_pickup(&ecs, player, &inv);
    CHECK(!ecs_is_alive(&ecs, ammo0));
    CHECK(inv.grenades == 0);
    CHECK(inv.launcher_ammo == 0);

    /* After unlocking grenades + launcher, pickups feed both stocks. */
    inv.unlocked[WEAPON_GRENADE] = true;
    inv.unlocked[WEAPON_LAUNCHER] = true;
    Entity ammo1 = items_spawn(&ecs, vec2(110, 100), ITEM_AMMO);
    CHECK(ammo1 != ECS_NULL_ENTITY);
    items_check_pickup(&ecs, player, &inv);
    CHECK(inv.grenades == 1);
    CHECK(inv.launcher_ammo == 2);

    /* Second pickup accumulates. */
    items_spawn(&ecs, vec2(110, 100), ITEM_AMMO);
    items_check_pickup(&ecs, player, &inv);
    CHECK(inv.grenades == 2);
    CHECK(inv.launcher_ammo == 4);
}

static void test_camera_clamp_world(void) {
    LOG_INFO("--- Test: Camera clamps to world bounds (B6) ---");
    Camera cam;
    camera_init(&cam, 1280, 720);   /* viewport: half 640 x 360 */

    float world_w = 2944.0f;        /* 46x34 tiles @ 64px */
    float world_h = 2176.0f;

    /* Mid-world: no clamping (position is well inside). */
    cam.position = vec2(world_w * 0.5f, world_h * 0.5f);
    camera_clamp_world(&cam, world_w, world_h);
    CHECK(fabsf(cam.position.x - world_w * 0.5f) < 0.001f);

    /* Far corner: clamp to half-viewport margins. */
    cam.position = vec2(0.0f, 0.0f);
    camera_clamp_world(&cam, world_w, world_h);
    CHECK(cam.position.x == 640.0f);
    CHECK(cam.position.y == 360.0f);

    cam.position = vec2(world_w, world_h);
    camera_clamp_world(&cam, world_w, world_h);
    CHECK(cam.position.x == 2944.0f - 640.0f);
    CHECK(cam.position.y == 2176.0f - 360.0f);

    /* World smaller than the viewport: keep centered, no void. */
    Camera small;
    camera_init(&small, 1280, 720);
    float tiny_w = 800.0f, tiny_h = 600.0f;
    small.position = vec2(99999.0f, -99999.0f);
    camera_clamp_world(&small, tiny_w, tiny_h);
    CHECK(small.position.x == 400.0f);
    CHECK(small.position.y == 300.0f);
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
    GameWorld world = {0};
    WaveSystem waves;

    ecs_init(&ecs);
    world_init(&world);
    waves_init(&waves, &world);

    Entity player = spawn_player(&ecs, vec2(world.world_pixel_w * 0.5f,
                                            world.world_pixel_h * 0.5f));
    CHECK(player != ECS_NULL_ENTITY);

    /* Force a small, fast wave so the test is quick and deterministic. */
    waves_start_next_wave(&waves, 1);
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
    GameWorld world = {0};
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
    waves_start_next_wave(&waves, 1);
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
    GameWorld world = {0};
    WaveSystem waves;
    ecs_init(&ecs);
    world_init(&world);
    waves_init(&waves, &world);

    Entity player = spawn_player(&ecs, vec2(world.world_pixel_w * 0.5f,
                                            world.world_pixel_h * 0.5f));
    CHECK(player != ECS_NULL_ENTITY);

    waves_start_next_wave(&waves, 1);
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
    GameWorld world = {0};
    WaveSystem waves;
    ecs_init(&ecs);
    world_init(&world);
    waves_init(&waves, &world);

    Entity player = spawn_player(&ecs, vec2(world.world_pixel_w * 0.5f,
                                            world.world_pixel_h * 0.5f));
    if (player == ECS_NULL_ENTITY) return -1;

    waves_start_next_wave(&waves, 1);
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
    GameWorld world = {0};
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

/* R13 merge: ported from the assets branch. Upstream these drove
 * system_sword(&ecs, &input, &inv, dt) and system_player_input(&ecs, &input,
 * &cam, dt, NULL); the multiplayer layer moved input+inventory onto the Player
 * slot, so the tests now wrap the entity in a slot and pass &p. */
static void test_sword_sweep_hits(void) {
    LOG_INFO("--- Test: Sword sweep hits along the blade ---");
    World ecs;
    ecs_init(&ecs);
    GameWorld world = {0};
    world_init(&world);

    Vec2 spawn = {world.world_pixel_w * 0.5f, world.world_pixel_h * 0.5f};
    Entity player = spawn_player(&ecs, spawn);
    CHECK(player != ECS_NULL_ENTITY);

    Player p;
    player_slot_wrap(&p, player);
    p.inventory.points = 1000;
    weapons_buy_sword(&p.inventory);
    weapons_select(&p.inventory, WEAPON_SWORD);

    /* A zombie sitting mid-blade (inner circle 26 -> outer 58) must be hit
     * by the sweeping blade, not just the single orbital point. */
    Vec2 zpos = vec2_add(spawn, vec2(42.0f, 0.0f));
    Entity zombie = waves_spawn_zombie(&ecs, zpos, THEME_GRASSLAND);
    CHECK(zombie != ECS_NULL_ENTITY);
    float hp_before = ecs_get_health(&ecs, zombie)->current;

    p.input.mouse_buttons[0] = true;
    p.input.mouse_world_x = spawn.x + 100.0f;
    p.input.mouse_world_y = spawn.y;

    int frames = 0;
    while (frames < 120 && ecs_get_health(&ecs, zombie)->current >= hp_before) {
        system_sword(&ecs, &p, 1.0f / 60.0f);
        frames++;
    }
    CHECK(ecs_get_health(&ecs, zombie)->current < hp_before);
}

static void test_zombie_contact_and_slow(void) {
    LOG_INFO("--- Test: Zombie contact damage & player slow ---");
    World ecs;
    ecs_init(&ecs);
    GameWorld world = {0};
    world_init(&world);

    Vec2 player_pos = {150.0f, 150.0f};
    Entity player = spawn_player(&ecs, player_pos);
    CHECK(player != ECS_NULL_ENTITY);

    Player p;
    player_slot_wrap(&p, player);

    /* A zombie overlapping the player: skimming through must no longer be
     * free; contact deals damage and slows the player. */
    Entity zombie = waves_spawn_zombie(&ecs, vec2(157.0f, 150.0f), THEME_GRASSLAND);
    CHECK(zombie != ECS_NULL_ENTITY);

    float hp_before = ecs_get_health(&ecs, player)->current;
    system_collision(&ecs, &world);

    CHECK(ecs_get_health(&ecs, player)->current < hp_before);
    CHECK(ecs_get_player_tag(&ecs, player)->slow_timer > 0.0f);

    /* Cooldown gating: an immediate second contact must not double-hit. */
    float hp_after_first = ecs_get_health(&ecs, player)->current;
    system_collision(&ecs, &world);
    CHECK(ecs_get_health(&ecs, player)->current == hp_after_first);

    /* While slowed, held movement input moves the player at reduced speed. */
    Camera cam;
    camera_init(&cam, 800, 600);
    p.input.keys[SDL_SCANCODE_D] = true;

    system_player_input(&ecs, &p, &cam, 1.0f / 60.0f);
    CVelocity *pvel = ecs_get_velocity(&ecs, player);
    CHECK(pvel->vel.x > 0.0f);
    CHECK(pvel->vel.x < 200.0f);   /* 200 max speed throttled by the hit slow */
}

static void test_rocket_damage_and_destruction(void) {
    LOG_INFO("--- Test: Rocket pierce & out-of-bounds destruction ---");
    World ecs;
    ecs_init(&ecs);
    GameWorld world = {0};
    world_init(&world);

    Vec2 ppos = {world.world_pixel_w * 0.5f, world.world_pixel_h * 0.5f};
    Entity player = spawn_player(&ecs, ppos);
    CHECK(player != ECS_NULL_ENTITY);

    /* Place a zombie directly in front of the rocket path. */
    Vec2 zpos = vec2_add(ppos, vec2(20.0f, 0.0f));
    Entity zombie = waves_spawn_zombie(&ecs, zpos, THEME_GRASSLAND);
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
    GameWorld world = {0};
    world_init(&world);

    Vec2 ppos = {world.world_pixel_w * 0.5f, world.world_pixel_h * 0.5f};
    Entity player = spawn_player(&ecs, ppos);
    CHECK(player != ECS_NULL_ENTITY);

    /* Place a zombie near the aim point. */
    Vec2 zpos = vec2_add(ppos, vec2(40.0f, 0.0f));
    Entity zombie = waves_spawn_zombie(&ecs, zpos, THEME_GRASSLAND);
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

static void test_map_registry_loads(void) {
    LOG_INFO("--- Test: Ship map registry loads ---");
    int count = map_registry_count();
    CHECK(count >= 4);

    for (int i = 0; i < count; i++) {
        const MapDef *def = map_registry_get(i);
        CHECK(def != NULL);
        CHECK(def->name != NULL && def->name[0] != '\0');
        CHECK(def->file != NULL && def->file[0] != '\0');

        GameWorld w = {0};
        CHECK(world_load_map(&w, def));
        CHECK(w.width >= 8 && w.height >= 8);
        CHECK(w.world_pixel_w == (float)w.width * WORLD_GRID_SIZE);
        CHECK(w.world_pixel_h == (float)w.height * WORLD_GRID_SIZE);
        CHECK(w.theme == def->theme);
        CHECK(w.tiles != NULL);

        /* Interior is bounded by walls and mostly walkable. */
        int walkable = 0, total = 0;
        for (int y = 0; y < w.height; y++) {
            for (int x = 0; x < w.width; x++) {
                total++;
                if (world_is_walkable(&w, x * WORLD_GRID_SIZE + 32,
                                             y * WORLD_GRID_SIZE + 32)) {
                    walkable++;
                }
            }
        }
        CHECK(walkable > total / 2);

        Vec2 sp = world_get_spawn_point(&w);
        CHECK(world_is_walkable(&w, sp.x, sp.y));
        world_free(&w);
    }

    /* Registry entry 0 resolves to an actual file on disk. */
    const MapDef *first = map_registry_get(0);
    GameWorld probe = {0};
    CHECK(world_load_map(&probe, first));
    world_free(&probe);
}

/* R13 merge: a non-zero map index, so the HELLO round trip proves the field
 * is carried rather than defaulting to 0 and passing by accident. */
#define TEST_HELLO_MAP 2

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
        uint8_t buf[NET_HDR_SIZE + 4 + 1 + NET_NAME_MAX + 32];
        NetHeader jh = {NET_WIRE_VERSION, NET_PKT_JOIN, 42, 0, 0};
        int len = net_encode_join(buf, (int)sizeof(buf), &jh, long_name,
                                  NET_ASSET_VERSION);
        CHECK(len >= NET_HDR_SIZE);
        NetHeader h;
        char name[NET_NAME_CAP];
        uint32_t assets = 0;
        CHECK(net_decode_join(buf, len, &h, name, NET_NAME_CAP, &assets) == 0);
        CHECK(h.kind == NET_PKT_JOIN);
        CHECK(h.seq == 42);
        CHECK((int)strlen(name) == NET_NAME_MAX); /* trimmed, not truncated wire */
        /* N2: the client's asset version must reach the host, or it cannot
         * refuse art drift. */
        CHECK(assets == NET_ASSET_VERSION);
        /* Normal name. */
        NetHeader sh = {NET_WIRE_VERSION, NET_PKT_JOIN, 7, 0, 0};
        len = net_encode_join(buf, (int)sizeof(buf), &sh, "Alice", 77u);
        CHECK(net_decode_join(buf, len, &h, name, NET_NAME_CAP, &assets) == 0);
        CHECK(strcmp(name, "Alice") == 0);
        CHECK(assets == 77u);
        /* A pre-N2 peer sends a JOIN with NO asset tail: that decodes, and is
         * reported as "unknown" so the host refuses with an asset reason. */
        {
            /* Names are length-prefixed on the wire (see put_name), so a
             * pre-N2 JOIN is header + len byte + name and nothing else. */
            uint8_t old[NET_HDR_SIZE + 8];
            NetHeader oh = {NET_WIRE_VERSION, NET_PKT_JOIN, 3, 0, 0};
            /* net_hdr_encode returns 0 on success, not the byte count. */
            CHECK(net_hdr_encode(old, &oh) == 0);
            old[NET_HDR_SIZE] = 3;
            memcpy(old + NET_HDR_SIZE + 1, "Bob", 3);
            int name_end = NET_HDR_SIZE + 4;
            CHECK(net_decode_join(old, name_end, &h, name, NET_NAME_CAP,
                                  &assets) == 0);
            CHECK(assets == 0xFFFFFFFFu);
            CHECK(strcmp(name, "Bob") == 0);
            /* A PARTIAL tail is corrupt, not "old": reject rather than let a
             * damaged JOIN through as merely out of date. */
            CHECK(net_decode_join(old, name_end + 1, &h, name, NET_NAME_CAP,
                                  &assets) != 0);
            CHECK(net_decode_join(old, name_end + 3, &h, name, NET_NAME_CAP,
                                  &assets) != 0);
        }
    }

    /* HELLO: slot + seed + world_gen + host name. */
    {
        uint8_t buf[NET_HDR_SIZE + 4 + 1 + 4 + 4 + 1 + 4 + NET_NAME_MAX];
        NetHeader h = {NET_WIRE_VERSION, NET_PKT_HELLO, 0, 255, 0};
        int len = net_encode_hello(buf, (int)sizeof(buf), &h, 3, 2026u,
                                   NET_WORLD_GEN_VERSION, TEST_HELLO_MAP,
                                   NET_ASSET_VERSION, "Host-1");
        CHECK(len > NET_HDR_SIZE);
        NetHeader out;
        uint8_t slot, map_index;
        uint32_t seed, world, host_assets = 0;
        char host_name[NET_NAME_CAP];
        CHECK(net_decode_hello(buf, len, &out, &slot, &seed, &world,
                               &map_index, &host_assets, host_name,
                               NET_NAME_CAP) == 0);
        /* N2: the host's asset version must survive, or the client cannot
         * refuse art drift. */
        CHECK(host_assets == NET_ASSET_VERSION);
        CHECK(slot == 3);
        CHECK(seed == 2026u);
        /* R13 merge: the host's map index must survive the round trip, or the
         * client builds a different world than the host. */
        CHECK(map_index == TEST_HELLO_MAP);
        CHECK(world == NET_WORLD_GEN_VERSION);
        CHECK(strcmp(host_name, "Host-1") == 0);
    }

    /* REJECT + LEAVE round trips. */
    {
        uint8_t buf[NET_HDR_SIZE + 1 + 4];
        NetHeader h = {NET_WIRE_VERSION, NET_PKT_REJECT, 0, 0,
                       NET_FLAG_VERSION_MISMATCH};
        int len = net_encode_reject(buf, (int)sizeof(buf), &h, NET_REJECT_VERSION,
                                    NET_WIRE_VERSION);
        NetHeader out;
        uint8_t reason = 0;
        uint32_t host_version = 0;
        CHECK(net_decode_reject(buf, len, &out, &reason, &host_version) == 0);
        CHECK(reason == NET_REJECT_VERSION);
        CHECK(out.flags == NET_FLAG_VERSION_MISMATCH);
        /* N2: the refused player must be able to see the host's number. */
        CHECK(host_version == NET_WIRE_VERSION);
        len = net_encode_reject(buf, (int)sizeof(buf), &h, NET_REJECT_ASSET,
                                NET_ASSET_VERSION);
        CHECK(net_decode_reject(buf, len, &out, &reason, &host_version) == 0);
        CHECK(reason == NET_REJECT_ASSET);
        CHECK(host_version == NET_ASSET_VERSION);
        /* A pre-N2 host sends only the reason byte: report unknown. A partial
         * tail is corrupt and must be rejected. */
        CHECK(net_decode_reject(buf, NET_HDR_SIZE + 1, &out, &reason,
                                &host_version) == 0);
        CHECK(host_version == 0xFFFFFFFFu);
        CHECK(net_decode_reject(buf, NET_HDR_SIZE + 2, &out, &reason,
                                &host_version) != 0);

        /* N2: the refusal text a player reads must name both numbers and the
         * remedy. "version mismatch" alone is what made this bug invisible. */
        char msg[160];
        net_version_conflict_message(msg, (int)sizeof(msg), NET_REJECT_VERSION,
                                     5, 4);
        CHECK(strstr(msg, "5") != NULL && strstr(msg, "4") != NULL);
        net_version_conflict_message(msg, (int)sizeof(msg), NET_REJECT_ASSET,
                                     1, 2);
        CHECK(strstr(msg, "art") != NULL);
        net_version_conflict_message(msg, (int)sizeof(msg), NET_REJECT_FULL,
                                     0, 0);
        CHECK(strstr(msg, "full") != NULL);

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
        uint8_t buf[NET_HDR_SIZE + 4 + 1 + NET_NAME_MAX];
        NetHeader jh = {NET_WIRE_VERSION, NET_PKT_JOIN, 1, 0, 0};
        int len = net_encode_join(buf, (int)sizeof(buf), &jh, "Bob",
                                  NET_ASSET_VERSION);
        NetHeader h;
        char name[NET_NAME_CAP];
        uint32_t assets = 0;
        CHECK(net_decode_join(buf, NET_HDR_SIZE, &h, name, NET_NAME_CAP,
                              &assets) != 0);
        CHECK(net_decode_join(buf, 0, &h, name, NET_NAME_CAP, &assets) != 0);
        /* Cutting one byte off a complete JOIN leaves a partial asset tail. */
        CHECK(net_decode_join(buf, len - 1, &h, name, NET_NAME_CAP, &assets) != 0);
    }

    /* Convenience name helpers. */
    CHECK(strcmp(net_pkt_kind_name(NET_PKT_HELLO), "hello") == 0);
    CHECK(strcmp(net_reject_reason_name(NET_REJECT_VERSION), "version mismatch") == 0);
    CHECK(strcmp(net_leave_reason_name(NET_LEAVE_HOST_STOPPED), "host stopped") == 0);
    CHECK(net_slot_color(0).r == COLOR_BLUE.r); /* host color is blue */
}

static void test_theme_registry(void) {
    LOG_INFO("--- Test: Theme registry sanity ---");
    CHECK(theme_count() == THEME_COUNT);
    CHECK(theme_count() >= 4);

    for (int id = 0; id < THEME_COUNT; id++) {
        const Theme *t = theme_get((ThemeID)id);
        CHECK(t != NULL);
        CHECK(t->name != NULL && t->name[0] != '\0');
        for (int i = 0; i < 3; i++) {
            CHECK(t->ground[i] != NULL && t->ground[i][0] != '\0');
        }
        CHECK(t->wall != NULL && t->wall[0] != '\0');
        CHECK(t->water != NULL && t->water[0] != '\0');
        CHECK(t->road != NULL && t->road[0] != '\0');
        CHECK(t->ground_color.a == 1.0f);
        CHECK(t->player_color.a == 1.0f);
        for (int i = 0; i < 3; i++) {
            CHECK(t->zombie_tints[i].a == 1.0f);
        }
        CHECK(theme_name((ThemeID)id) == t->name);
    }

    /* Out-of-range id clamps to a valid theme. */
    CHECK(theme_get((ThemeID)THEME_COUNT) != NULL);
    CHECK(theme_get((ThemeID)-1) != NULL);
}

static void test_map_parser(void) {
    LOG_INFO("--- Test: Map ASCII parser ---");
    GameWorld w = {0};
    const char *ascii =
        "#######\n"
        "#S.+##\n"
        "#~...#\n"
        "#####.#\n";
    CHECK(world_init_from_string(&w, ascii, THEME_DESERT));
    CHECK(w.width == 7);
    CHECK(w.height == 4);
    CHECK(w.world_pixel_w == 7.0f * WORLD_GRID_SIZE);
    CHECK(w.world_pixel_h == 4.0f * WORLD_GRID_SIZE);
    CHECK(w.theme == THEME_DESERT);

    CHECK(world_get_tile(&w, 0, 0) == TILE_WALL);
    CHECK(world_get_tile(&w, 6, 3) == TILE_WALL);
    CHECK(world_get_tile(&w, 5, 3) == TILE_GROUND);
    CHECK(world_get_tile(&w, 3, 1) == TILE_ROAD);
    CHECK(world_get_tile(&w, 1, 2) == TILE_WATER);
    CHECK(world_get_tile(&w, 1, 1) == TILE_GROUND);   /* 'S' -> ground */

    CHECK(w.has_spawn);
    CHECK(w.spawn_x == 1 && w.spawn_y == 1);
    Vec2 sp = world_get_spawn_point(&w);
    CHECK(world_is_walkable(&w, sp.x, sp.y));

    CHECK(world_get_tile(&w, -1, 0) == TILE_WALL);
    CHECK(world_get_tile(&w, 0, -1) == TILE_WALL);
    CHECK(world_get_tile(&w, 99, 99) == TILE_WALL);

    /* Too small to be a map. */
    GameWorld tiny = {0};
    CHECK(!world_init_from_string(&tiny, "ab\ncd\n", THEME_SNOW));
    world_free(&w);
}

/* Channel 1: input sample + world snapshot byte-exact round trips. */
static void test_net_codec_game(void) {
    LOG_INFO("--- Test: wire codec game traffic round trips ---");

    /* INPUT: controls map 1:1 to the wire (latest-wins, no reliability). */
    {
        uint8_t buf[NET_HDR_SIZE + 3 + 8];
        NetHeader h = {NET_WIRE_VERSION, NET_PKT_INPUT, 300, 1, 0};
        NetInput in = {
            NET_INPUT_MOVE_UP | NET_INPUT_MOVE_LEFT,
            NET_INPUT_BTN_SHOOT,
            2,
            -48.5f, 731.25f
        };
        int len = net_encode_input(buf, (int)sizeof(buf), &h, &in);
        CHECK(len == NET_HDR_SIZE + 3 + 8);
        NetHeader out;
        NetInput got;
        memset(&got, 0, sizeof(got));
        CHECK(net_decode_input(buf, len, &out, &got) == 0);
        CHECK(out.kind == NET_PKT_INPUT);
        CHECK(out.seq == 300);
        CHECK(out.from_slot == 1);
        CHECK(got.move_flags == in.move_flags);
        CHECK(got.buttons == in.buttons);
        CHECK(got.weapon == in.weapon);
        CHECK(got.aim_x == in.aim_x);
        CHECK(got.aim_y == in.aim_y);
        /* Re-encode -> identical bytes (byte-exact contract). */
        uint8_t again[sizeof(buf)];
        int len2 = net_encode_input(again, (int)sizeof(again), &out, &got);
        CHECK(len2 == len);
        CHECK(memcmp(buf, again, (size_t)len) == 0);
    }

    /* SNAPSHOT: header + wave state + mixed entity kinds round trip. */
    {
        uint8_t buf[NET_SNAP_MAX_BYTES];
        NetSnapshot snap = {0};
        snap.sim_time = 12.5f;
        snap.wave_number = 3;
        snap.wave_active = 1;
        snap.total_kills = 42;
        snap.count = 3;
        snap.entities[0] = (NetEntitySnap){0, NET_ENT_PLAYER,
                                           {100.0f, 200.0f}, {0.0f, 0.0f},
                                           100.0f, 0, 0};
        snap.entities[1] = (NetEntitySnap){88, NET_ENT_ZOMBIE,
                                           {10.25f, -5.5f}, {30.0f, -12.0f},
                                           44.0f, 0, 0};
        snap.entities[2] = (NetEntitySnap){120, NET_ENT_BULLET,
                                           {55.0f, 33.0f}, {600.0f, 400.0f},
                                           0.0f, 0, 1};
        NetHeader h = {NET_WIRE_VERSION, NET_PKT_SNAPSHOT, 7, 0, 0};
        int len = net_encode_snapshot(buf, (int)sizeof(buf), &h, &snap);
        CHECK(len > NET_HDR_SIZE);
        NetHeader out;
        NetSnapshot got;
        memset(&got, 0, sizeof(got));
        CHECK(net_decode_snapshot(buf, len, &out, &got, NET_SNAP_MAX_ENTITIES) == 0);
        CHECK(out.kind == NET_PKT_SNAPSHOT);
        CHECK(out.from_slot == 0);
        CHECK(got.sim_time == snap.sim_time);
        CHECK(got.wave_number == 3);
        CHECK(got.wave_active == 1);
        CHECK(got.total_kills == 42);
        CHECK(got.count == 3);
        for (int i = 0; i < 3; i++) {
            CHECK(got.entities[i].id == snap.entities[i].id);
            CHECK(got.entities[i].kind == snap.entities[i].kind);
            CHECK(got.entities[i].pos.x == snap.entities[i].pos.x);
            CHECK(got.entities[i].pos.y == snap.entities[i].pos.y);
            CHECK(got.entities[i].vel.x == snap.entities[i].vel.x);
            CHECK(got.entities[i].vel.y == snap.entities[i].vel.y);
            CHECK(got.entities[i].hp == snap.entities[i].hp);
            CHECK(got.entities[i].owner == snap.entities[i].owner);
        }
        /* Re-encode decoded snapshot -> byte-exact. */
        uint8_t again[sizeof(buf)];
        int len2 = net_encode_snapshot(again, (int)sizeof(again), &out, &got);
        CHECK(len2 == len);
        CHECK(memcmp(buf, again, (size_t)len) == 0);
        /* Truncated snapshot must fail, not over-read. */
        NetSnapshot bad;
        CHECK(net_decode_snapshot(buf, NET_HDR_SIZE + 5, &out, &bad,
                                  NET_SNAP_MAX_ENTITIES) != 0);
        /* count beyond the caller's buffer must be rejected. */
        CHECK(net_decode_snapshot(buf, len, &out, &bad, 2) != 0);
    }

    /* Snapshot with many entities (interpolation sanity: 100 zombies). */
    {
        uint8_t buf[NET_SNAP_MAX_BYTES];
        NetSnapshot snap = {0};
        snap.count = 100;
        for (int i = 0; i < 100; i++) {
            snap.entities[i].id = (uint16_t)i;
            snap.entities[i].kind = NET_ENT_ZOMBIE;
            snap.entities[i].pos.x = (float)i * 3.0f;
            snap.entities[i].pos.y = 777.0f;
            snap.entities[i].hp = 100.0f;
        }
        NetHeader h = {NET_WIRE_VERSION, NET_PKT_SNAPSHOT, 1, 0, 0};
        int len = net_encode_snapshot(buf, (int)sizeof(buf), &h, &snap);
        CHECK(len > NET_HDR_SIZE);
        NetHeader out;
        NetSnapshot got;
        memset(&got, 0, sizeof(got));
        CHECK(net_decode_snapshot(buf, len, &out, &got, NET_SNAP_MAX_ENTITIES) == 0);
        CHECK(got.count == 100);
        for (int i = 0; i < 100; i++) {
            CHECK(got.entities[i].id == (uint16_t)i);
            CHECK(got.entities[i].pos.x == (float)i * 3.0f);
        }
    }
}

/* Shared art table integrity (ADR-2). This is the only part of the appearance
 * work CI can genuinely verify: that both peers resolve an id to the same path
 * and back, and that the sizes the wire maths depends on are sane. Whether the
 * client then DRAWS the right thing is visual and needs a human. */
static void test_net_art_table(void) {
    LOG_INFO("--- Test: shared art table integrity ---");

    /* NET_ART_NONE means "the host drew a flat shape" and must have no
     * texture and no size to divide by. */
    CHECK(net_art_path(NET_ART_NONE) == NULL);
    CHECK(net_art_base_px(NET_ART_NONE) == 0);
    CHECK(net_art_from_path(NULL) == NET_ART_NONE);
    CHECK(net_art_from_path("") == NET_ART_NONE);

    int textured = 0;
    for (int i = 1; i < NET_ART_COUNT; i++) {
        const char *path = net_art_path((uint8_t)i);
        CHECK(path != NULL);
        CHECK(path[0] != '\0');
        /* Round trip: this is the property that guarantees both peers agree.
         * A typo in the table fails here rather than on a second machine. */
        CHECK(net_art_from_path(path) == (uint8_t)i);
        CHECK(net_art_base_px((uint8_t)i) > 0);
        textured++;
    }
    CHECK(textured == NET_ART_COUNT - 1);

    /* An id from a newer host must degrade to NET_ART_NONE (draw a circle),
     * never to some other sprite. */
    CHECK(net_art_path(NET_ART_COUNT) == NULL);
    CHECK(net_art_path(200) == NULL);
    CHECK(net_art_base_px(200) == 0);
    CHECK(net_art_from_path("textures/entities/not_a_thing.png") == NET_ART_NONE);
    /* The paths must be the ones the spawn code actually asks for; if a spawn
     * site is renamed and the table is not, art silently degrades to a circle. */
    CHECK(strcmp(net_art_path(NET_ART_ZOMBIE),
                 "textures/entities/zombie.png") == 0);
    CHECK(strcmp(net_art_path(NET_ART_BULLET),
                 "textures/entities/bullet.png") == 0);
}

/* Snapshot builder maps a live world into the 20 Hz wire format. */
static void test_net_snapshot_build(void) {
    LOG_INFO("--- Test: net_snapshot_build maps the live world ---");
    World ecs;
    ecs_init(&ecs);
    Player players[MAX_PLAYERS];
    players_reset(players, MAX_PLAYERS);
    player_respawn(players, &ecs, 0, "P1", &COLOR_RED, vec2(10, 20));
    Entity p0 = players[0].entity;
    Entity z = waves_spawn_zombie(&ecs, vec2(100, 200), THEME_GRASSLAND);
    CHECK(z != ECS_NULL_ENTITY);

    Entity bullet = ecs_create_entity(&ecs);
    CHECK(bullet != ECS_NULL_ENTITY);
    ecs_add_component(&ecs, bullet, COMP_POSITION);
    ecs_add_component(&ecs, bullet, COMP_VELOCITY);
    ecs_add_component(&ecs, bullet, COMP_SPRITE);
    ecs_add_component(&ecs, bullet, COMP_COLLIDER);
    ecs_add_component(&ecs, bullet, COMP_BULLET_TAG);
    ecs_get_position(&ecs, bullet)->pos = vec2(55, 66);
    ecs_get_velocity(&ecs, bullet)->vel = vec2(400, 0);
    ecs_get_velocity(&ecs, bullet)->max_speed = 400.0f;
    ecs_get_bullet_tag(&ecs, bullet)->owner = p0;

    GameWorld gw = {0};
    world_init(&gw);
    WaveSystem ws;
    waves_init(&ws, &gw);
    ws.wave_number = 4;
    ws.wave_active = true;
    ws.total_kills = 9;

    NetSnapshot snap;
    int n = net_snapshot_build(&ecs, players, MAX_PLAYERS, 6.25f, &ws, &snap);
    CHECK(n >= 3);
    CHECK(snap.sim_time == 6.25f);
    CHECK(snap.wave_number == 4);
    CHECK(snap.wave_active == 1);
    CHECK(snap.total_kills == 9);
    CHECK(snap.slot_entities[0] == p0);
    CHECK(snap.slot_entities[1] == 0);
    for (int s = 0; s < NET_MAX_PLAYERS; s++) CHECK(snap.slot_entities[s] < 2048);

    int players_n = 0, zombies_n = 0, bullets_n = 0;
    Entity bulletOwner = ECS_NULL_ENTITY;
    for (int i = 0; i < n; i++) {
        const NetEntitySnap *e = &snap.entities[i];
        if (e->kind == NET_ENT_PLAYER) {
            players_n++;
            CHECK(e->id == p0);
            CHECK(e->pos.x == 10.0f && e->pos.y == 20.0f);
            CHECK(e->hp > 0.0f);
        } else if (e->kind == NET_ENT_ZOMBIE) {
            zombies_n++;
            CHECK(e->id == z);
            CHECK(e->pos.x == 100.0f);
        } else if (e->kind == NET_ENT_BULLET) {
            bullets_n++;
            CHECK(e->id == bullet);
            CHECK(e->vel.x == 400.0f);
            bulletOwner = e->owner;
        }
    }
    CHECK(players_n == 1);
    CHECK(zombies_n == 1);
    CHECK(bullets_n == 1);
    CHECK(bulletOwner == p0);

    /* Appearance block (R13-I5). Note the diameter is 2*radius on the flat
     * path and scale*base_px on the textured path; for a zombie both are 2*size,
     * so this assertion holds headless AND on a device with real art. */
    NetEntitySnap zombie_e = {0};
    NetEntitySnap player_e = {0};
    bool have_zombie = false, have_player = false;
    for (int i = 0; i < n; i++) {
        if (snap.entities[i].kind == NET_ENT_ZOMBIE) {
            zombie_e = snap.entities[i];
            have_zombie = true;
        } else if (snap.entities[i].kind == NET_ENT_PLAYER) {
            player_e = snap.entities[i];
            have_player = true;
        }
    }
    CHECK(have_zombie && have_player);
    /* size_q is a world diameter in half units: 2*size. */
    CHECK(zombie_e.size_q > 0);
    CHECK(zombie_e.size_q / 4.0f >= 10.0f && zombie_e.size_q / 4.0f <= 15.0f);
    /* The player is drawn as a 16-unit square at scale 0.5 -> diameter 16. */
    CHECK(player_e.size_q == 32);
    /* Headless has no asset manager, so nothing is textured and the host must
     * faithfully report NET_ART_NONE (a flat shape) rather than claim art it
     * did not draw. */
    CHECK(zombie_e.art == NET_ART_NONE);
    /* Tint is the host's literal sprite colour, so it must be non-black for a
     * themed zombie and must match the player's own colour. */
    CHECK(zombie_e.tint[0] || zombie_e.tint[1] || zombie_e.tint[2]);
    CHECK(player_e.tint[0] == 255);   /* COLOR_RED */

    /* The built snapshot round-trips through the wire codec. */
    uint8_t buf[NET_HDR_SIZE + 18 + NET_SNAP_MAX_ENTITIES * NET_SNAP_ENTRY_BYTES];
    NetHeader h = {NET_WIRE_VERSION, NET_PKT_SNAPSHOT, 3, 0, 0};
    int len = net_encode_snapshot(buf, (int)sizeof(buf), &h, &snap);
    CHECK(len > NET_HDR_SIZE);
    NetSnapshot got;
    memset(&got, 0, sizeof(got));
    NetHeader oh;
    CHECK(net_decode_snapshot(buf, len, &oh, &got, NET_SNAP_MAX_ENTITIES) == 0);
    CHECK(got.count == n);
    CHECK(got.slot_entities[0] == p0);
    CHECK(got.sim_time == 6.25f);
    /* NET_WIRE_VERSION 4: appearance survives the round trip, and the encoded
     * size must be exactly the declared entry size or a v3/v4 pair silently
     * strides the wrong distance. */
    bool got_zombie = false;
    for (int i = 0; i < got.count; i++) {
        if (got.entities[i].kind == NET_ENT_ZOMBIE) {
            got_zombie = true;
            CHECK(got.entities[i].art == zombie_e.art);
            CHECK(got.entities[i].size_q == zombie_e.size_q);
            CHECK(got.entities[i].tint[0] == zombie_e.tint[0]);
            CHECK(got.entities[i].tint[1] == zombie_e.tint[1]);
            CHECK(got.entities[i].tint[2] == zombie_e.tint[2]);
        }
    }
    CHECK(got_zombie);
    /* Header via the constant, not a hand-counted 18: the header grew by
     * NET_PLAYER_STATE_BYTES * NET_MAX_PLAYERS in wire v6 and a literal here
     * would have gone stale silently. */
    int expect_len = NET_HDR_SIZE + NET_SNAP_HEADER_BYTES +
                     n * NET_SNAP_ENTRY_BYTES;
    CHECK(len == expect_len);
    /* NetEntitySnap is NOT packed: the compiler pads it (36 bytes here against
     * 31 on the wire). That is fine and intended — the codec writes each field
     * explicitly, so the padding never reaches the wire. This assertion exists
     * so nobody "fixes" NET_SNAP_ENTRY_BYTES to sizeof() and silently changes
     * the protocol. The load-bearing check is the encoded length above. */
    CHECK(sizeof(NetEntitySnap) >= NET_SNAP_ENTRY_BYTES);
}

/* Sprint N3 (B22/B23): the per-slot state block the client HUD now reads.
 *
 * Before this, the HUD asked the client's OWN ECS for HP and the client's own
 * Player struct for points/ammo/weapon. A render-only client simulates nothing,
 * so those values froze at join and contradicted the host. This test is the
 * regression: state the host has moved on from must reach the wire, including
 * for a slot that has NO live entity (dead), because a dead slot is exactly
 * when the respawn countdown has to be shown. */
static void test_net_player_state(void) {
    World ecs;
    Player players[MAX_PLAYERS];
    NetSnapshot snap, got;

    ecs_init(&ecs);
    players_reset(players, MAX_PLAYERS);

    CHECK(player_respawn(players, &ecs, 0, "Host", &COLOR_RED, vec2(10, 20)) == 0);
    CHECK(player_respawn(players, &ecs, 1, "Alice", &COLOR_BLUE, vec2(30, 40)) == 1);
    CHECK(player_respawn(players, &ecs, 2, "Bob", &COLOR_BLUE, vec2(50, 60)) == 2);

    players[0].inventory.points = 1234;
    players[0].inventory.grenades = 7;
    players[0].inventory.launcher_ammo = 3;
    players[0].inventory.current = WEAPON_LAUNCHER;
    players[0].inventory.unlocked[WEAPON_SWORD] = true;
    players[0].inventory.unlocked[WEAPON_LAUNCHER] = true;
    players[1].inventory.points = 55;

    /* Bob is dead: no live entity, but the host still owes the client a
     * respawn countdown. */
    players[2].alive = false;
    players[2].respawn_timer = 4.25f;
    players[2].entity = ECS_NULL_ENTITY;

    CHECK(net_snapshot_build(&ecs, players, MAX_PLAYERS, 1.5f, NULL, &snap) >= 0);

    /* ---- host-side encoding ---- */
    const NetPlayerState *h0 = &snap.player_states[0];
    CHECK((h0->flags & NET_PST_IN_USE) != 0);
    CHECK((h0->flags & NET_PST_ALIVE) != 0);
    CHECK(h0->points == 1234);
    CHECK(h0->grenades == 7);
    CHECK(h0->launcher_ammo == 3);
    CHECK(h0->weapon == (uint8_t)WEAPON_LAUNCHER);
    CHECK((h0->unlocked_mask & (1u << WEAPON_SWORD)) != 0);
    CHECK((h0->unlocked_mask & (1u << WEAPON_LAUNCHER)) != 0);
    CHECK((h0->unlocked_mask & (1u << WEAPON_PISTOL)) != 0); /* always owned */
    CHECK(h0->hp_centis > 0 && h0->hp_max_centis > 0);

    const NetPlayerState *d2 = &snap.player_states[2];
    CHECK((d2->flags & NET_PST_IN_USE) != 0);
    CHECK((d2->flags & NET_PST_ALIVE) == 0);
    CHECK(d2->respawn_centis == 425);          /* 4.25s, nearest centi */
    CHECK(snap.slot_entities[2] == 0);         /* and there is no entity */

    /* An unused slot must be flagged out, or a client would draw a phantom
     * "0 points / dead" player before the roster arrives. Slot 3 was never
     * spawned, so it must read as not-in-use. */
    CHECK((snap.player_states[3].flags & NET_PST_IN_USE) == 0);

    /* ---- round trip ---- */
    uint8_t buf[NET_SNAP_MAX_BYTES];
    NetHeader hdr = {NET_WIRE_VERSION, NET_PKT_SNAPSHOT, 7, 0, 0};
    int len = net_encode_snapshot(buf, (int)sizeof(buf), &hdr, &snap);
    CHECK(len > 0);
    CHECK(net_decode_snapshot(buf, len, &hdr, &got, NET_SNAP_MAX_ENTITIES) == 0);
    for (int i = 0; i < NET_MAX_PLAYERS; i++) {
        CHECK(got.player_states[i].points == snap.player_states[i].points);
        CHECK(got.player_states[i].grenades == snap.player_states[i].grenades);
        CHECK(got.player_states[i].launcher_ammo ==
              snap.player_states[i].launcher_ammo);
        CHECK(got.player_states[i].hp_centis == snap.player_states[i].hp_centis);
        CHECK(got.player_states[i].hp_max_centis ==
              snap.player_states[i].hp_max_centis);
        CHECK(got.player_states[i].respawn_centis ==
              snap.player_states[i].respawn_centis);
        CHECK(got.player_states[i].weapon == snap.player_states[i].weapon);
        CHECK(got.player_states[i].unlocked_mask ==
              snap.player_states[i].unlocked_mask);
        CHECK(got.player_states[i].flags == snap.player_states[i].flags);
    }

    /* ---- the state must survive the MIRROR, which is what the HUD reads --- */
    NetMirror m;
    net_mirror_reset(&m);
    NetPlayerState hs;
    CHECK(!net_mirror_player_state(&m, 0, &hs));   /* nothing pushed yet */
    net_mirror_push(&m, &snap);
    CHECK(net_mirror_player_state(&m, 0, &hs));
    CHECK(hs.points == 1234 && hs.grenades == 7 && hs.launcher_ammo == 3);
    CHECK(hs.weapon == WEAPON_LAUNCHER);
    CHECK(net_mirror_player_state(&m, 2, &hs));  /* dead, but still in use */
    CHECK((hs.flags & NET_PST_ALIVE) == 0);
    /* The countdown is what a dead client has to show; it must survive too. */
    CHECK(hs.respawn_centis == 425);

    /* A truncated packet must be refused, not half-applied: a client that read
     * 3 of 4 slots would show one player's real HP next to two frozen ones. */
    CHECK(net_decode_snapshot(buf, NET_HDR_SIZE + NET_SNAP_HEADER_BYTES - 1,
                              &hdr, &got, NET_SNAP_MAX_ENTITIES) != 0);

    /* Negative/oversized game values must not wrap into absurd u16s. */
    players[0].inventory.points = -5;
    players[0].inventory.grenades = 1 << 20;
    CHECK(net_snapshot_build(&ecs, players, MAX_PLAYERS, 1.5f, NULL, &snap) >= 0);
    CHECK(snap.player_states[0].points == 0);
    CHECK(snap.player_states[0].grenades == 65535u);
}

/* Two-snapshot mirror: pushes blend pos/hp, slot map resolves own player,
 * single-snapshot state samples at full weight, absent ids are rejected. */
static void test_net_mirror_interp(void) {
    LOG_INFO("--- Test: net_mirror interpolation ---");
    NetMirror m;
    net_mirror_reset(&m);
    CHECK(!net_mirror_ready(&m));
    CHECK(!net_mirror_sample(&m, 5, 0.5f, NULL));

    NetSnapshot s0 = {0};
    s0.sim_time = 0.0f;
    s0.slot_entities[0] = 7;
    s0.count = 1;
    s0.entities[0] = (NetEntitySnap){7, NET_ENT_PLAYER, {0, 0}, {0, 0}, 200.0f, 0, 0};
    net_mirror_push(&m, &s0);
    CHECK(net_mirror_ready(&m));
    /* Single snapshot: full-weight regardless of t. */
    NetEntitySnap e;
    CHECK(net_mirror_sample(&m, 7, 0.0f, &e));
    CHECK(e.pos.x == 0.0f);
    CHECK(net_mirror_slot_for(&m, 7) == 0);
    CHECK(net_mirror_slot_for(&m, 99) == -1);

    NetSnapshot s1 = {0};
    s1.sim_time = 1.0f;
    s1.slot_entities[0] = 7;
    s1.count = 1;
    s1.entities[0] = (NetEntitySnap){7, NET_ENT_PLAYER, {100, 20}, {0, 0}, 150.0f, 0, 0};
    net_mirror_push(&m, &s1);

    /* Blend 0 -> older, 1 -> newer, 0.5 -> midpoint. */
    CHECK(net_mirror_sample(&m, 7, 0.0f, &e));
    CHECK(e.pos.x == 0.0f);
    CHECK(net_mirror_sample(&m, 7, 1.0f, &e));
    CHECK(e.pos.x == 100.0f);
    CHECK(e.hp == 150.0f);
    CHECK(net_mirror_sample(&m, 7, 0.5f, &e));
    CHECK(e.pos.x == 50.0f && e.pos.y == 10.0f);
    CHECK(net_mirror_sample(&m, 7, 2.0f, &e));
    CHECK(e.pos.x == 100.0f);           /* t clamped */
    CHECK(net_mirror_sample(&m, 7, -1.0f, &e));
    CHECK(e.pos.x == 0.0f);

    /* Entries only in the newer snapshot resolve at full weight. */
    NetSnapshot s2 = {0};
    s2.sim_time = 2.0f;
    s2.slot_entities[0] = 7;
    s2.count = 2;
    s2.entities[0] = (NetEntitySnap){7, NET_ENT_PLAYER, {200, 0}, {0, 0}, 100.0f, 0, 0};
    s2.entities[1] = (NetEntitySnap){11, NET_ENT_ZOMBIE, {55, 0}, {0, 0}, 50.0f, 0, 0};
    net_mirror_push(&m, &s2);
    CHECK(net_mirror_sample(&m, 11, 0.25f, &e));
    CHECK(e.kind == NET_ENT_ZOMBIE);
    CHECK(e.pos.x == 55.0f);
    CHECK(!net_mirror_sample(&m, 999, 0.5f, &e));
    CHECK(net_mirror_slot_for(&m, 7) == 0);
    CHECK(net_mirror_older_time(&m) == 1.0f);
    CHECK(net_mirror_newer_time(&m) == 2.0f);
}

/* Relayed-event codec round trip + mirror death-removal (§6.3). */
static void test_net_events_codec(void) {
    LOG_INFO("--- Test: net_events codec + mirror removal ---");
    NetRelayedEvent ev[3] = {
        {GE_ENTITY_DEATH, GEK_ZOMBIE, 42, 100, 200, 0, 0},
        {GE_PLAYER_HEALTH, GEK_PLAYER, 7, 0, 0, 88.0f, 200.0f},
        {GE_WAVE_START, GEK_NONE, 0, 0, 0, 2.0f, 8.0f},
    };
    uint8_t buf[NET_EVENTS_MAX_BYTES];
    NetHeader h = {NET_WIRE_VERSION, NET_PKT_EVENTS, 9, 0, 0};
    int len = net_encode_events(buf, (int)sizeof(buf), &h, ev, 3);
    CHECK(len > NET_HDR_SIZE);
    NetHeader oh;
    NetRelayedEvent got[NET_EVENTS_MAX_BATCH];
    int n = 0;
    CHECK(net_decode_events(buf, len, &oh, got, NET_EVENTS_MAX_BATCH, &n) == 0);
    CHECK(n == 3);
    CHECK(oh.kind == NET_PKT_EVENTS && oh.seq == 9);
    CHECK(got[0].type == GE_ENTITY_DEATH && got[0].id == 42);
    CHECK(got[0].x == 100.0f && got[0].y == 200.0f);
    CHECK(got[1].type == GE_PLAYER_HEALTH && got[1].a == 88.0f);
    CHECK(got[2].type == GE_WAVE_START && got[2].a == 2.0f && got[2].b == 8.0f);
    CHECK(net_decode_events(buf, NET_HDR_SIZE + 1, &oh, got, 4, &n) == -1);

    /* A relayed death removes the entity from BOTH mirror snapshots. */
    NetMirror m;
    net_mirror_reset(&m);
    NetSnapshot a = {0};
    a.sim_time = 0.0f;
    a.count = 2;
    a.entities[0] = (NetEntitySnap){42, NET_ENT_ZOMBIE, {0, 0}, {0, 0}, 50.0f, 0, 0};
    a.entities[1] = (NetEntitySnap){7, NET_ENT_PLAYER, {1, 1}, {0, 0}, 100.0f, 0, 0};
    net_mirror_push(&m, &a);
    NetSnapshot b = {0};
    b.sim_time = 0.05f;
    b.count = 2;
    b.entities[0] = (NetEntitySnap){42, NET_ENT_ZOMBIE, {10, 0}, {0, 0}, 12.0f, 0, 0};
    b.entities[1] = (NetEntitySnap){7, NET_ENT_PLAYER, {2, 2}, {0, 0}, 100.0f, 0, 0};
    uint16_t dead[1] = {42};
    net_mirror_push_removing(&m, &b, dead, 1);
    CHECK(m.newer.count == 1);
    CHECK(m.older.count == 1);
    NetEntitySnap e;
    CHECK(!net_mirror_sample(&m, 42, 0.5f, &e));   /* removed from both */
    CHECK(net_mirror_sample(&m, 7, 0.5f, &e));     /* survivor stays */
    CHECK(e.pos.x == 1.5f);
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
    test_hud_damage_flash();
    test_item_ammo_pickup();
    test_camera_clamp_world();
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
    /* R13 merge: both engines' suites run. The assets tests (map parser,
     * registry, theme, HUD flash, ammo pickup, camera clamp, sword sweep,
     * zombie contact) guard the world/texture layer; the net tests guard
     * replication. Neither set subsumes the other. */
    test_theme_registry();
    test_map_parser();
    test_map_registry_loads();
    test_sword_sweep_hits();
    test_zombie_contact_and_slow();
    test_net_codec();
    test_net_codec_game();
    test_net_art_table();
    test_net_snapshot_build();
    test_net_player_state();
    test_net_mirror_interp();
    test_net_events_codec();

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