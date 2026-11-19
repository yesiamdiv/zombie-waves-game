#include "world/waves.h"
#include "events/event_bus.h"
#include "config.h"
#include "core/log.h"
#include "graphics/anim.h"
#include "players.h"
#include <stdlib.h>

#define SPAWN_RING_MIN 400.0f
#define SPAWN_RING_MAX 600.0f

/* Zombies must be able to sense the player from anywhere in the spawn ring,
 * otherwise a spawn at ring-edge leaves them idle forever and the wave
 * soft-locks (playtest finding B1). */
#define ZOMBIE_DETECTION_RANGE (SPAWN_RING_MAX + 100.0f)

/* B49 (owner decision 2026-11-20). A wave that outlives this many seconds used
 * to have every surviving zombie force-destroyed on the spot. That was the B1
 * soft-lock safety net, but with no feedback at the moment it fired, so from
 * the player's seat the zombies simply vanished and the next wave started --
 * which is exactly how the owner reported it after the 2026-11-19 playtest.
 *
 * Instead the stragglers now ENRAGE: they hunt the player from anywhere on the
 * map, move much faster and render with a red tint, so the wave resolves
 * through gameplay. Deleting entities was replaced, not merely delayed. */
#define WAVE_MAX_DURATION 60.0f

/* ...but the soft-lock guarantee B1 bought us cannot be given up: an enraged
 * zombie that is genuinely stuck (wedged in geometry, unreachable, invincible)
 * would stall the game forever. So the force-destroy survives as a last-resort
 * backstop at a later deadline. Reaching it means the enrage failed to resolve
 * the wave, which is a bug worth seeing in the log, not normal play. */
#define WAVE_HARD_TIMEOUT 120.0f

/* Enrage tuning. The speed multiplier is what makes a straggler close the
 * distance; the detection range is set beyond any map diagonal so an enraged
 * zombie can never fall back to the wander branch in zombie_ai.c. */
#define ENRAGE_SPEED_MULT 2.5f
#define ENRAGE_DETECTION_RANGE 100000.0f

/* R13/D3: difficulty scales with player count (tune later). Each extra
 * player above the 1st adds a few more zombies to the wave budget and a
 * slightly faster spawn cadence. These are only applied when player_count > 1
 * (see waves_start_next_wave), so single-player stays byte-identical. */
#define WAVE_PER_PLAYER_BONUS 2
#define WAVE_PLAYER_INTERVAL_STEP 0.05f

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

