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
#include "config.h"

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
        waves_update(&waves, &ecs, &world, dt);
        system_zombie_ai(&ecs, dt);
        system_movement(&ecs, &world, dt);
        system_collision(&ecs, &world);
        system_cleanup(&ecs, &waves, NULL);
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
        system_cleanup(&ecs, &waves, NULL);
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
    waves_start_next_wave(&waves);
    waves.zombies_to_spawn = 4;
    waves.spawn_interval = 0.05f;

    float dt = 1.0f / 120.0f;
    int guard = 0;
    while (guard < 3000 && waves.wave_number == 1 && !waves.between_waves) {
        guard++;

        waves_update(&waves, &ecs, &world, dt);

        /* Shoot every alive zombie every frame until it dies. */
        for (uint32_t i = 0; i < ECS_MAX_ENTITIES; i++) {
            if (!ecs.alive[i]) continue;
            if (!(ecs.component_masks[i] & (1u << COMP_ZOMBIE_TAG))) continue;
            Entity b = spawn_test_bullet(&ecs, ecs_get_position(&ecs, i)->pos, player);
            if (b != ECS_NULL_ENTITY) {
                system_collision(&ecs, &world);
            }
        }

        system_cleanup(&ecs, &waves, NULL);
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
    waves_start_next_wave(&waves);
    waves.zombies_to_spawn = 1;
    waves.spawn_interval = 0.01f;

    float dt = 1.0f / 120.0f;
    int frames_ran = 0;
    int max_frames = (int)(75.0f / dt);
    while (!waves.between_waves && frames_ran < max_frames) {
        waves_update(&waves, &ecs, &world, dt);
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

    waves_start_next_wave(&waves);
    waves.zombies_to_spawn = 4;
    waves.spawn_interval = 0.05f;

    float dt = 1.0f / 120.0f;
    int guard = 0;
    while (guard < 3000 && waves.wave_number == 1 && !waves.between_waves) {
        guard++;
        event_bus_tick(bus, dt);

        waves_update(&waves, &ecs, &world, dt);

        for (uint32_t i = 0; i < ECS_MAX_ENTITIES; i++) {
            if (!ecs.alive[i]) continue;
            if (!(ecs.component_masks[i] & (1u << COMP_ZOMBIE_TAG))) continue;
            Entity b = spawn_test_bullet(&ecs, ecs_get_position(&ecs, i)->pos, player);
            if (b != ECS_NULL_ENTITY) {
                system_collision(&ecs, &world);
            }
        }

        system_cleanup(&ecs, &waves, NULL);
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

    waves_start_next_wave(&waves);
    waves.zombies_to_spawn = DET_MAX_SPAWNS;
    waves.spawn_interval = 0.01f;

    float dt = 1.0f / 120.0f;
    int n = 0;
    for (int i = 0; i < 2000 && n < max_out; i++) {
        waves_update(&waves, &ecs, &world, dt);
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

    PlayerInventory inv;
    weapons_inventory_init(&inv);
    inv.points = 1000;
    weapons_buy_sword(&inv);
    weapons_select(&inv, WEAPON_SWORD);

    /* Fake input: mouse held, aim straight ahead. */
    InputState input;
    input_init(&input);
    input.mouse_buttons[0] = true;
    input.mouse_world_x = spawn.x + 100.0f;
    input.mouse_world_y = spawn.y;

    /* First tick: the system should spawn a sword entity. */
    system_sword(&ecs, &input, &inv, 1.0f / 60.0f);

    Entity sword_entity = ECS_NULL_ENTITY;
    for (uint32_t i = 0; i < ECS_MAX_ENTITIES; i++) {
        if (!ecs.alive[i]) continue;
        if (ecs.component_masks[i] & (1u << COMP_SWORD_TAG)) { sword_entity = i; break; }
    }
    CHECK(sword_entity != ECS_NULL_ENTITY);
    CHECK(ecs_get_sword_tag(&ecs, sword_entity)->angle != 0.0f);

    /* Release mouse: the sword should be destroyed. */
    input.mouse_buttons[0] = false;
    system_sword(&ecs, &input, &inv, 1.0f / 60.0f);

    bool alive = false;
    for (uint32_t i = 0; i < ECS_MAX_ENTITIES; i++) {
        if (!ecs.alive[i]) continue;
        if (ecs.component_masks[i] & (1u << COMP_SWORD_TAG)) alive = true;
    }
    CHECK(!alive);
}

static void test_sword_sweep_hits(void) {
    LOG_INFO("--- Test: Sword sweep hits along the blade ---");
    World ecs;
    ecs_init(&ecs);
    GameWorld world = {0};
    world_init(&world);

    Vec2 spawn = {world.world_pixel_w * 0.5f, world.world_pixel_h * 0.5f};
    Entity player = spawn_player(&ecs, spawn);
    CHECK(player != ECS_NULL_ENTITY);

    PlayerInventory inv;
    weapons_inventory_init(&inv);
    inv.points = 1000;
    weapons_buy_sword(&inv);
    weapons_select(&inv, WEAPON_SWORD);

    /* A zombie sitting mid-blade (inner circle 26 -> outer 58) must be hit
     * by the sweeping blade, not just the single orbital point. */
    Vec2 zpos = vec2_add(spawn, vec2(42.0f, 0.0f));
    Entity zombie = waves_spawn_zombie(&ecs, zpos, THEME_GRASSLAND);
    CHECK(zombie != ECS_NULL_ENTITY);
    float hp_before = ecs_get_health(&ecs, zombie)->current;

    InputState input;
    input_init(&input);
    input.mouse_buttons[0] = true;
    input.mouse_world_x = spawn.x + 100.0f;
    input.mouse_world_y = spawn.y;

    int frames = 0;
    while (frames < 120 && ecs_get_health(&ecs, zombie)->current >= hp_before) {
        system_sword(&ecs, &input, &inv, 1.0f / 60.0f);
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
    InputState input;
    input_init(&input);
    input.keys[SDL_SCANCODE_D] = true;

    system_player_input(&ecs, &input, &cam, 1.0f / 60.0f, NULL);
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

    PlayerInventory inv;
    weapons_inventory_init(&inv);
    inv.points = 1000;
    weapons_buy_launcher(&inv);
    weapons_select(&inv, WEAPON_LAUNCHER);

    InputState input;
    input_init(&input);
    input.mouse_pressed[0] = true;      /* fire on the click edge */
    input.mouse_world_x = ppos.x + 200.0f;
    input.mouse_world_y = ppos.y;

    /* Fire a rocket. */
    system_rockets(&ecs, &input, &inv, &world, 0.0f);
    CHECK(inv.launcher_ammo == LAUNCHER_STARTER_ROCKETS - 1);

    Entity rocket = ECS_NULL_ENTITY;
    for (uint32_t i = 0; i < ECS_MAX_ENTITIES; i++) {
        if (!ecs.alive[i]) continue;
        if (ecs.component_masks[i] & (1u << COMP_ROCKET_TAG)) { rocket = i; break; }
    }
    CHECK(rocket != ECS_NULL_ENTITY);

    /* Advance and collide: rocket pierces through the zombie. */
    input.mouse_pressed[0] = false;
    system_rockets(&ecs, &input, &inv, &world, 0.016f);
    CHECK(ecs_get_health(&ecs, zombie)->current < hp_before);

    /* Move rocket out of world bounds → should be destroyed. */
    *ecs_get_position(&ecs, rocket) = (CPosition){{-100.0f, -100.0f}};
    system_rockets(&ecs, &input, &inv, &world, 0.01f);
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

    PlayerInventory inv;
    weapons_inventory_init(&inv);
    inv.points = 1000;
    weapons_buy_grenade_pack(&inv);
    CHECK(inv.grenades == GRENADES_PER_PACK);
    weapons_select(&inv, WEAPON_GRENADE);

    InputState input;
    input_init(&input);
    input.mouse_pressed[0] = true;
    input.mouse_world_x = ppos.x + 200.0f;
    input.mouse_world_y = ppos.y;

    /* Throw a grenade. */
    system_grenades(&ecs, &input, &inv, 0.0f);
    CHECK(inv.grenades == GRENADES_PER_PACK - 1);

    Entity grenade = ECS_NULL_ENTITY;
    for (uint32_t i = 0; i < ECS_MAX_ENTITIES; i++) {
        if (!ecs.alive[i]) continue;
        if (ecs.component_masks[i] & (1u << COMP_GRENADE_TAG)) { grenade = i; break; }
    }
    CHECK(grenade != ECS_NULL_ENTITY);

    /* Advance the fuse until it detonates. */
    input.mouse_pressed[0] = false;
    for (int frame = 0; frame < 120; frame++) {
        system_grenades(&ecs, &input, &inv, 1.0f / 60.0f);
        if (!ecs_is_alive(&ecs, grenade)) break;
    }
    CHECK(!ecs_is_alive(&ecs, grenade));
    CHECK(ecs_get_health(&ecs, zombie)->current < hp_before);
}

/* ------------------------------------------------------- Map file loading */

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

/* ---------------------------------------------------------------- End Weapons */

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
    test_hud_damage_flash();
    test_entity_limit();
    test_wave_completion();
    test_script_aim_shot();
    test_event_stream();
    test_deterministic_seed();
    test_wave_timeout();
    test_weapons_inventory();
    test_sword_spin();
    test_sword_sweep_hits();
    test_zombie_contact_and_slow();
    test_rocket_damage_and_destruction();
    test_grenade_detonation();
    test_theme_registry();
    test_map_parser();
    test_map_registry_loads();

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