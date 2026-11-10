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

/* Starter knife: a shorter, faster, weaker version of the same sweeping
 * blade. It reuses the sword's art (scaled) and orbit mechanism, so the
 * starter kit adds no texture and no second melee system. */
#define KNIFE_RADIUS       10.0f
#define KNIFE_TIP_RADIUS   30.0f
#define KNIFE_WIDTH        6.0f
#define KNIFE_SPIN_SPEED   7.0f
#define KNIFE_DAMAGE       22.0f
#define KNIFE_HIT_INTERVAL 0.12f

typedef struct {
    float radius;
    float outer_radius;
    float width;
    float spin_speed;
    float damage;
    float hit_interval;
    const char *deploy_msg;
} MeleeTuning;

static MeleeTuning sword_tuning(void) {
    return (MeleeTuning){SWORD_RADIUS, SWORD_TIP_RADIUS, SWORD_WIDTH,
                         SWORD_SPIN_SPEED, SWORD_DAMAGE, SWORD_HIT_INTERVAL,
                         "Sword deployed"};
}

static MeleeTuning knife_tuning(void) {
    return (MeleeTuning){KNIFE_RADIUS, KNIFE_TIP_RADIUS, KNIFE_WIDTH,
                         KNIFE_SPIN_SPEED, KNIFE_DAMAGE, KNIFE_HIT_INTERVAL,
                         "Knife deployed"};
}

/* Damage a zombie like the bullet path in system_collision (same knockback
 * and hurt-state so the render/audit behavior stays consistent). `owner` is
 * credited with the kill if the zombie dies. */
static void sword_hit_zombie(World *ecs, Entity zombie, Entity owner,
                             float dmg, Vec2 origin) {
    CHealth *hp = ecs_get_health(ecs, zombie);
    float prev = hp->current;
    hp->current = prev - dmg;

    ecs_get_zombie_tag(ecs, zombie)->last_hit_by = owner;

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

static Entity find_sword(World *ecs, Entity owner) {
    for (uint32_t i = 0; i < ECS_MAX_ENTITIES; i++) {
        if (!ecs->alive[i]) continue;
        if (!(ecs->component_masks[i] & (1u << COMP_SWORD_TAG))) continue;
        if (ecs->sword_tags[i].owner != owner) continue;
        return i;
    }
    return ECS_NULL_ENTITY;
}

static Entity spawn_sword(World *ecs, Entity player, float aim_angle,
                          const MeleeTuning *t) {
    Entity s = ecs_create_entity(ecs);
    if (s == ECS_NULL_ENTITY) return s;

    ecs_add_component(ecs, s, COMP_POSITION);
    ecs_add_component(ecs, s, COMP_SPRITE);
    ecs_add_component(ecs, s, COMP_SWORD_TAG);

    Vec2 ppos = ecs_get_position(ecs, player)->pos;
    Vec2 handle = vec2_add(ppos, vec2_from_angle(aim_angle, t->radius));
    *ecs_get_position(ecs, s) = (CPosition){{handle.x, handle.y}};
    *ecs_get_sword_tag(ecs, s) = (CSwordTag){
        .owner = player,
        .radius = t->radius,
        .outer_radius = t->outer_radius,
        .angle = aim_angle,
        .spin_speed = t->spin_speed,
        .damage = t->damage,
        .hit_interval = t->hit_interval
    };
    Sprite blade = sprite_rect(t->width, t->outer_radius - t->radius,
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

void system_sword(World *ecs, Player *p, float dt) {
    if (!p || !p->in_use || !p->alive) return;
    Entity player = p->entity;
    if (player == ECS_NULL_ENTITY || !ecs_is_alive(ecs, player)) return;

    InputState *input = &p->input;
    PlayerInventory *inv = &p->inventory;

    /* Both the purchasable sword and the always-unlocked starter knife use the
     * same orbit. Only one can be equipped at a time. */
    WeaponType melee = WEAPON_COUNT;
    if (inv->unlocked[WEAPON_SWORD] && inv->current == WEAPON_SWORD) {
        melee = WEAPON_SWORD;
    } else if (inv->unlocked[WEAPON_KNIFE] && inv->current == WEAPON_KNIFE) {
        melee = WEAPON_KNIFE;
    }
    bool spinning = melee != WEAPON_COUNT && input->mouse_buttons[0];
    MeleeTuning tune = (melee == WEAPON_KNIFE) ? knife_tuning()
                                               : sword_tuning();

    Entity sword = find_sword(ecs, player);

    /* Swapping sword <-> knife while the button is held must not keep the old
     * blade's reach: destroy it so the next block respawns with the new size. */
    if (sword != ECS_NULL_ENTITY &&
        ecs_get_sword_tag(ecs, sword)->radius != tune.radius) {
        ecs_destroy_entity(ecs, sword);
        sword = ECS_NULL_ENTITY;
    }

    if (!spinning) {
        if (sword != ECS_NULL_ENTITY) {
            ecs_destroy_entity(ecs, sword);
            LOG_DEBUG("Melee released");
        }
        return;
    }

    CPosition *ppos = ecs_get_position(ecs, player);
    if (sword == ECS_NULL_ENTITY) {
        Vec2 aim = vec2(input->mouse_world_x, input->mouse_world_y);
        float aim_angle = atan2f(aim.y - ppos->pos.y, aim.x - ppos->pos.x);
        sword = spawn_sword(ecs, player, aim_angle, &tune);
        if (sword == ECS_NULL_ENTITY) return;
        LOG_DEBUG("%s (angle: %.2f)", tune.deploy_msg, aim_angle);
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
    float blade_half_width = tune.width * 0.5f;
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
            sword_hit_zombie(ecs, i, player, tag->damage, closest);
            ztag->sword_hit_timer = tag->hit_interval;
        }
    }
}