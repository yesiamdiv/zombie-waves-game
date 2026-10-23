#ifndef NET_ART_H
#define NET_ART_H

#include <stdbool.h>
#include <stdint.h>

/* Appearance ids for the shared art table (docs/VERSIONING.md, and
 * docs/NET_PROTOCOL_DESIGN.md ADR-2).
 *
 * This header owns the enum deliberately: the art table must be usable without
 * dragging in the rest of net.h (and through it the ECS and player tables).
 *
 * APPEND ONLY. Never renumber or reuse a released id. A peer that meets an id it
 * does not know draws NET_ART_NONE (a circle), which is visibly wrong; a stale
 * id that silently means a different sprite is invisibly wrong. */
enum {
    NET_ART_NONE    = 0,   /* host is drawing a flat circle: reproduce it */
    NET_ART_PLAYER  = 1,
    NET_ART_ZOMBIE  = 2,
    NET_ART_BULLET  = 3,
    NET_ART_GRENADE = 4,
    NET_ART_ROCKET  = 5,
    NET_ART_MEDKIT  = 6,
    NET_ART_AMMO    = 7,
    NET_ART_SPEED   = 8,
    NET_ART_SWORD   = 9,
    /* The zombie walk cycle: ONE FILE PER FRAME, so a frame can be named and
     * therefore travel as an art id (docs/knowledge/animation-transport.md).
     * The block is contiguous on purpose - a frame number maps to an id with
     * NET_ART_ZOMBIE_WALK0 + frame. */
    NET_ART_ZOMBIE_WALK0 = 10,
    NET_ART_ZOMBIE_WALK1 = 11,
    NET_ART_ZOMBIE_WALK2 = 12,
    NET_ART_ZOMBIE_WALK3 = 13,
    NET_ART_ZOMBIE_WALK4 = 14,
    NET_ART_ZOMBIE_WALK5 = 15,
    NET_ART_ZOMBIE_WALK6 = 16,
    NET_ART_ZOMBIE_WALK7 = 17,
    /* The player walk cycle: same rule, one file per frame. */
    NET_ART_PLAYER_WALK0 = 18,
    NET_ART_PLAYER_WALK1 = 19,
    NET_ART_PLAYER_WALK2 = 20,
    NET_ART_PLAYER_WALK3 = 21,
    NET_ART_PLAYER_WALK4 = 22,
    NET_ART_PLAYER_WALK5 = 23,
    NET_ART_PLAYER_WALK6 = 24,
    NET_ART_PLAYER_WALK7 = 25,
    NET_ART_COUNT   = 26
};

/* How many frames the zombie walk cycle has. */
#define NET_ART_ZOMBIE_WALK_COUNT 8
/* How many frames the player walk cycle has. */
#define NET_ART_PLAYER_WALK_COUNT 8

/* One row of the shared art table. `base_px` is the PNG's natural width; it
 * converts the snapshot's world diameter into a draw scale. */
typedef struct {
    const char *path;   /* NULL for NET_ART_NONE */
    int base_px;
} NetArtSpec;

const char *net_art_path(uint8_t art);
int net_art_base_px(uint8_t art);
/* Host side: map an asset path back to its art id, or NET_ART_NONE. Lets the
 * snapshot builder read the id off the entity's own sprite instead of guessing
 * from the entity kind. */
uint8_t net_art_from_path(const char *path);

#endif /* NET_ART_H */