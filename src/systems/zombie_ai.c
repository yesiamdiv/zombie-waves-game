#include "systems/systems.h"
#include "events/event_bus.h"
#include "config.h"
#include "core/log.h"

#define ZOMBIE_ATTACK_DAMAGE 18.0f

/* Shared melee hit on the player: damage, knockback away from the zombie, and
 * a brief movement slow so the player cannot instantly escape the hit. Used by
 * both the state-machine attack (zombie_ai) and direct contact (collision). */
void zombie_damage_player(World *ecs, Entity zombie, Vec2 origin) {
    if (zombie == ECS_NULL_ENTITY || !ecs_is_alive(ecs, zombie)) return;

    for (uint32_t j = 0; j < ECS_MAX_ENTITIES; j++) {
        if (!ecs->alive[j]) continue;
        if (!(ecs->component_masks[j] & (1u << COMP_PLAYER_TAG))) continue;
        if (!(ecs->component_masks[j] & (1u << COMP_HEALTH))) continue;

        CHealth *hp = ecs_get_health(ecs, j);
        float prev = hp->current;
        float dmg = ZOMBIE_ATTACK_DAMAGE * g_zombie_damage_mult;
        hp->current = prev - dmg;
        LOG_DEBUG("Zombie hit player! Health: %.0f", hp->current);

        Vec2 ppos = ecs->positions[j].pos;
        event_emit(g_events, GE_PLAYER_HEALTH, j, GEK_PLAYER,
                   ppos.x, ppos.y, hp->current, hp->max, (int)prev, 0);
        event_emit(g_events, GE_DAMAGE, j, GEK_PLAYER,
                   ppos.x, ppos.y, dmg, hp->current > 0 ? hp->current : 0,
                   (int)prev, zombie);

        if (ecs_has_component(ecs, j, COMP_VELOCITY)) {
            Vec2 dir = vec2_sub(ppos, origin);
            float len = vec2_length(dir);
            if (len < 0.001f) dir = vec2(1, 0); else dir = vec2_scale(dir, 1.0f / len);
            CVelocity *pvel = ecs_get_velocity(ecs, j);
            pvel->vel = vec2_add(pvel->vel, vec2_scale(dir, 200.0f));
        }

        CPlayerTag *ptag = ecs_get_player_tag(ecs, j);
        ptag->slow_timer = PLAYER_HURT_SLOW_DURATION;
        break;
    }
}

void system_zombie_ai(World *ecs, float dt) {
    /* Find player position */
    Vec2 player_pos = {0, 0};
    bool player_found = false;

    for (uint32_t i = 0; i < ECS_MAX_ENTITIES; i++) {
        if (!ecs->alive[i]) continue;
        if (ecs->component_masks[i] & (1u << COMP_PLAYER_TAG)) {
            player_pos = ecs_get_position(ecs, i)->pos;
            player_found = true;
            break;
        }
    }
    if (!player_found) return;

    for (uint32_t i = 0; i < ECS_MAX_ENTITIES; i++) {
        if (!ecs->alive[i]) continue;
        if (!(ecs->component_masks[i] & (1u << COMP_ZOMBIE_TAG))) continue;
        if (!(ecs->component_masks[i] & (1u << COMP_POSITION))) continue;
        if (!(ecs->component_masks[i] & (1u << COMP_VELOCITY))) continue;

        CZombieTag *ztag = ecs_get_zombie_tag(ecs, i);
        CPosition *pos = ecs_get_position(ecs, i);
        CVelocity *vel = ecs_get_velocity(ecs, i);

        /* Hurt state */
        if (ztag->hurt_timer > 0) {
            ztag->hurt_timer -= dt;
            vel->vel = vec2_scale(vel->vel, 0.9f);  /* decelerate during hurt */
            continue;
        }

        ztag->attack_timer -= dt;

        float dist = vec2_distance(pos->pos, player_pos);

        switch (ztag->state) {
            case ZOMBIE_CHASE: {
                if (dist < ztag->attack_range) {
                    ztag->state = ZOMBIE_ATTACK;
                    ztag->attack_timer = 0.15f;  /* short windup, then strike */
                } else if (dist < ztag->detection_range) {
                    Vec2 dir = vec2_normalize(vec2_sub(player_pos, pos->pos));
                    vel->vel = vec2_scale(dir, vel->max_speed);
                } else {
                    /* Wander */
                    vel->vel = vec2_scale(vel->vel, 0.95f);
                }
                break;
            }

            case ZOMBIE_ATTACK: {
                vel->vel = vec2(0, 0);

                if (dist > ztag->attack_range * 1.5f) {
                    ztag->state = ZOMBIE_CHASE;
                } else if (ztag->attack_timer <= 0) {
                    zombie_damage_player(ecs, i, pos->pos);
                    ztag->attack_timer = ztag->attack_cooldown;
                }
                break;
            }

            case ZOMBIE_HURT:
                /* Handled by hurt_timer above */
                if (ztag->hurt_timer <= 0) {
                    ztag->state = ZOMBIE_CHASE;
                }
                break;

            default:
                ztag->state = ZOMBIE_CHASE;
                break;
        }
    }
}