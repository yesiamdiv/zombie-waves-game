#include "systems/systems.h"
#include "core/log.h"

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

        float dist = vec2_distance(pos->pos, player_pos);

        switch (ztag->state) {
            case ZOMBIE_CHASE: {
                if (dist < ztag->attack_range) {
                    ztag->state = ZOMBIE_ATTACK;
                    ztag->attack_timer = ztag->attack_cooldown * 0.5f;
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
                ztag->attack_timer -= dt;

                if (dist > ztag->attack_range * 1.5f) {
                    ztag->state = ZOMBIE_CHASE;
                } else if (ztag->attack_timer <= 0) {
                    /* Deal damage to player */
                    for (uint32_t j = 0; j < ECS_MAX_ENTITIES; j++) {
                        if (!ecs->alive[j]) continue;
                        if (ecs->component_masks[j] & (1u << COMP_PLAYER_TAG)) {
                            if (ecs_has_component(ecs, j, COMP_HEALTH)) {
                                ecs_get_health(ecs, j)->current -= 10.0f;
                                LOG_DEBUG("Zombie hit player! Health: %.0f",
                                         ecs_get_health(ecs, j)->current);
                            }
                            break;
                        }
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
