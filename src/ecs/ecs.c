#include "ecs/ecs.h"
#include <stdlib.h>
#include <string.h>

void ecs_init(World *world) {
    memset(world, 0, sizeof(World));
    world->entity_count = 0;
    world->alive_count = 0;

    for (uint32_t i = 0; i < ECS_MAX_ENTITIES; i++) {
        world->entities[i] = i;
        world->alive[i] = false;
        world->component_masks[i] = 0;
    }

    LOG_INFO("ECS initialized (max_entities=%d, max_components=%d)",
             ECS_MAX_ENTITIES, ECS_MAX_COMPONENTS);
}

Entity ecs_create_entity(World *world) {
    for (uint32_t i = 0; i < ECS_MAX_ENTITIES; i++) {
        if (!world->alive[i]) {
            world->alive[i] = true;
            world->component_masks[i] = 0;
            world->alive_count++;
            LOG_TRACE("Entity created: %u (total alive: %u)", i, world->alive_count);
            return world->entities[i];
        }
    }
    LOG_ERROR("ECS: No free entities available!");
    return ECS_NULL_ENTITY;
}

void ecs_destroy_entity(World *world, Entity entity) {
    uint32_t idx = ecs_get_entity_index(world, entity);
    if (idx < ECS_MAX_ENTITIES && world->alive[idx]) {
        world->alive[idx] = false;
        world->component_masks[idx] = 0;
        world->alive_count--;
        LOG_TRACE("Entity destroyed: %u (total alive: %u)", entity, world->alive_count);
    }
}

bool ecs_is_alive(World *world, Entity entity) {
    if (entity >= ECS_MAX_ENTITIES) return false;
    return world->alive[entity];
}

void ecs_add_component(World *world, Entity entity, ComponentType type) {
    uint32_t idx = ecs_get_entity_index(world, entity);
    if (idx < ECS_MAX_ENTITIES) {
        world->component_masks[idx] |= (1u << type);
    }
}

void ecs_remove_component(World *world, Entity entity, ComponentType type) {
    uint32_t idx = ecs_get_entity_index(world, entity);
    if (idx < ECS_MAX_ENTITIES) {
        world->component_masks[idx] &= ~(1u << type);
    }
}

bool ecs_has_component(World *world, Entity entity, ComponentType type) {
    uint32_t idx = ecs_get_entity_index(world, entity);
    if (idx >= ECS_MAX_ENTITIES) return false;
    return (world->component_masks[idx] & (1u << type)) != 0;
}

uint32_t ecs_get_entity_index(World *world, Entity entity) {
    if (entity >= ECS_MAX_ENTITIES) return ECS_MAX_ENTITIES;
    return entity;
}
