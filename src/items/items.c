#include "items/items.h"
#include "events/event_bus.h"
#include "graphics/sprite.h"
#include "core/log.h"
#include <math.h>
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

    /* Textured pickup with a flat-color fallback: headless runs and missing
     * art still render a readable, correctly-sized pickup. */
    const char *item_tex = NULL;
    switch (type) {
        case ITEM_HEALTH:      item_tex = "textures/entities/medkit.png"; break;
        case ITEM_AMMO:        item_tex = "textures/entities/ammo.png"; break;
        case ITEM_SPEED_BOOST: item_tex = "textures/entities/speed.png"; break;
        default: break;
    }
    Sprite is = sprite_rect(8.0f, 8.0f, color);
    SDL_Texture *pickup_tex = sprite_tex(item_tex);
    if (pickup_tex) {
        is = sprite_texture(pickup_tex);
        is.color = COLOR_WHITE;
    }

    *ecs_get_item_tag(ecs, e) = (CItemTag){
        .type = type, .value = value, .bob_timer = 0,
        .base_pos = pos};
    *ecs_get_sprite(ecs, e) = (CSprite){
        .sprite = is,
        .scale = 0.5f,   /* 32px art -> 16 world units, same footprint as player */
        .base_alpha = 1.0f
    };

    LOG_DEBUG("Item spawned: %s at (%.0f, %.0f)", name, pos.x, pos.y);
    event_emit(g_events, GE_ENTITY_SPAWN, e, GEK_ITEM, pos.x, pos.y,
               (float)type, value, 0, 0);
    return e;
}

void items_spawn_random(World *ecs, Vec2 pos) {
    int type = rand() % ITEM_COUNT;
    items_spawn(ecs, pos, (ItemType)type);
}

int items_count_alive(World *ecs) {
    int count = 0;
    for (uint32_t i = 0; i < ECS_MAX_ENTITIES; i++) {
        if (ecs->alive[i] && (ecs->component_masks[i] & (1u << COMP_ITEM_TAG))) {
            count++;
        }
    }
    return count;
}

void items_check_pickup(World *ecs, Entity player, PlayerInventory *inv) {
    Vec2 player_pos = ecs_get_position(ecs, player)->pos;
    float pickup_range = 20.0f;

    for (uint32_t i = 0; i < ECS_MAX_ENTITIES; i++) {
        if (!ecs->alive[i]) continue;
        if (!(ecs->component_masks[i] & (1u << COMP_ITEM_TAG))) continue;
        if (!(ecs->component_masks[i] & (1u << COMP_POSITION))) continue;

        CItemTag *item = ecs_get_item_tag(ecs, i);
        /* The SIM position, not the displayed one: the bob is cosmetic and
         * must not move the pickup window (see CItemTag.base_pos). */
        Vec2 item_pos = item->base_pos;
        float dist = vec2_distance(player_pos, item_pos);

        if (dist < pickup_range) {

            switch (item->type) {
                case ITEM_HEALTH:
                    if (ecs_has_component(ecs, player, COMP_HEALTH)) {
                        CHealth *hp = ecs_get_health(ecs, player);
                        float prev = hp->current;
                        hp->current = hp->current + item->value;
                        if (hp->current > hp->max) hp->current = hp->max;
                        /* Same payload shape as the melee emitter in
                         * zombie_ai.c: a=hp after, b=max, ia=hp before. */
                        event_emit(g_events, GE_PLAYER_HEALTH, player, GEK_PLAYER,
                                   player_pos.x, player_pos.y,
                                   hp->current, hp->max, (int)prev, 0);
                    }
                    break;

                case ITEM_AMMO:
                    /* Pistol ammo is unlimited, so the ammo pickup is a
                     * consumable refill instead (B5): it feeds whichever
                     * owned weapons drain ammo. Unlocking the weapon is the
                     * gate, so a fresh run's pickup is a safe no-op. */
                    if (inv) {
                        if (inv->unlocked[WEAPON_GRENADE]) inv->grenades += 1;
                        if (inv->unlocked[WEAPON_LAUNCHER]) inv->launcher_ammo += 2;
                    }
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
            event_emit(g_events, GE_ITEM_PICKUP, i, GEK_ITEM,
                       item_pos.x, item_pos.y, (float)item->type, item->value, 0, 0);
            ecs_destroy_entity(ecs, i);
        }
    }
}

/* Pickup bob: the item hovers so it reads as a pickup rather than scenery.
 *
 * It writes the entity's CPosition - the DISPLAYED position, which the
 * renderer draws and the snapshot replicates, so host and client show the same
 * bob - while CItemTag.base_pos keeps the true position for the sim. That is
 * what makes this safe: the pickup range and the GE_ITEM_PICKUP payload use
 * base_pos, so single-player pickup timing is unchanged (docs/PROGRESS.md,
 * AN5). The bob is vertical only and deterministic (accumulated dt, no rand). */
void items_tick_bob(World *ecs, float dt) {
    for (uint32_t i = 0; i < ECS_MAX_ENTITIES; i++) {
        if (!ecs->alive[i]) continue;
        if (!(ecs->component_masks[i] & (1u << COMP_ITEM_TAG))) continue;
        if (!(ecs->component_masks[i] & (1u << COMP_POSITION))) continue;

        CItemTag *item = ecs_get_item_tag(ecs, i);
        item->bob_timer += dt;

        CPosition *pos = ecs_get_position(ecs, i);
        pos->pos.x = item->base_pos.x;
        pos->pos.y = item->base_pos.y +
                     ITEM_BOB_PX * sinf(item->bob_timer *
                                        (2.0f * 3.14159265f * ITEM_BOB_HZ));
    }
}
