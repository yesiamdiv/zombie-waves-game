#include "systems/systems.h"
#include "events/event_bus.h"
#include "core/log.h"

#define BULLET_DAMAGE 25.0f

void system_collision(World *ecs, GameWorld *world) {
    /* Collect all entities with colliders */
    Entity colliders[ECS_MAX_ENTITIES];
    int count = 0;

    for (uint32_t i = 0; i < ECS_MAX_ENTITIES; i++) {
        if (!ecs->alive[i]) continue;
        if (!(ecs->component_masks[i] & (1u << COMP_COLLIDER))) continue;
        if (!(ecs->component_masks[i] & (1u << COMP_POSITION))) continue;
        colliders[count++] = i;
    }

    /* Check bullet vs zombie and bullet vs player */
    for (int i = 0; i < count; i++) {
        Entity a = colliders[i];
        if (!ecs_has_component(ecs, a, COMP_BULLET_TAG)) continue;

        for (int j = 0; j < count; j++) {
            if (i == j) continue;
            Entity b = colliders[j];

            /* Bullets shouldn't hit other bullets or the owner */
            if (ecs_has_component(ecs, b, COMP_BULLET_TAG)) continue;
            CBulletTag *bullet = ecs_get_bullet_tag(ecs, a);
            if (b == bullet->owner) continue;

            /* R13-I3: dropped items carry a collider but no health, so a bullet
             * crossing one used to be destroyed for a "hit" that did nothing.
             * Bullets pass through items (and any other non-solid collider). */
            if (ecs_has_component(ecs, b, COMP_ITEM_TAG)) continue;

            Vec2 pos_a = ecs_get_position(ecs, a)->pos;
            Vec2 pos_b = ecs_get_position(ecs, b)->pos;
            float rad_a = ecs_get_collider(ecs, a)->radius;
            float rad_b = ecs_get_collider(ecs, b)->radius;

            if (circle_circle_collision(pos_a, rad_a, pos_b, rad_b)) {
                /* Damage the target */
                if (ecs_has_component(ecs, b, COMP_HEALTH)) {
                    CHealth *hp = ecs_get_health(ecs, b);
                    float dmg = BULLET_DAMAGE;
                    float prev = hp->current;
                    hp->current = prev - dmg;

                    /* Credit the shooter with any resulting kill. */
                    if (ecs_has_component(ecs, b, COMP_ZOMBIE_TAG)) {
                        ecs_get_zombie_tag(ecs, b)->last_hit_by = bullet->owner;
                    }

                    EventKind kind = GEK_NONE;
                    if (ecs_has_component(ecs, b, COMP_ZOMBIE_TAG)) kind = GEK_ZOMBIE;
                    else if (ecs_has_component(ecs, b, COMP_PLAYER_TAG)) kind = GEK_PLAYER;
                    event_emit(g_events, GE_DAMAGE, b, kind,
                               pos_b.x, pos_b.y, dmg, hp->current > 0 ? hp->current : 0,
                               (int)prev, 0);

                    /* Knockback */
                    if (ecs_has_component(ecs, b, COMP_VELOCITY)) {
                        Vec2 dir = vec2_normalize(vec2_sub(pos_b, pos_a));
                        CVelocity *vel = ecs_get_velocity(ecs, b);
                        vel->vel = vec2_add(vel->vel, vec2_scale(dir, 200.0f));
                    }

                    /* Hurt state for zombies */
                    if (ecs_has_component(ecs, b, COMP_ZOMBIE_TAG)) {
                        CZombieTag *ztag = ecs_get_zombie_tag(ecs, b);
                        ztag->hurt_timer = 0.15f;
                        ztag->state = ZOMBIE_HURT;
                    }
                }

                /* Destroy bullet */
                ecs_destroy_entity(ecs, a);
                break;
            }
        }
    }

    /* Zombie vs player collision */
    for (int i = 0; i < count; i++) {
        Entity a = colliders[i];
        if (!ecs_has_component(ecs, a, COMP_ZOMBIE_TAG)) continue;

        for (int j = 0; j < count; j++) {
            Entity b = colliders[j];
            if (!ecs_has_component(ecs, b, COMP_PLAYER_TAG)) continue;

            Vec2 pos_a = ecs_get_position(ecs, a)->pos;
            Vec2 pos_b = ecs_get_position(ecs, b)->pos;
            float rad_a = ecs_get_collider(ecs, a)->radius;
            float rad_b = ecs_get_collider(ecs, b)->radius;

            if (circle_circle_collision(pos_a, rad_a, pos_b, rad_b)) {
                /* Contact is not a free pass: a zombie overlapping a player
                 * deals damage and slows them. Gated by the zombie's own
                 * attack_timer, which the zombie_ai attack state machine shares,
                 * so a hit lands once per cooldown rather than every frame. */
                CZombieTag *ztag = ecs_get_zombie_tag(ecs, a);
                if (ztag->attack_timer <= 0) {
                    zombie_damage_player(ecs, a, b, pos_a);
                    ztag->attack_timer = ztag->attack_cooldown;
                }

                /* Push player away */
                Vec2 dir = vec2_normalize(vec2_sub(pos_b, pos_a));
                CVelocity *pvel = ecs_get_velocity(ecs, b);
                pvel->vel = vec2_add(pvel->vel, vec2_scale(dir, 150.0f));
            }
        }
    }
}
