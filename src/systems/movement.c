#include "systems/systems.h"
#include "core/log.h"

void system_movement(World *ecs, GameWorld *world, float dt) {
    for (uint32_t i = 0; i < ECS_MAX_ENTITIES; i++) {
        if (!ecs->alive[i]) continue;
        if (!(ecs->component_masks[i] & (1u << COMP_POSITION))) continue;
        if (!(ecs->component_masks[i] & (1u << COMP_VELOCITY))) continue;

        CPosition *pos = &ecs->positions[i];
        CVelocity *vel = &ecs->velocities[i];

        Vec2 new_pos = vec2_add(pos->pos, vec2_scale(vel->vel, dt));

        /* Clamp to world bounds */
        new_pos.x = clampf(new_pos.x, 16.0f, world->world_pixel_w - 16.0f);
        new_pos.y = clampf(new_pos.y, 16.0f, world->world_pixel_h - 16.0f);

        /* World collision - try sliding */
        if (!world_is_walkable(world, new_pos.x, new_pos.y)) {
            /* Try X only */
            Vec2 try_x = {new_pos.x, pos->pos.y};
            if (world_is_walkable(world, try_x.x, try_x.y)) {
                new_pos = try_x;
            } else {
                /* Try Y only */
                Vec2 try_y = {pos->pos.x, new_pos.y};
                if (world_is_walkable(world, try_y.x, try_y.y)) {
                    new_pos = try_y;
                } else {
                    new_pos = pos->pos;
                }
            }
        }

        pos->pos = new_pos;
    }
}
