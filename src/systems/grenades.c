#include "systems/systems.h"
#include "events/event_bus.h"
#include "core/log.h"
#include <stdlib.h>
#include <math.h>

#define GRENADE_SPEED           320.0f
#define GRENADE_FUSE            0.8f
#define GRENADE_RADIUS          6.0f
#define GRENADE_EXPLOSION_RANGE 90.0f
#define GRENADE_DAMAGE          120.0f

static void explode_grenade(World *ecs, Entity grenade) {
    CGrenadeTag *tag = ecs_get_grenade_tag(ecs, grenade);
    Vec2 gpos = ecs_get_position(ecs, grenade)->pos;

    /* Safe: grenade has already been destroyed within the caller. */
    LOG_DEBUG("Grenade exploded at (%.0f, %.0f)", gpos.x, gpos.y);
    event_emit(g_events, GE_ENTITY_DEATH, grenade, GEK_GRENADE,
               gpos.x, gpos.y, tag->explosion_radius, tag->damage, 0, 0);

    /* Area-of-effect damage to every zombie within the blast radius. */
    for (uint32_t i = 0; i < ECS_MAX_ENTITIES; i++) {
        if (!ecs->alive[i]) continue;
        if (!(ecs->component_masks[i] & (1u << COMP_ZOMBIE_TAG))) continue;
        if (!(ecs->component_masks[i] & (1u << COMP_POSITION))) continue;
        if (!(ecs->component_masks[i] & (1u << COMP_HEALTH))) continue;

        Vec2 zpos = ecs->positions[i].pos;
        float zradius = ecs_has_component(ecs, i, COMP_COLLIDER)
                            ? ecs->colliders[i].radius : 12.0f;
        if (vec2_distance(gpos, zpos) > tag->explosion_radius + zradius) continue;

        CHealth *hp = &ecs->healths[i];
        float prev = hp->current;
        hp->current = prev - tag->damage;
        ecs->zombie_tags[i].last_hit_by = tag->owner;
        event_emit(g_events, GE_DAMAGE, i, GEK_ZOMBIE,
                   zpos.x, zpos.y, tag->damage, hp->current > 0 ? hp->current : 0,
                   (int)prev, 0);

        if (ecs_has_component(ecs, i, COMP_VELOCITY)) {
            Vec2 dir = vec2_sub(zpos, gpos);
            float len = vec2_length(dir);
            if (len < 0.001f) dir = vec2(1, 0); else dir = vec2_scale(dir, 1.0f / len);
            CVelocity *vel = &ecs->velocities[i];
            vel->vel = vec2_add(vel->vel, vec2_scale(dir, 250.0f));
        }

        CZombieTag *ztag = &ecs->zombie_tags[i];
        ztag->hurt_timer = 0.3f;
        ztag->state = ZOMBIE_HURT;
    }

    /* Orange fireball particles for the blast. */
    for (int p = 0; p < 16; p++) {
        Entity particle = ecs_create_entity(ecs);
        if (particle == ECS_NULL_ENTITY) continue;

        ecs_add_component(ecs, particle, COMP_POSITION);
        ecs_add_component(ecs, particle, COMP_SPRITE);
        ecs_add_component(ecs, particle, COMP_PARTICLE);

        float angle = (float)(rand() % 360) * M_PI / 180.0f;
        *ecs_get_position(ecs, particle) = (CPosition){{gpos.x, gpos.y}};
        *ecs_get_particle(ecs, particle) = (CParticle){
            .lifetime = 0.4f + (float)(rand() % 4) * 0.1f,
            .max_lifetime = 0.8f,
            .vel = vec2_from_angle(angle, 120.0f + (float)(rand() % 120)),
            .size_decay = 1.0f
        };
        *ecs_get_sprite(ecs, particle) = (CSprite){
            .sprite = sprite_circle(4.0f + (float)(rand() % 3), COLOR_ORANGE),
            .scale = 1.0f,
            .base_alpha = 1.0f
        };
        event_emit(g_events, GE_ENTITY_SPAWN, particle, GEK_PARTICLE,
                   gpos.x, gpos.y, 0, 0, 0, 0);
    }
}

