#include "systems/systems.h"
#include "events/event_bus.h"
#include "config.h"
#include "core/log.h"

#define ZOMBIE_ATTACK_DAMAGE 10.0f

/* Nearest target for a zombie. When a player slot table is provided (multiplayer
 * / resident slots) only alive slots are candidates; otherwise fall back to the
 * nearest COMP_PLAYER_TAG entity (tests / pre-slot callers). Returns the target
 * entity and writes its position to *out_pos, or ECS_NULL_ENTITY when no
 * survivor exists. */
static Entity nearest_alive_player(World *ecs, const Player *players,
                                   int player_count, Vec2 from, Vec2 *out_pos) {
    Entity best = ECS_NULL_ENTITY;
    float best_d = 0.0f;
    Vec2 best_pos = {0, 0};

    if (players && player_count > 0) {
        for (int s = 0; s < player_count; s++) {
            const Player *p = &players[s];
            if (!p->in_use || !p->alive) continue;
            if (p->entity == ECS_NULL_ENTITY || !ecs_is_alive(ecs, p->entity)) continue;
            if (!(ecs->component_masks[p->entity] & (1u << COMP_POSITION))) continue;
            Vec2 pos = ecs_get_position(ecs, p->entity)->pos;
            float d = vec2_distance(from, pos);
            if (best == ECS_NULL_ENTITY || d < best_d) {
                best = p->entity;
                best_d = d;
                best_pos = pos;
            }
        }
    } else {
        for (uint32_t i = 0; i < ECS_MAX_ENTITIES; i++) {
            if (!ecs->alive[i]) continue;
            if (!(ecs->component_masks[i] & (1u << COMP_PLAYER_TAG))) continue;
            if (!(ecs->component_masks[i] & (1u << COMP_POSITION))) continue;
            Vec2 pos = ecs_get_position(ecs, i)->pos;
            float d = vec2_distance(from, pos);
            if (best == ECS_NULL_ENTITY || d < best_d) {
                best = i;
                best_d = d;
                best_pos = pos;
            }
        }
    }

    if (best != ECS_NULL_ENTITY && out_pos) *out_pos = best_pos;
    return best;
}

void system_zombie_ai(World *ecs, Player *players, int player_count, float dt) {
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

        /* No survivors = nothing to hunt. */
        Vec2 target_pos;
        Entity target = nearest_alive_player(ecs, players, player_count,
                                             pos->pos, &target_pos);
        if (target == ECS_NULL_ENTITY) continue;

        float dist = vec2_distance(pos->pos, target_pos);

        switch (ztag->state) {
            case ZOMBIE_CHASE: {
                if (dist < ztag->attack_range) {
                    ztag->state = ZOMBIE_ATTACK;
                    ztag->attack_timer = ztag->attack_cooldown * 0.5f;
                } else if (dist < ztag->detection_range) {
                    Vec2 dir = vec2_normalize(vec2_sub(target_pos, pos->pos));
                    vel->vel = vec2_scale(dir, vel->max_speed);
                } else {
                    /* Wander */
                    vel->vel = vec2_scale(vel->vel, 0.95f);
                }
                break;
            }

            case ZOMBIE_ATTACK: {
                vel->vel = vec2(0, 0);
                ztag->attack_timer -= dt;

                if (dist > ztag->attack_range * 1.5f) {
                    ztag->state = ZOMBIE_CHASE;
                } else if (ztag->attack_timer <= 0) {
                    /* Deal damage to the chosen target (the alive player this
                     * zombie actually chased - not "the first player found"). */
                    if (ecs_has_component(ecs, target, COMP_HEALTH)) {
                        CHealth *hp = ecs_get_health(ecs, target);
                        float prev = hp->current;
                        float dmg = ZOMBIE_ATTACK_DAMAGE * g_zombie_damage_mult;
                        hp->current = prev - dmg;
                        LOG_DEBUG("Zombie hit player! Health: %.0f", hp->current);
                        event_emit(g_events, GE_PLAYER_HEALTH, target, GEK_PLAYER,
                                   ecs->positions[target].pos.x, ecs->positions[target].pos.y,
                                   hp->current, hp->max, (int)prev, 0);
                        event_emit(g_events, GE_DAMAGE, target, GEK_PLAYER,
                                   ecs->positions[target].pos.x, ecs->positions[target].pos.y,
                                   dmg, hp->current > 0 ? hp->current : 0,
                                   (int)prev, i);
                    }
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