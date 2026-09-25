#include "world/waves.h"
#include "events/event_bus.h"
#include "graphics/sprite.h"
#include "config.h"
#include "core/log.h"
#include <stdlib.h>

#define SPAWN_RING_MIN 400.0f
#define SPAWN_RING_MAX 600.0f

/* Zombies must be able to sense the player from anywhere in the spawn ring,
 * otherwise a spawn at ring-edge leaves them idle forever and the wave
 * soft-locks (playtest finding B1). */
#define ZOMBIE_DETECTION_RANGE (SPAWN_RING_MAX + 100.0f)

/* Safety net: if a wave is still "in progress" after this many seconds it is
 * force-completed so a stuck/unkillable zombie can never soft-lock the game. */
#define WAVE_MAX_DURATION 60.0f

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
    ws->wave_elapsed = 0;

    LOG_INFO("=== WAVE %d STARTED === (%d zombies, interval: %.2fs, difficulty: %.2f)",
             ws->wave_number, ws->zombies_to_spawn, ws->spawn_interval, ws->difficulty_multiplier);
    event_emit(g_events, GE_WAVE_START, ECS_NULL_ENTITY, GEK_NONE,
               0, 0, (float)ws->wave_number, (float)ws->zombies_to_spawn, 0, 0);
}

void waves_on_zombie_killed(WaveSystem *ws) {
    if (!ws) return;
    ws->zombies_alive--;
    ws->total_kills++;
    LOG_DEBUG("Zombie killed (alive: %d, total kills: %d)", ws->zombies_alive, ws->total_kills);
}

/* Find a walkable spawn point in an annulus around the player so the fight
 * comes toward the player instead of at the far map edges. Falls back to a
 * fixed edge point after too many attempts. */
static bool spawn_point_near_player(World *ecs, GameWorld *world, Vec2 *out) {
    Vec2 player_pos = {0, 0};
    bool player_found = false;
    for (uint32_t i = 0; i < ECS_MAX_ENTITIES; i++) {
        if (!ecs->alive[i]) continue;
        if (ecs->component_masks[i] & (1u << COMP_PLAYER_TAG)) {
            player_pos = ecs->positions[i].pos;
            player_found = true;
            break;
        }
    }

    if (!player_found) return false;

    for (int attempt = 0; attempt < 10; attempt++) {
        float angle = (float)(rand() % 360) * M_PI / 180.0f;
        float radius = SPAWN_RING_MIN +
                       (float)(rand() % (int)(SPAWN_RING_MAX - SPAWN_RING_MIN));
        Vec2 candidate = vec2_add(player_pos, vec2_from_angle(angle, radius));

        candidate.x = clampf(candidate.x, 32.0f, world->world_pixel_w - 32.0f);
        candidate.y = clampf(candidate.y, 32.0f, world->world_pixel_h - 32.0f);

        if (world_is_walkable(world, candidate.x, candidate.y)) {
            *out = candidate;
            return true;
        }
    }
    return false;
}

