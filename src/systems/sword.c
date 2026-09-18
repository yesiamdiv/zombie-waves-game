#include "systems/systems.h"
#include "events/event_bus.h"
#include "graphics/sprite.h"
#include "core/log.h"
#include <math.h>

#define SWORD_RADIUS       26.0f   /* inner circle: handle pivot */
#define SWORD_TIP_RADIUS   58.0f   /* outer circle: blade tip */
#define SWORD_WIDTH        8.0f
#define SWORD_SPIN_SPEED   5.0f
#define SWORD_DAMAGE       55.0f
#define SWORD_HIT_INTERVAL 0.15f   /* per-zombie re-hit cooldown */

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

static Entity spawn_sword(World *ecs, Entity player, float aim_angle) {
    Entity s = ecs_create_entity(ecs);
    if (s == ECS_NULL_ENTITY) return s;

    ecs_add_component(ecs, s, COMP_POSITION);
    ecs_add_component(ecs, s, COMP_SPRITE);
    ecs_add_component(ecs, s, COMP_SWORD_TAG);

    Vec2 ppos = ecs_get_position(ecs, player)->pos;
    Vec2 handle = vec2_add(ppos, vec2_from_angle(aim_angle, SWORD_RADIUS));
    *ecs_get_position(ecs, s) = (CPosition){{handle.x, handle.y}};
    *ecs_get_sword_tag(ecs, s) = (CSwordTag){
        .owner = player,
        .radius = SWORD_RADIUS,
        .outer_radius = SWORD_TIP_RADIUS,
        .angle = aim_angle,
        .spin_speed = SWORD_SPIN_SPEED,
        .damage = SWORD_DAMAGE,
        .hit_interval = SWORD_HIT_INTERVAL
    };
    Sprite blade = sprite_rect(SWORD_WIDTH, SWORD_TIP_RADIUS - SWORD_RADIUS,
                               COLOR_LIGHT_BLUE);
    blade.texture = sprite_tex("textures/entities/sword_blade.png");
    if (blade.texture) {
        blade.shape = SPRITE_SHAPE_TEXTURE;
        blade.color = COLOR_WHITE;
    }
    *ecs_get_sprite(ecs, s) = (CSprite){
        .sprite = blade,
        .scale = 1.0f,
        .base_alpha = 0.95f
    };

    event_emit(g_events, GE_ENTITY_SPAWN, s, GEK_SWORD,
               handle.x, handle.y, 0, 0, 0, 0);
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
        sword = spawn_sword(ecs, player, aim_angle);
        if (sword == ECS_NULL_ENTITY) return;
        LOG_DEBUG("Sword deployed (angle: %.2f)", aim_angle);
    }

    CSwordTag *tag = ecs_get_sword_tag(ecs, sword);
    tag->angle += tag->spin_speed * dt;

    /* Handle tracks the inner circle, the tip the outer circle. The entity
     * position is the handle point; the renderer pivots the blade there. */
    Vec2 handle = vec2_add(ppos->pos,
                           vec2_from_angle(tag->angle, tag->radius));
    Vec2 tip = vec2_add(ppos->pos,
                        vec2_from_angle(tag->angle, tag->outer_radius));
    *ecs_get_position(ecs, sword) = (CPosition){{handle.x, handle.y}};

    /* Segment-vs-circle hit test against the full blade. Cooldowns are tracked
     * per zombie so a single sweep can slash through several at once. */
    float blade_half_width = SWORD_WIDTH * 0.5f;
    for (uint32_t i = 0; i < ECS_MAX_ENTITIES; i++) {
        if (!ecs->alive[i]) continue;
        if (!(ecs->component_masks[i] & (1u << COMP_ZOMBIE_TAG))) continue;
        if (!(ecs->component_masks[i] & (1u << COMP_POSITION))) continue;
        if (!(ecs->component_masks[i] & (1u << COMP_HEALTH))) continue;

        CZombieTag *ztag = ecs_get_zombie_tag(ecs, i);
        ztag->sword_hit_timer -= dt;
        if (ztag->sword_hit_timer > 0) continue;

        Vec2 zpos = ecs->positions[i].pos;
        float zradius = ecs_has_component(ecs, i, COMP_COLLIDER)
                            ? ecs->colliders[i].radius : 12.0f;
        Vec2 closest = vec2_closest_on_segment(zpos, handle, tip);
        if (vec2_distance(zpos, closest) < blade_half_width + zradius) {
            sword_hit_zombie(ecs, i, tag->damage, closest);
            ztag->sword_hit_timer = tag->hit_interval;
        }
    }
}