void waves_start_next_wave(WaveSystem *ws, int player_count) {
    ws->wave_number++;
    ws->zombies_per_wave = 5 + ws->wave_number * 3;
    ws->zombies_to_spawn = ws->zombies_per_wave;
    ws->zombies_spawned = 0;
    ws->spawn_timer = 0;
    ws->spawn_interval = fmaxf(0.3f, 2.0f - ws->wave_number * 0.1f);
    /* R13/D3: difficulty scales with player count. Larger groups face more
     * zombies and a slightly faster spawn cadence, but ONLY for player_count > 1:
     * single-player (player_count == 1) MUST be byte-identical to the legacy
     * budget above, so SP determinism (D1/D5) is preserved exactly. */
    if (player_count > 1) {
        ws->zombies_per_wave += (player_count - 1) * WAVE_PER_PLAYER_BONUS;
        ws->zombies_to_spawn = ws->zombies_per_wave;
        ws->spawn_interval = fmaxf(0.3f, ws->spawn_interval -
                                   (player_count - 1) * WAVE_PLAYER_INTERVAL_STEP);
    }
    ws->wave_active = true;
    ws->between_waves = false;
    ws->difficulty_multiplier = 1.0f + (ws->wave_number - 1) * 0.15f;
    ws->wave_elapsed = 0;
    ws->enrage_applied = false;  /* B49: per-wave, not per-run */

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

/* Find a walkable spawn point in an annulus around the NEAREST alive player
 * (to the world center) so the fight comes toward whoever is actually
 * fighting instead of the far map edges. When no slot table is provided,
 * falls back to the nearest player-tagged entity (tests / pre-slot callers).
 * Falls back to a fixed edge point after too many attempts. */
static bool spawn_point_near_player(World *ecs, GameWorld *world,
                                    Player *players, int player_count,
                                    Vec2 *out) {
    Vec2 center = vec2(world->world_pixel_w * 0.5f, world->world_pixel_h * 0.5f);
    Vec2 player_pos = {0, 0};
    bool player_found = false;
    float best_d = 0.0f;

    if (players && player_count > 0) {
        for (int s = 0; s < player_count; s++) {
            const Player *p = &players[s];
            if (!p->in_use || !p->alive) continue;
            if (p->entity == ECS_NULL_ENTITY || !ecs_is_alive(ecs, p->entity)) continue;
            if (!(ecs->component_masks[p->entity] & (1u << COMP_POSITION))) continue;
            Vec2 pos = ecs_get_position(ecs, p->entity)->pos;
            float d = vec2_distance(center, pos);
            if (!player_found || d < best_d) {
                player_pos = pos;
                best_d = d;
                player_found = true;
            }
        }
    } else {
        for (uint32_t i = 0; i < ECS_MAX_ENTITIES; i++) {
            if (!ecs->alive[i]) continue;
            if (!(ecs->component_masks[i] & (1u << COMP_PLAYER_TAG))) continue;
            Vec2 pos = ecs->positions[i].pos;
            float d = vec2_distance(center, pos);
            if (!player_found || d < best_d) {
                player_pos = pos;
                best_d = d;
                player_found = true;
            }
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
    ecs_add_component(ecs, e, COMP_ANIMATION);

    *ecs_get_position(ecs, e) = (CPosition){{pos.x, pos.y}};
    *ecs_get_velocity(ecs, e) = (CVelocity){{0, 0}, 80.0f};
    *ecs_get_health(ecs, e) = (CHealth){100.0f, 100.0f};
    *ecs_get_collider(ecs, e) = (CCollider){12.0f, false};
    *ecs_get_zombie_tag(ecs, e) = (CZombieTag){
        .state = ZOMBIE_CHASE,
        .attack_timer = 0,
        .attack_cooldown = 1.0f,
        .detection_range = ZOMBIE_DETECTION_RANGE,
        .attack_range = 20.0f,
        .hurt_timer = 0,
        .last_hit_by = ECS_NULL_ENTITY
    };

    /* Vary zombie colors/sizes; tints resolve through the map's theme. */
    int variant = rand() % 3;
    SDL_FColor color = theme_get(theme)->zombie_tints[variant];

    float size = 10.0f + (float)(rand() % 6);
    /* R13 merge fix (B15): the merge dropped the zombie texture, leaving a flat
     * circle. Restore it; the theme tint keeps the per-variant palette. */
    Sprite zs = sprite_circle(size, color);
    /* The walk cycle's frame 0 IS the idle zombie pose, so a zombie starts on
     * the picture it always had and does not pop when it starts walking.
     * anim_walk_texture() is the one place that knows which file a frame is;
     * the plain zombie.png is the fallback for a build without the cycle. */
    SDL_Texture *zombie_tex = anim_walk_texture(0);
    if (!zombie_tex) zombie_tex = sprite_tex("textures/entities/zombie.png");
    if (zombie_tex) {
        /* 32px art at 2*size world units matches the old circle diameter. */
        zs = sprite_texture(zombie_tex);
        zs.color = color;
    }
    *ecs_get_sprite(ecs, e) = (CSprite){
        .sprite = zs,
        .scale = size / 16.0f,
        .base_alpha = 1.0f
    };
    /* P1a: stagger the horde. Every zombie used to start on frame 0 with the
     * same frame period, so a whole wave marched in perfect unison - the
     * strongest "this is fake" tell in the game. The STARTING FRAME is hashed
     * from the entity's slot; the period is unchanged, so the offset lasts for
     * the zombie's whole life. Display-only: the frame is what the snapshot
     * names, so a client inherits the stagger without a wire field. No rand() -
     * the seed sequence is load-bearing for single-player determinism. */
    uint32_t phase = e * 2654435761u;
    phase ^= phase >> 15;
    phase *= 0x85ebca6bu;
    phase ^= phase >> 13;
    *ecs_get_animation(ecs, e) = (CAnimation){
        .current_frame = (int)(phase % (uint32_t)ANIM_WALK_FRAMES),
        .frame_timer = 0.0f,
        .frame_duration = ANIM_FRAME_PERIOD,
        .total_frames = ANIM_WALK_FRAMES
    };
    ecs_get_collider(ecs, e)->radius = size;

    return e;
}

void waves_update(WaveSystem *ws, World *ecs, GameWorld *world,
                  Player *players, int player_count, float dt) {
    if (ws->between_waves) {
        ws->wave_cooldown_timer += dt;
        if (ws->wave_cooldown_timer >= ws->wave_cooldown) {
            ws->wave_cooldown_timer = 0;
            waves_start_next_wave(ws, player_count);
        }
        return;
    }

    ws->wave_elapsed += dt;

    /* B49: a wave that drags on past WAVE_MAX_DURATION enrages its survivors
     * rather than deleting them. Only once per wave, and only once every
     * zombie the wave owed has actually spawned -- enraging mid-spawn would
     * turn a slow wave into an unwinnable one. */
    if (!ws->enrage_applied && ws->wave_elapsed >= WAVE_MAX_DURATION &&
        ws->zombies_spawned >= ws->zombies_to_spawn && ws->zombies_alive > 0) {
        ws->enrage_applied = true;
        int enraged = 0;
        for (uint32_t i = 0; i < ECS_MAX_ENTITIES; i++) {
            if (!ecs->alive[i]) continue;
            if (!(ecs->component_masks[i] & (1u << COMP_ZOMBIE_TAG))) continue;
            CZombieTag *zt = ecs_get_zombie_tag(ecs, i);
            zt->enraged = true;
            /* Hunt from anywhere: zombie_ai.c only chases while
             * dist < detection_range, and wanders otherwise. */
            zt->detection_range = ENRAGE_DETECTION_RANGE;
            if (ecs->component_masks[i] & (1u << COMP_VELOCITY)) {
                ecs_get_velocity(ecs, i)->max_speed *= ENRAGE_SPEED_MULT;
            }
            enraged++;
        }
        LOG_WARN("Wave %d outlived %.0fs; ENRAGING %d straggler(s) instead of "
                 "force-destroying them (B49)",
                 ws->wave_number, ws->wave_elapsed, enraged);
        /* Appended to the event stream so the behaviour is verifiable
         * headlessly: ia = how many zombies were enraged. */
        event_emit(g_events, GE_WAVE_ENRAGE, ECS_NULL_ENTITY, GEK_ZOMBIE,
                   0.0f, 0.0f, ws->wave_elapsed, 0.0f, enraged, ws->wave_number);
    }

    /* Last-resort backstop: the enrage is supposed to end the wave through
     * gameplay. If it has not by WAVE_HARD_TIMEOUT, something is genuinely
     * stuck and the original B1 force-destroy applies so the game cannot
     * soft-lock. This path firing is a bug signal, hence LOG_ERROR. */
    if (ws->wave_elapsed >= WAVE_HARD_TIMEOUT &&
        ws->zombies_spawned >= ws->zombies_to_spawn && ws->zombies_alive > 0) {
        LOG_ERROR("Wave %d still alive %.0fs after the enrage; force-ending "
                  "(%d zombies) - the enrage failed to resolve the wave",
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

            /* Spawn in a ring around the nearest alive player when possible */
            Vec2 spawn_pos;
            if (!spawn_point_near_player(ecs, world, players, player_count, &spawn_pos)) {
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
