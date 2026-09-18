#include "net.h"
#include "ecs/ecs.h"

/* Build the 20 Hz world snapshot on the host. Only semantic (networked)
 * state crosses the wire: entity id (a stable ECS index), kind, pos, vel,
 * hp, and the projectile/owner linkage. Visuals are reconstructed by the
 * client from `kind` (deterministic factories on both sides). The slot map
 * lets each client pick its own player out of the entity list. */
int net_snapshot_build(const World *ecs, const Player *players, int player_count,
                       float sim_time, const WaveSystem *waves, NetSnapshot *out) {
    if (!ecs || !out) return -1;
    *out = (NetSnapshot){0};
    out->sim_time = sim_time;
    if (waves) {
        out->wave_number = (uint16_t)waves->wave_number;
        out->wave_active = waves->wave_active ? 1 : 0;
        out->total_kills = (uint16_t)(waves->total_kills & 0xFFFF);
    }
    for (int s = 0; s < player_count && s < NET_MAX_PLAYERS; s++) {
        if (players && players[s].in_use &&
            players[s].entity != ECS_NULL_ENTITY &&
            ecs->alive[players[s].entity]) {
            out->slot_entities[s] = (uint16_t)players[s].entity;
        }
    }

    for (Entity e = 0; e < ECS_MAX_ENTITIES; e++) {
        if (!ecs->alive[e]) continue;
        uint32_t mask = ecs->component_masks[e];
        uint8_t kind;
        Entity owner = 0;
        if (mask & (1u << COMP_PLAYER_TAG)) {
            kind = NET_ENT_PLAYER;
        } else if (mask & (1u << COMP_ZOMBIE_TAG)) {
            kind = NET_ENT_ZOMBIE;
        } else if (mask & (1u << COMP_BULLET_TAG)) {
            kind = NET_ENT_BULLET;
            owner = ecs->bullet_tags[e].owner;
        } else if (mask & (1u << COMP_GRENADE_TAG)) {
            kind = NET_ENT_GRENADE;
            owner = ecs->grenade_tags[e].owner;
        } else if (mask & (1u << COMP_ROCKET_TAG)) {
            kind = NET_ENT_ROCKET;
            owner = ecs->rocket_tags[e].owner;
        } else if (mask & (1u << COMP_ITEM_TAG)) {
            kind = NET_ENT_ITEM;
        } else {
            continue;   /* swords/particles are visuals, not replicated */
        }
        if (out->count >= NET_SNAP_MAX_ENTITIES) break;

        NetEntitySnap *se = &out->entities[out->count++];
        se->id = (uint16_t)e;
        se->kind = kind;
        se->pos = ecs->positions[e].pos;
        se->vel = (mask & (1u << COMP_VELOCITY))
                      ? ecs->velocities[e].vel
                      : (Vec2){0.0f, 0.0f};
        se->hp = (mask & (1u << COMP_HEALTH)) ? ecs->healths[e].current : 0.0f;
        se->flags = 0;
        se->owner = (uint16_t)owner;
    }
    return out->count;
}