Entity waves_spawn_zombie(World *ecs, Vec2 pos, ThemeID theme) {
    Entity e = ecs_create_entity(ecs);
    if (e == ECS_NULL_ENTITY) return e;

    ecs_add_component(ecs, e, COMP_POSITION);
    ecs_add_component(ecs, e, COMP_VELOCITY);
    ecs_add_component(ecs, e, COMP_HEALTH);
    ecs_add_component(ecs, e, COMP_SPRITE);
    ecs_add_component(ecs, e, COMP_COLLIDER);
    ecs_add_component(ecs, e, COMP_ZOMBIE_TAG);

    *ecs_get_position(ecs, e) = (CPosition){{pos.x, pos.y}};
    *ecs_get_velocity(ecs, e) = (CVelocity){{0, 0}, 130.0f};
    *ecs_get_health(ecs, e) = (CHealth){100.0f, 100.0f};
    *ecs_get_collider(ecs, e) = (CCollider){12.0f, false};
    *ecs_get_zombie_tag(ecs, e) = (CZombieTag){
        .state = ZOMBIE_CHASE,
        .attack_timer = 0,
        .attack_cooldown = 0.75f,
        .detection_range = ZOMBIE_DETECTION_RANGE,
        .attack_range = 30.0f,
        .hurt_timer = 0
    };

    /* Vary zombie colors/sizes; tints resolve through the map's theme. */
    int variant = rand() % 3;
    SDL_FColor color = theme_get(theme)->zombie_tints[variant];

    float size = 10.0f + (float)(rand() % 6);
    Sprite zs = sprite_circle(size, color);
    SDL_Texture *zombie_tex = sprite_tex("textures/entities/zombie.png");
    if (zombie_tex) {
        /* 32px art drawn at 2*size world units matches the previous circle
         * diameter; tint keeps the per-variant palette. */
        zs = sprite_texture(zombie_tex);
        zs.color = color;
    }
    *ecs_get_sprite(ecs, e) = (CSprite){
        .sprite = zs,
        .scale = size / 16.0f,
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

    ws->wave_elapsed += dt;

    /* Soft-lock safety net: if all zombies are spawned but some are still
     * alive well past the expected clear time, force-end the wave so a
     * stuck/unkillable zombie can never stall the game forever (B1). */
    if (ws->wave_elapsed >= WAVE_MAX_DURATION &&
        ws->zombies_spawned >= ws->zombies_to_spawn && ws->zombies_alive > 0) {
        LOG_WARN("Wave %d timed out after %.0fs; force-ending (%d zombies)",
                 ws->wave_number, ws->wave_elapsed, ws->zombies_alive);
        for (uint32_t i = 0; i < ECS_MAX_ENTITIES; i++) {
            if (ecs->alive[i] && (ecs->component_masks[i] & (1u << COMP_ZOMBIE_TAG))) {
                ecs_destroy_entity(ecs, i);
            }
        }
        ws->zombies_alive = 0;
    }

    /* Spawn zombies */
    if (ws->zombies_spawned < ws->zombies_to_spawn) {
        ws->spawn_timer += dt;
        if (ws->spawn_timer >= ws->spawn_interval) {
            ws->spawn_timer = 0;

            /* Spawn in a ring around the player when possible */
            Vec2 spawn_pos;
            if (!spawn_point_near_player(ecs, world, &spawn_pos)) {
                /* fallback: fixed edge point */
                int idx = rand() % ws->spawn_point_count;
                spawn_pos = ws->spawn_points[idx];
                spawn_pos.x += (float)(rand() % 100 - 50);
                spawn_pos.y += (float)(rand() % 100 - 50);
            }

            Entity z = waves_spawn_zombie(ecs, spawn_pos, world->theme);
            if (z != ECS_NULL_ENTITY) {
                /* Scale health and speed with difficulty */
                CHealth *h = ecs_get_health(ecs, z);
                h->max *= ws->difficulty_multiplier;
                h->current = h->max;
                CVelocity *v = ecs_get_velocity(ecs, z);
                v->max_speed *= ws->difficulty_multiplier;
                v->max_speed *= g_zombie_speed_mult;

                ws->zombies_spawned++;
                ws->zombies_alive++;
                event_emit(g_events, GE_ENTITY_SPAWN, z, GEK_ZOMBIE,
                           spawn_pos.x, spawn_pos.y, 0, 0, 0, 0);
            }
        }
    }

    /* Check if wave is complete */
    if (ws->zombies_spawned >= ws->zombies_to_spawn && ws->zombies_alive <= 0) {
        ws->wave_active = false;
        ws->between_waves = true;
        ws->wave_cooldown_timer = 0;
        LOG_INFO("=== WAVE %d COMPLETE === (total kills: %d)", ws->wave_number, ws->total_kills);
        event_emit(g_events, GE_WAVE_END, ECS_NULL_ENTITY, GEK_NONE,
                   0, 0, (float)ws->wave_number, (float)ws->total_kills, 0, 0);
    }
}
