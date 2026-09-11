#include "systems/systems.h"
#include "events/event_bus.h"
#include "core/log.h"
#include <math.h>

#define SWORD_RADIUS       30.0f
#define SWORD_SPIN_SPEED   5.0f
#define SWORD_DAMAGE       55.0f
#define SWORD_HIT_INTERVAL 0.2f
#define BLADE_RADIUS       13.0f

/* Damage a zombie like the bullet path in system_collision (same knockback
 * and hurt-state so the render/audit behavior stays consistent). */
static void sword_hit_zombie(World *ecs, Entity zombie, float dmg, Vec2 origin) {
    CHealth *hp = ecs_get_health(ecs, zombie);
    float prev = hp->current;
    hp->current = prev - dmg;

    Vec2 zpos = ecs_get_position(ecs, zombie)->pos;
    event_emit(g_events, GE_DAMAGE, zombie, GEK_ZOMBIE,
               zpos.x, zpos.y, dmg, hp->current > 0 ? hp->current : 0,
               (int)prev, 0);

    if (ecs_has_component(ecs, zombie, COMP_VELOCITY)) {
        Vec2 dir = vec2_sub(zpos, origin);
        float len = vec2_length(dir);
        if (len < 0.001f) dir = vec2(1, 0); else dir = vec2_scale(dir, 1.0f / len);
        CVelocity *vel = ecs_get_velocity(ecs, zombie);
        vel->vel = vec2_add(vel->vel, vec2_scale(dir, 180.0f));
    }

    CZombieTag *ztag = ecs_get_zombie_tag(ecs, zombie);
    ztag->hurt_timer = 0.15f;
    ztag->state = ZOMBIE_HURT;
}

static Entity find_sword(World *ecs) {
    for (uint32_t i = 0; i < ECS_MAX_ENTITIES; i++) {
        if (!ecs->alive[i]) continue;
        if (ecs->component_masks[i] & (1u << COMP_SWORD_TAG)) return i;
    }
    return ECS_NULL_ENTITY;
}

static Entity spawn_sword(World *ecs, Entity player, const InputState *input,
                          float aim_angle) {
    Entity s = ecs_create_entity(ecs);
    if (s == ECS_NULL_ENTITY) return s;

    ecs_add_component(ecs, s, COMP_POSITION);
    ecs_add_component(ecs, s, COMP_SPRITE);
    ecs_add_component(ecs, s, COMP_SWORD_TAG);

    Vec2 ppos = ecs_get_position(ecs, player)->pos;
    *ecs_get_position(ecs, s) = (CPosition){{ppos.x, ppos.y}};
    *ecs_get_sword_tag(ecs, s) = (CSwordTag){
        .owner = player,
        .radius = SWORD_RADIUS,
        .angle = aim_angle,
        .spin_speed = SWORD_SPIN_SPEED,
        .damage = SWORD_DAMAGE,
        .hit_timer = 0,
        .hit_interval = SWORD_HIT_INTERVAL
    };
    *ecs_get_sprite(ecs, s) = (CSprite){
        .sprite = sprite_rect(7.0f, 26.0f, COLOR_LIGHT_BLUE),
        .scale = 1.0f,
        .base_alpha = 0.95f
    };

    event_emit(g_events, GE_ENTITY_SPAWN, s, GEK_SWORD,
               ppos.x, ppos.y, 0, 0, 0, 0);
    (void)input;
    return s;
}

void system_sword(World *ecs, InputState *input, const PlayerInventory *inv, float dt) {
    Entity player = ECS_NULL_ENTITY;
    for (uint32_t i = 0; i < ECS_MAX_ENTITIES; i++) {
        if (!ecs->alive[i]) continue;
        if (ecs->component_masks[i] & (1u << COMP_PLAYER_TAG)) { player = i; break; }
    }
    if (player == ECS_NULL_ENTITY) {
        Entity s = find_sword(ecs);
        if (s != ECS_NULL_ENTITY) ecs_destroy_entity(ecs, s);
        return;
    }

    bool spinning = input && inv &&
                    inv->unlocked[WEAPON_SWORD] &&
                    inv->current == WEAPON_SWORD &&
                    input->mouse_buttons[0];

    Entity sword = find_sword(ecs);

    if (!spinning) {
        if (sword != ECS_NULL_ENTITY) {
            ecs_destroy_entity(ecs, sword);
            LOG_DEBUG("Sword released");
        }
        return;
    }

    CPosition *ppos = ecs_get_position(ecs, player);
    if (sword == ECS_NULL_ENTITY) {
        Vec2 aim = vec2(input->mouse_world_x, input->mouse_world_y);
        float aim_angle = atan2f(aim.y - ppos->pos.y, aim.x - ppos->pos.x);
        sword = spawn_sword(ecs, player, input, aim_angle);
        if (sword == ECS_NULL_ENTITY) return;
        LOG_DEBUG("Sword deployed (angle: %.2f)", aim_angle);
    }

    CSwordTag *tag = ecs_get_sword_tag(ecs, sword);
    tag->angle += tag->spin_speed * dt;
    tag->hit_timer -= dt;

    Vec2 blade_pos = vec2_add(ppos->pos,
                              vec2_from_angle(tag->angle, tag->radius));
    *ecs_get_position(ecs, sword) = (CPosition){{blade_pos.x, blade_pos.y}};

    if (tag->hit_timer > 0) return;

    float blade_radius = BLADE_RADIUS;
    for (uint32_t i = 0; i < ECS_MAX_ENTITIES; i++) {
        if (!ecs->alive[i]) continue;
        if (!(ecs->component_masks[i] & (1u << COMP_ZOMBIE_TAG))) continue;
        if (!(ecs->component_masks[i] & (1u << COMP_POSITION))) continue;
        if (!(ecs->component_masks[i] & (1u << COMP_HEALTH))) continue;

        Vec2 zpos = ecs->positions[i].pos;
        float zradius = ecs_has_component(ecs, i, COMP_COLLIDER)
                            ? ecs->colliders[i].radius : 12.0f;
        if (vec2_distance(blade_pos, zpos) < blade_radius + zradius) {
            sword_hit_zombie(ecs, i, tag->damage, blade_pos);
            tag->hit_timer = tag->hit_interval;
            break;
        }
    }
}