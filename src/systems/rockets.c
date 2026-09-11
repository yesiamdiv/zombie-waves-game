#include "systems/systems.h"
#include "events/event_bus.h"
#include "core/log.h"

#define LAUNCHER_SPEED  520.0f
#define LAUNCHER_DAMAGE 120.0f
#define ROCKET_LIFETIME 3.0f
#define ROCKET_RADIUS   5.0f

static Entity find_player(World *ecs) {
    for (uint32_t i = 0; i < ECS_MAX_ENTITIES; i++) {
        if (!ecs->alive[i]) continue;
        if (ecs->component_masks[i] & (1u << COMP_PLAYER_TAG)) return i;
    }
    return ECS_NULL_ENTITY;
}

static void fire_rocket(World *ecs, Entity player, const InputState *input) {
    CPosition *ppos = ecs_get_position(ecs, player);

    Vec2 dir = vec2_sub(vec2(input->mouse_world_x, input->mouse_world_y),
                        ppos->pos);
    if (vec2_length(dir) < 0.001f) dir = vec2(1, 0);
    else dir = vec2_normalize(dir);

    Entity r = ecs_create_entity(ecs);
    if (r == ECS_NULL_ENTITY) return;

    ecs_add_component(ecs, r, COMP_POSITION);
    ecs_add_component(ecs, r, COMP_VELOCITY);
    ecs_add_component(ecs, r, COMP_SPRITE);
    ecs_add_component(ecs, r, COMP_COLLIDER);
    ecs_add_component(ecs, r, COMP_ROCKET_TAG);

    Vec2 spawn = vec2_add(ppos->pos, vec2_scale(dir, 22.0f));
    *ecs_get_position(ecs, r) = (CPosition){{spawn.x, spawn.y}};
    *ecs_get_velocity(ecs, r) = (CVelocity){
        .vel = vec2_scale(dir, LAUNCHER_SPEED),
        .max_speed = LAUNCHER_SPEED
    };
    *ecs_get_collider(ecs, r) = (CCollider){ROCKET_RADIUS, true};
    *ecs_get_rocket_tag(ecs, r) = (CRocketTag){
        .owner = player,
        .damage = LAUNCHER_DAMAGE,
        .lifetime = ROCKET_LIFETIME
    };
    *ecs_get_sprite(ecs, r) = (CSprite){
        .sprite = sprite_circle(ROCKET_RADIUS, COLOR_ORANGE),
        .scale = 1.0f,
        .base_alpha = 1.0f
    };

    event_emit(g_events, GE_PLAYER_SHOT, r, GEK_ROCKET,
               spawn.x, spawn.y, dir.x, dir.y, 0, 0);
}

/* Damages every zombie currently overlapping the rocket (piercing: it does
 * not stop on contact, it keeps flying until it leaves the world). */
static void damage_overlapping(World *ecs, Entity rocket, CRocketTag *tag) {
    Vec2 rpos = ecs->positions[rocket].pos;

    for (uint32_t i = 0; i < ECS_MAX_ENTITIES; i++) {
        if (!ecs->alive[i]) continue;
        if (!(ecs->component_masks[i] & (1u << COMP_ZOMBIE_TAG))) continue;
        if (!(ecs->component_masks[i] & (1u << COMP_POSITION))) continue;
        if (!(ecs->component_masks[i] & (1u << COMP_HEALTH))) continue;

        Vec2 zpos = ecs->positions[i].pos;
        float zradius = ecs_has_component(ecs, i, COMP_COLLIDER)
                            ? ecs->colliders[i].radius : 12.0f;
        if (vec2_distance(rpos, zpos) > ROCKET_RADIUS + zradius) continue;

        CHealth *hp = &ecs->healths[i];
        float prev = hp->current;
        hp->current = prev - tag->damage;
        event_emit(g_events, GE_DAMAGE, i, GEK_ZOMBIE,
                   zpos.x, zpos.y, tag->damage, hp->current > 0 ? hp->current : 0,
                   (int)prev, 0);

        if (ecs_has_component(ecs, i, COMP_VELOCITY)) {
            Vec2 d = vec2_sub(zpos, rpos);
            float len = vec2_length(d);
            if (len < 0.001f) d = vec2(1, 0); else d = vec2_scale(d, 1.0f / len);
            CVelocity *vel = &ecs->velocities[i];
            vel->vel = vec2_add(vel->vel, vec2_scale(d, 60.0f));
        }

        CZombieTag *ztag = &ecs->zombie_tags[i];
        ztag->hurt_timer = 0.15f;
        ztag->state = ZOMBIE_HURT;
    }
}

void system_rockets(World *ecs, InputState *input, PlayerInventory *inv,
                    GameWorld *world, float dt) {
    Entity player = find_player(ecs);
    if (player == ECS_NULL_ENTITY || !input || !inv) return;

    /* Fire on the click edge, consuming one rocket. */
    if (input->mouse_pressed[0] &&
        inv->unlocked[WEAPON_LAUNCHER] &&
        inv->current == WEAPON_LAUNCHER &&
        inv->launcher_ammo > 0) {
        inv->launcher_ammo--;
        fire_rocket(ecs, player, input);
    }

    for (uint32_t i = 0; i < ECS_MAX_ENTITIES; i++) {
        if (!ecs->alive[i]) continue;
        if (!(ecs->component_masks[i] & (1u << COMP_ROCKET_TAG))) continue;

        CRocketTag *tag = &ecs->rocket_tags[i];
        Vec2 pos = ecs->positions[i].pos;

        tag->lifetime -= dt;
        if (tag->lifetime <= 0 || !world_in_bounds(world, pos.x, pos.y)) {
            event_emit(g_events, GE_ENTITY_DEATH, i, GEK_ROCKET,
                       pos.x, pos.y, 0, 0, 0, 0);
            ecs_destroy_entity(ecs, i);
            continue;
        }

        damage_overlapping(ecs, i, tag);
    }
}