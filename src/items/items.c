#include "items/items.h"
#include "core/log.h"
#include <stdlib.h>

Entity items_spawn(World *ecs, Vec2 pos, ItemType type) {
    Entity e = ecs_create_entity(ecs);
    if (e == ECS_NULL_ENTITY) return e;

    ecs_add_component(ecs, e, COMP_POSITION);
    ecs_add_component(ecs, e, COMP_SPRITE);
    ecs_add_component(ecs, e, COMP_COLLIDER);
    ecs_add_component(ecs, e, COMP_ITEM_TAG);

    *ecs_get_position(ecs, e) = (CPosition){{pos.x, pos.y}};
    *ecs_get_collider(ecs, e) = (CCollider){10.0f, true};

    SDL_FColor color;
    float value;
    const char *name;

    switch (type) {
        case ITEM_HEALTH:
            color = (SDL_FColor){0.2f, 0.9f, 0.3f, 1.0f};
            value = 30.0f;
            name = "Health";
            break;
        case ITEM_AMMO:
            color = (SDL_FColor){1.0f, 0.9f, 0.2f, 1.0f};
            value = 10.0f;
            name = "Ammo";
            break;
        case ITEM_SPEED_BOOST:
            color = (SDL_FColor){0.3f, 0.6f, 1.0f, 1.0f};
            value = 5.0f;
            name = "Speed Boost";
            break;
        default:
            color = COLOR_WHITE;
            value = 1.0f;
            name = "Unknown";
    }

    *ecs_get_item_tag(ecs, e) = (CItemTag){.type = type, .value = value, .bob_timer = 0};
    *ecs_get_sprite(ecs, e) = (CSprite){
        .sprite = sprite_rect(8.0f, 8.0f, color),
        .scale = 1.0f,
        .base_alpha = 1.0f
    };

    LOG_DEBUG("Item spawned: %s at (%.0f, %.0f)", name, pos.x, pos.y);
    return e;
}

void items_spawn_random(World *ecs, Vec2 pos) {
    int type = rand() % ITEM_COUNT;
    items_spawn(ecs, pos, (ItemType)type);
}

void items_check_pickup(World *ecs, Entity player) {
    Vec2 player_pos = ecs_get_position(ecs, player)->pos;
    float pickup_range = 20.0f;

    for (uint32_t i = 0; i < ECS_MAX_ENTITIES; i++) {
        if (!ecs->alive[i]) continue;
        if (!(ecs->component_masks[i] & (1u << COMP_ITEM_TAG))) continue;
        if (!(ecs->component_masks[i] & (1u << COMP_POSITION))) continue;

        Vec2 item_pos = ecs_get_position(ecs, i)->pos;
        float dist = vec2_distance(player_pos, item_pos);

        if (dist < pickup_range) {
            CItemTag *item = ecs_get_item_tag(ecs, i);

            switch (item->type) {
                case ITEM_HEALTH:
                    if (ecs_has_component(ecs, player, COMP_HEALTH)) {
                        CHealth *hp = ecs_get_health(ecs, player);
                        hp->current = hp->current + item->value;
                        if (hp->current > hp->max) hp->current = hp->max;
                    }
                    break;

                case ITEM_AMMO:
                    /* In current impl unlimited ammo, could extend later */
                    break;

                case ITEM_SPEED_BOOST:
                    if (ecs_has_component(ecs, player, COMP_VELOCITY)) {
                        ecs_get_velocity(ecs, player)->max_speed += item->value;
                    }
                    break;

                default:
                    break;
            }

            LOG_DEBUG("Player picked up item type %d", item->type);
            ecs_destroy_entity(ecs, i);
        }
    }
}