static void throw_grenade(World *ecs, Entity player, const InputState *input) {
    CPosition *ppos = ecs_get_position(ecs, player);

    Vec2 dir = vec2_sub(vec2(input->mouse_world_x, input->mouse_world_y),
                        ppos->pos);
    if (vec2_length(dir) < 0.001f) dir = vec2(1, 0);
    else dir = vec2_normalize(dir);

    Entity g = ecs_create_entity(ecs);
    if (g == ECS_NULL_ENTITY) return;

    ecs_add_component(ecs, g, COMP_POSITION);
    ecs_add_component(ecs, g, COMP_VELOCITY);
    ecs_add_component(ecs, g, COMP_SPRITE);
    ecs_add_component(ecs, g, COMP_COLLIDER);
    ecs_add_component(ecs, g, COMP_GRENADE_TAG);

    Vec2 spawn = vec2_add(ppos->pos, vec2_scale(dir, 18.0f));
    *ecs_get_position(ecs, g) = (CPosition){{spawn.x, spawn.y}};
    *ecs_get_velocity(ecs, g) = (CVelocity){
        .vel = vec2_scale(dir, GRENADE_SPEED),
        .max_speed = GRENADE_SPEED
    };
    *ecs_get_collider(ecs, g) = (CCollider){GRENADE_RADIUS, true};
    *ecs_get_grenade_tag(ecs, g) = (CGrenadeTag){
        .owner = player,
        .fuse = GRENADE_FUSE,
        .max_fuse = GRENADE_FUSE,
        .explosion_radius = GRENADE_EXPLOSION_RANGE,
        .damage = GRENADE_DAMAGE
    };
    /* R13 merge fix (B17): grenade art was dropped; restore it. */
    Sprite gs = sprite_circle(GRENADE_RADIUS, COLOR_DARK_GREEN);
    SDL_Texture *grenade_tex = sprite_tex("textures/entities/grenade.png");
    if (grenade_tex) {
        gs = sprite_texture(grenade_tex);
        gs.color = COLOR_WHITE;
    }
    *ecs_get_sprite(ecs, g) = (CSprite){
        .sprite = gs,
        .scale = 0.375f,   /* 32px art -> 12 world units diameter */
        .base_alpha = 1.0f
    };

    event_emit(g_events, GE_PLAYER_SHOT, g, GEK_GRENADE,
               spawn.x, spawn.y, dir.x, dir.y, 0, 0);
}

void system_grenades(World *ecs, Player *p, float dt) {
    if (!p || !p->in_use || !p->alive) return;
    Entity player = p->entity;
    if (player == ECS_NULL_ENTITY || !ecs_is_alive(ecs, player)) return;

    InputState *input = &p->input;
    PlayerInventory *inv = &p->inventory;

    /* Trigger a throw on the click edge, consuming one grenade. */
    if (input->mouse_pressed[0] &&
        inv->unlocked[WEAPON_GRENADE] &&
        inv->current == WEAPON_GRENADE &&
        inv->grenades > 0) {
        inv->grenades--;
        throw_grenade(ecs, player, input);
    }

    /* Advance all live grenades: fuse countdown + impact-on-contact, then
     * detonate. */
    for (uint32_t i = 0; i < ECS_MAX_ENTITIES; i++) {
        if (!ecs->alive[i]) continue;
        if (!(ecs->component_masks[i] & (1u << COMP_GRENADE_TAG))) continue;

        CGrenadeTag *tag = &ecs->grenade_tags[i];
        tag->fuse -= dt;

        Vec2 gpos = ecs->positions[i].pos;
        bool struck = false;
        if (tag->fuse <= 0) {
            struck = true;
        } else {
            /* Impact: detonate immediately if the grenade lands on a zombie. */
            for (uint32_t j = 0; j < ECS_MAX_ENTITIES; j++) {
                if (!ecs->alive[j]) continue;
                if (!(ecs->component_masks[j] & (1u << COMP_ZOMBIE_TAG))) continue;
                if (!(ecs->component_masks[j] & (1u << COMP_POSITION))) continue;
                Vec2 zpos = ecs->positions[j].pos;
                float zradius = ecs_has_component(ecs, j, COMP_COLLIDER)
                                    ? ecs->colliders[j].radius : 12.0f;
                if (vec2_distance(gpos, zpos) < GRENADE_RADIUS + zradius) {
                    struck = true;
                    break;
                }
            }
        }

        if (struck) {
            explode_grenade(ecs, i);
            ecs_destroy_entity(ecs, i);
        }
    }
}