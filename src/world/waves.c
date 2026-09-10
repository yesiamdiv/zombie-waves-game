#include "world/waves.h"
#include "core/log.h"
#include <stdlib.h>

void waves_init(WaveSystem *ws, GameWorld *world) {
    memset(ws, 0, sizeof(WaveSystem));

    ws->wave_number = 0;
    ws->zombies_alive = 0;
    ws->zombies_spawned = 0;
    ws->zombies_to_spawn = 0;
    ws->zombies_per_wave = 5;
    ws->spawn_timer = 0;
    ws->spawn_interval = 2.0f;
    ws->wave_cooldown = 3.0f;
    ws->wave_cooldown_timer = 0;
    ws->wave_active = false;
    ws->between_waves = true;
    ws->total_kills = 0;
    ws->difficulty_multiplier = 1.0f;

    /* Generate spawn points around the edges */
    ws->spawn_point_count = 8;
    float margin = 200.0f;
    ws->spawn_points[0] = vec2(margin, margin);
    ws->spawn_points[1] = vec2(world->world_pixel_w - margin, margin);
    ws->spawn_points[2] = vec2(margin, world->world_pixel_h - margin);
    ws->spawn_points[3] = vec2(world->world_pixel_w - margin, world->world_pixel_h - margin);
    ws->spawn_points[4] = vec2(world->world_pixel_w * 0.5f, margin);
    ws->spawn_points[5] = vec2(world->world_pixel_w * 0.5f, world->world_pixel_h - margin);
    ws->spawn_points[6] = vec2(margin, world->world_pixel_h * 0.5f);
    ws->spawn_points[7] = vec2(world->world_pixel_w - margin, world->world_pixel_h * 0.5f);

    LOG_INFO("Wave system initialized");
}

void waves_start_next_wave(WaveSystem *ws) {
    ws->wave_number++;
    ws->zombies_per_wave = 5 + ws->wave_number * 3;
    ws->zombies_to_spawn = ws->zombies_per_wave;
    ws->zombies_spawned = 0;
    ws->spawn_timer = 0;
    ws->spawn_interval = fmaxf(0.3f, 2.0f - ws->wave_number * 0.1f);
    ws->wave_active = true;
    ws->between_waves = false;
    ws->difficulty_multiplier = 1.0f + (ws->wave_number - 1) * 0.15f;

    LOG_INFO("=== WAVE %d STARTED === (%d zombies, interval: %.2fs, difficulty: %.2f)",
             ws->wave_number, ws->zombies_to_spawn, ws->spawn_interval, ws->difficulty_multiplier);
}

void waves_on_zombie_killed(WaveSystem *ws) {
    ws->zombies_alive--;
    ws->total_kills++;
    LOG_DEBUG("Zombie killed (alive: %d, total kills: %d)", ws->zombies_alive, ws->total_kills);
}

Entity waves_spawn_zombie(World *ecs, Vec2 pos) {
    Entity e = ecs_create_entity(ecs);
    if (e == ECS_NULL_ENTITY) return e;

    ecs_add_component(ecs, e, COMP_POSITION);
    ecs_add_component(ecs, e, COMP_VELOCITY);
    ecs_add_component(ecs, e, COMP_HEALTH);
    ecs_add_component(ecs, e, COMP_SPRITE);
    ecs_add_component(ecs, e, COMP_COLLIDER);
    ecs_add_component(ecs, e, COMP_ZOMBIE_TAG);

    *ecs_get_position(ecs, e) = (CPosition){{pos.x, pos.y}};
    *ecs_get_velocity(ecs, e) = (CVelocity){{0, 0}, 80.0f};
    *ecs_get_health(ecs, e) = (CHealth){100.0f, 100.0f};
    *ecs_get_collider(ecs, e) = (CCollider){12.0f, false};
    *ecs_get_zombie_tag(ecs, e) = (CZombieTag){
        .state = ZOMBIE_CHASE,
        .attack_timer = 0,
        .attack_cooldown = 1.0f,
        .detection_range = 400.0f,
        .attack_range = 20.0f,
        .hurt_timer = 0
    };

    /* Vary zombie colors and sizes slightly */
    float hue = (float)(rand() % 3) / 3.0f;
    SDL_FColor color;
    if (hue < 0.33f) {
        color = (SDL_FColor){0.3f, 0.6f, 0.2f, 1.0f};  /* green zombie */
    } else if (hue < 0.66f) {
        color = (SDL_FColor){0.5f, 0.4f, 0.2f, 1.0f};  /* brown zombie */
    } else {
        color = (SDL_FColor){0.4f, 0.2f, 0.3f, 1.0f};  /* purple zombie */
    }

    float size = 10.0f + (float)(rand() % 6);
    *ecs_get_sprite(ecs, e) = (CSprite){
        .sprite = sprite_circle(size, color),
        .scale = 1.0f,
        .base_alpha = 1.0f
    };
    ecs_get_collider(ecs, e)->radius = size;

    return e;
}

void waves_update(WaveSystem *ws, World *ecs, GameWorld *world, float dt) {
    if (ws->between_waves) {
        ws->wave_cooldown_timer += dt;
        if (ws->wave_cooldown_timer >= ws->wave_cooldown) {
            ws->wave_cooldown_timer = 0;
            waves_start_next_wave(ws);
        }
        return;
    }

    /* Spawn zombies */
    if (ws->zombies_spawned < ws->zombies_to_spawn) {
        ws->spawn_timer += dt;
        if (ws->spawn_timer >= ws->spawn_interval) {
            ws->spawn_timer = 0;

            /* Pick random spawn point */
            int idx = rand() % ws->spawn_point_count;
            Vec2 spawn_pos = ws->spawn_points[idx];
            spawn_pos.x += (float)(rand() % 100 - 50);
            spawn_pos.y += (float)(rand() % 100 - 50);

            /* Make sure spawn is walkable */
            if (world_is_walkable(world, spawn_pos.x, spawn_pos.y)) {
                Entity z = waves_spawn_zombie(ecs, spawn_pos);
                if (z != ECS_NULL_ENTITY) {
                    /* Scale health with difficulty */
                    CHealth *h = ecs_get_health(ecs, z);
                    h->max *= ws->difficulty_multiplier;
                    h->current = h->max;

                    /* Scale speed */
                    CVelocity *v = ecs_get_velocity(ecs, z);
                    v->max_speed *= ws->difficulty_multiplier;

                    ws->zombies_spawned++;
                    ws->zombies_alive++;
                }
            }
        }
    }

    /* Check if wave is complete */
    if (ws->zombies_spawned >= ws->zombies_to_spawn && ws->zombies_alive <= 0) {
        ws->wave_active = false;
        ws->between_waves = true;
        ws->wave_cooldown_timer = 0;
        LOG_INFO("=== WAVE %d COMPLETE === (total kills: %d)", ws->wave_number, ws->total_kills);
    }
}
