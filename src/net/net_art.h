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
    NET_ART_COUNT   = 10
};

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