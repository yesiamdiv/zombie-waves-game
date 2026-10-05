#include "net.h"
#include "ecs/ecs.h"
#include "assets/asset_manager.h"
#include "weapons/weapons.h"

/* Float colour component -> byte. Clamps in FLOAT space: casting first would
 * make an out-of-range or negative component undefined, and comparing the
 * uint8_t result against 255 would be a tautology the compiler rejects. */
static uint8_t net_tint_byte(float v) {
    if (v <= 0.0f) return 0;
    if (v >= 1.0f) return 255;
    return (uint8_t)(v * 255.0f + 0.5f);
}

/* Non-negative float -> centi-units, saturating. Rounds to nearest so the
 * client never sees HP drift below the value the host is displaying. */
static uint16_t net_centis(float v) {
    if (!(v > 0.0f)) return 0;          /* also catches NaN */
    float c = v * 100.0f + 0.5f;
    if (c >= 65535.0f) return 65535u;
    return (uint16_t)c;
}

/* Clamp a possibly-negative int into a u16 without wrapping: a wrapped 65535
 * would read as a huge points total or ammo count on the client. */
static uint16_t net_clamp_u16(int v) {
    if (v <= 0) return 0;
    if (v >= 65535) return 65535u;
    return (uint16_t)v;
}

/* Build the 20 Hz world snapshot on the host. Only semantic (networked)
 * state crosses the wire: entity id (a stable ECS index), kind, pos, vel,
 * hp, and the projectile/owner linkage — plus, as of NET_WIRE_VERSION 4, the
 * entity's appearance (art/size_q/tint).
 *
 * The comment above used to say "Visuals are reconstructed by the client from
 * `kind` (deterministic factories on both sides)". That was the bug: it was
 * never deterministic, because zombie size is rand()'d and the colour variant
 * is a waves.c local that never reaches the ECS. The host now transmits
 * appearance (R13-I5, docs/NET_PROTOCOL_DESIGN.md). The slot map lets each
 * client pick its own player out of the entity list. */
int net_snapshot_build(const World *ecs, const Player *players, int player_count,
                       float sim_time, const WaveSystem *waves, NetSnapshot *out) {
    if (!ecs || !out) return -1;
    *out = (NetSnapshot){0};
    out->sim_time = sim_time;
    if (waves) {
        out->wave_number = (uint16_t)waves->wave_number;
        out->wave_active = waves->wave_active ? 1 : 0;
        out->total_kills = (uint16_t)(waves->total_kills & 0xFFFF);
        out->zombies_alive = (uint16_t)(waves->zombies_alive & 0xFFFF);
        out->between_waves = waves->between_waves ? 1 : 0;
        out->wave_cooldown_centis = net_centis(waves->wave_cooldown_timer);
    }
    for (int s = 0; s < player_count && s < NET_MAX_PLAYERS; s++) {
        if (!players || !players[s].in_use) continue;
        if (players[s].entity != ECS_NULL_ENTITY &&
            ecs->alive[players[s].entity]) {
            out->slot_entities[s] = (uint16_t)players[s].entity;
        }
        /* v6: the client HUD reads THIS, not its own (unsimulated) ECS. Sent
         * for in-use slots even while dead, because a dead slot has no entity
         * to ask - which is precisely when the respawn countdown matters. */
        NetPlayerState *ps = &out->player_states[s];
        const Player *p = &players[s];
        ps->flags = NET_PST_IN_USE;
        if (p->alive) ps->flags |= NET_PST_ALIVE;
        if (p->eliminated) ps->flags |= NET_PST_ELIMINATED;
        ps->weapon = (uint8_t)p->inventory.current;
        ps->unlocked_mask = 0;
        for (int w = 0; w < WEAPON_COUNT; w++) {
            if (p->inventory.unlocked[w]) ps->unlocked_mask |= (uint8_t)(1u << w);
        }
        ps->points = net_clamp_u16(p->inventory.points);
        ps->grenades = net_clamp_u16(p->inventory.grenades);
        ps->launcher_ammo = net_clamp_u16(p->inventory.launcher_ammo);
        ps->respawn_centis = net_centis(p->respawn_timer);
        ps->hp_centis = 0;
        ps->hp_max_centis = 0;
        if (p->entity != ECS_NULL_ENTITY && ecs->alive[p->entity] &&
            (ecs->component_masks[p->entity] & (1u << COMP_HEALTH))) {
            ps->hp_centis = net_centis(ecs->healths[p->entity].current);
            ps->hp_max_centis = net_centis(ecs->healths[p->entity].max);
        }
        ps->beacon_x = p->beacon_pos.x;
        ps->beacon_y = p->beacon_pos.y;
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
        /* v7: send the real maximum. Health bars used to divide by the starting
         * value, which stops being true as soon as difficulty scales it. */
        se->hp_max = (mask & (1u << COMP_HEALTH)) ? ecs->healths[e].max : 0.0f;
        se->flags = 0;
        se->owner = (uint16_t)owner;

        /* Appearance: report the sprite this entity ACTUALLY has, so the client
         * never has to guess (R13-I5). Read off the CSprite rather than derived
         * from `kind` — kind would be a heuristic, and heuristics are what put
         * a circle on screen in the first place.
         *
         * A missing CSprite, or one holding a flat shape, means the host is
         * genuinely drawing a circle: that is reported as NET_ART_NONE so the
         * client reproduces it faithfully instead of substituting art. */
        se->art = NET_ART_NONE;
        se->size_q = 0;
        se->tint[0] = se->tint[1] = se->tint[2] = 0;
        if (mask & (1u << COMP_SPRITE)) {
            const CSprite *cs = &ecs->sprites[e];
            se->tint[0] = net_tint_byte(cs->sprite.color.r);
            se->tint[1] = net_tint_byte(cs->sprite.color.g);
            se->tint[2] = net_tint_byte(cs->sprite.color.b);

            if (cs->sprite.shape == SPRITE_SHAPE_TEXTURE && cs->sprite.texture) {
                const char *name = asset_manager_name_of(cs->sprite.texture);
                uint8_t art = net_art_from_path(name);
                int base = net_art_base_px(art);
                if (base > 0) {
                    /* The wire carries a world DIAMETER, so multiply the host's
                     * own scale by the texture's natural size: scale is
                     * texture-relative, the diameter is not, and a diameter
                     * keeps its meaning if the art is ever rescaled. */
                    float diam = cs->scale * (float)base;
                    se->art = art;
                    int q = (int)(diam * 2.0f + 0.5f);
                    se->size_q = (uint8_t)(q > 255 ? 255 : (q < 0 ? 0 : q));
                } else {
                    /* Textured but not in the art table (unknown texture, or a
                     * table this build does not know). Half-unit circle of the
                     * base rect keeps it visible rather than invisible. */
                    float diam = cs->scale * 16.0f;
                    int q = (int)(diam * 2.0f + 0.5f);
                    se->size_q = (uint8_t)(q > 255 ? 255 : (q < 0 ? 0 : q));
                }
            } else {
                /* Flat shape: circle radius or rect width becomes the diameter,
                 * so the client's circle is the same size as the host's. */
                float diam = (cs->sprite.shape == SPRITE_SHAPE_CIRCLE)
                                 ? cs->sprite.as.circle.radius * 2.0f
                                 : cs->sprite.as.rect.w;
                int q = (int)(diam * 2.0f + 0.5f);
                se->size_q = (uint8_t)(q > 255 ? 255 : (q < 0 ? 0 : q));
            }
        }
    }
    return out->count;
}