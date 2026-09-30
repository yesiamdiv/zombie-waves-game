#include "players.h"
#include "core/log.h"
#include "core/input.h"
#include "events/event_bus.h"
#include <string.h>

void players_reset(Player *players, int count) {
    for (int i = 0; i < count; i++) {
        memset(&players[i], 0, sizeof(Player));
    }
}

int players_active_count(const Player *players, int count) {
    int active = 0;
    for (int i = 0; i < count; i++) {
        if (players[i].in_use) active++;
    }
    return active;
}

/* Create a fresh player entity (position/velocity/health/sprite/collider/
 * player tag) for the given slot material. */
static Entity spawn_player_entity(World *ecs, Vec2 pos, const SDL_FColor *color) {
    Entity e = ecs_create_entity(ecs);
    if (e == ECS_NULL_ENTITY) return e;

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
    /* R13 merge: restore the assets texture path. `feature/multiplayer` had
     * reverted this to a flat rect for determinism, but textures never affect
     * simulation - and headless falls through to the flat rect anyway because
     * the asset manager is a no-op there. The caller-supplied color wins: in
     * co-op that is net_slot_color(), which must match what the client's
     * mirror draws for this slot. */
    Sprite ps = sprite_rect(16.0f, 16.0f, pc);
    SDL_Texture *player_tex = sprite_tex("textures/entities/player.png");
    if (player_tex) {
        /* 32px art at scale 0.5 renders as the original 16x16 world unit. */
        ps = sprite_texture(player_tex);
        ps.color = pc;
    }
    *ecs_get_sprite(ecs, e) = (CSprite){
        .sprite = ps,
        .scale = 0.5f,
        .base_alpha = 1.0f
    };
    return e;
}

int player_respawn(Player *players, World *ecs, int idx, const char *slot_name,
                   const SDL_FColor *color, Vec2 pos) {
    if (!players || idx < 0 || idx >= MAX_PLAYERS) return -1;
    if (players[idx].in_use) return -1;

    Entity e = spawn_player_entity(ecs, pos, color);
    if (e == ECS_NULL_ENTITY) return -1;

    Player *p = &players[idx];
    memset(p, 0, sizeof(Player));
    p->in_use = true;
    p->alive = true;
    p->entity = e;
    p->color = color ? *color : COLOR_BLUE;
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

/* TDM respawn: recreate the player's entity at their OWN beacon, KEEPING their
 * inventory (purchases/points), name, color, and kill count. Returns the index
 * or -1 if the ECS is exhausted. */
int player_respawn_slot(Player *players, World *ecs, int idx) {
    if (!players || idx < 0 || idx >= MAX_PLAYERS) return -1;
    Player *p = &players[idx];
    if (!p->in_use) return -1;

    Entity e = spawn_player_entity(ecs, p->beacon_pos, &p->color);
    if (e == ECS_NULL_ENTITY) return -1;

    p->entity = e;
    p->alive = true;
    p->respawn_timer = 0.0f;
    p->shoot_cd = 0.0f;
    input_init(&p->input);

    LOG_INFO("Player '%s' respawned at beacon (%.0f, %.0f)",
             p->name, p->beacon_pos.x, p->beacon_pos.y);
    event_emit(g_events, GE_ENTITY_SPAWN, e, GEK_PLAYER,
               p->beacon_pos.x, p->beacon_pos.y, 0, 0, 0, 0);
    return idx;
}

int players_find_index(const Player *players, int count, Entity entity) {
    if (!players || entity == ECS_NULL_ENTITY) return -1;
    for (int i = 0; i < count; i++) {
        if (players[i].in_use && players[i].entity == entity) return i;
    }
    return -1;
}

/* Per-frame match rule update. Detects slots whose entity was just destroyed
 * by system_cleanup (health <= 0) and applies the mode's death rule:
 *
 *   SINGLE        - slot simply marked dead (game-over decided by the caller)
 *   MULTI_TDM     - dead slot waits MULTI_RESPAWN_TIME then respawns at beacon
 *   MULTI_HARDCORE - dead slot is eliminated for the rest of the match
 *
 * Returns the number of slots that are alive (host uses 0 in HARDCORE to end
 * the game when everyone is eliminated). Call AFTER system_cleanup. */
int players_match_update(World *ecs, Player *players, int count, GameMode mode,
                         float dt) {
    int alive_count = 0;

    for (int s = 0; s < count; s++) {
        Player *p = &players[s];
        if (!p->in_use) continue;

        /* Entity gone on a still-"alive" slot == just died this frame. */
        if (p->alive && p->entity != ECS_NULL_ENTITY &&
            !ecs_is_alive(ecs, p->entity)) {
            p->alive = false;
            if (game_mode_is_multi(mode)) {
                if (mode == GAME_MODE_MULTI_HARDCORE) {
                    p->eliminated = true;
                    LOG_INFO("HARDCORE: player '%s' eliminated (permanent)", p->name);
                } else {
                    p->respawn_timer = MULTI_RESPAWN_TIME;
                    LOG_INFO("Player '%s' died; respawning at beacon in %.1fs",
                             p->name, MULTI_RESPAWN_TIME);
                }
            }
        }

        if (p->alive) {
            alive_count++;
            continue;
        }

        /* Dead slot: only TDM brings it back. */
        if (game_mode_is_multi(mode) && mode == GAME_MODE_MULTI_TDM &&
            !p->eliminated && p->respawn_timer > 0.0f) {
            p->respawn_timer -= dt;
            if (p->respawn_timer <= 0.0f) {
                p->respawn_timer = 0.0f;
                if (player_respawn_slot(players, ecs, s) >= 0) {
                    alive_count++; /* revived this tick: count it now */
                }
            }
        }
    }

    return alive_count;
}