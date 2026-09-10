#include "systems/systems.h"

void system_particles(World *ecs, float dt) {
    for (uint32_t i = 0; i < ECS_MAX_ENTITIES; i++) {
        if (!ecs->alive[i]) continue;
        if (!(ecs->component_masks[i] & (1u << COMP_PARTICLE))) continue;

        CParticle *p = ecs_get_particle(ecs, i);
        CPosition *pos = ecs_get_position(ecs, i);

        p->lifetime -= dt;
        if (p->lifetime <= 0) {
            ecs_destroy_entity(ecs, i);
            continue;
        }

        pos->pos = vec2_add(pos->pos, vec2_scale(p->vel, dt));
        p->vel = vec2_scale(p->vel, 0.98f);

        if (ecs_has_component(ecs, i, COMP_SPRITE)) {
            CSprite *spr = ecs_get_sprite(ecs, i);
            float life_ratio = p->lifetime / p->max_lifetime;
            spr->base_alpha = life_ratio;
            spr->scale = life_ratio;
        }
    }
}
