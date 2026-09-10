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
        waves_update(&waves, &ecs, &world, dt);
        system_zombie_ai(&ecs, dt);
        system_movement(&ecs, &world, dt);
        system_collision(&ecs, &world);
        system_cleanup(&ecs);
    }

    CHECK(waves.wave_number >= 1);
    CHECK(waves.zombies_spawned > 0);

    /* Spawn a zombie directly and test bullet damage */
    Entity zombie = waves_spawn_zombie(&ecs, vec2(500, 500));
    CHECK(zombie != ECS_NULL_ENTITY);
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
        system_cleanup(&ecs);
        safety++;
    }
    CHECK(!ecs_is_alive(&ecs, zombie));
}

static void test_items(void) {
    LOG_INFO("--- Test: Items ---");
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

int tests_run_all(void) {
    tests_passed = 0;
    tests_failed = 0;

    LOG_INFO("========================================");
    LOG_INFO("  Running static test suite");
    LOG_INFO("========================================");

    test_ecs_basics();
    test_world_valid();
    test_wave_system();
    test_items();
    test_entity_limit();

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