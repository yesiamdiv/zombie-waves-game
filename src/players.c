#include "players.h"
#include "core/log.h"
#include "core/input.h"
#include <string.h>

void players_reset(Player *players, int count) {
    for (int i = 0; i < count; i++) {
        memset(&players[i], 0, sizeof(Player));
    }
}

int player_respawn(Player *players, World *ecs, int idx, const char *slot_name,
                   const SDL_FColor *color, Vec2 pos) {
    if (!players || idx < 0 || idx >= MAX_PLAYERS) return -1;
    if (players[idx].in_use) return -1;

    Entity e = ecs_create_entity(ecs);
    if (e == ECS_NULL_ENTITY) return -1;

    ecs_add_component(ecs, e, COMP_POSITION);
    ecs_add_component(ecs, e, COMP_VELOCITY);
    ecs_add_component(ecs, e, COMP_HEALTH);
    ecs_add_component(ecs, e, COMP_SPRITE);
    ecs_add_component(ecs, e, COMP_COLLIDER);
    ecs_add_component(ecs, e, COMP_PLAYER_TAG);

    *ecs_get_position(ecs, e) = (CPosition){{pos.x, pos.y}};
    *ecs_get_velocity(ecs, e) = (CVelocity){{0, 0}, 200.0f};
    *ecs_get_health(ecs, e) = (CHealth){200.0f, 200.0f};
    *ecs_get_collider(ecs, e) = (CCollider){14.0f, false};

    SDL_FColor pc = color ? *color : COLOR_BLUE;
    *ecs_get_sprite(ecs, e) = (CSprite){
        .sprite = sprite_rect(16.0f, 16.0f, pc),
        .scale = 1.0f,
        .base_alpha = 1.0f
    };

    Player *p = &players[idx];
    memset(p, 0, sizeof(Player));
    p->in_use = true;
    p->alive = true;
    p->entity = e;
    p->color = pc;
    p->beacon_pos = pos; /* beacon marker + respawn anchor */
    if (slot_name) {
        snprintf(p->name, sizeof(p->name), "%s", slot_name);
    } else {
        snprintf(p->name, sizeof(p->name), "Player %d", idx + 1);
    }
    input_init(&p->input);
    weapons_inventory_init(&p->inventory);

    LOG_INFO("Player '%s' spawned at (%.0f, %.0f)", p->name, pos.x, pos.y);
    return idx;
}

int players_find_index(const Player *players, int count, Entity entity) {
    if (!players || entity == ECS_NULL_ENTITY) return -1;
    for (int i = 0; i < count; i++) {
        if (players[i].in_use && players[i].entity == entity) return i;
    }
    return -1;
}