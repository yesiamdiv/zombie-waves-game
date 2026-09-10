#include "systems/systems.h"
#include "core/log.h"

void system_bullets(World *ecs, GameWorld *world, float dt) {
    for (uint32_t i = 0; i < ECS_MAX_ENTITIES; i++) {
        if (!ecs->alive[i]) continue;
        if (!(ecs->component_masks[i] & (1u << COMP_BULLET_TAG))) continue;

        CBulletTag *bt = ecs_get_bullet_tag(ecs, i);
        bt->lifetime -= dt;

        if (bt->lifetime <= 0) {
            ecs_destroy_entity(ecs, i);
            continue;
        }

        /* Destroy if out of world bounds */
        if (ecs_has_component(ecs, i, COMP_POSITION)) {
            Vec2 pos = ecs_get_position(ecs, i)->pos;
            if (!world_in_bounds(world, pos.x, pos.y)) {
                ecs_destroy_entity(ecs, i);
            }
        }
    }
}
