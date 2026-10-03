#include "net_art.h"

#include <string.h>

/* The shared art table (docs/NET_PROTOCOL_DESIGN.md ADR-2).
 *
 * Both peers compile this in, so art id N means the same PNG on each side.
 * That is the whole reason ids exist instead of paths: repeating the path on
 * the wire at 20 Hz per entity would cost bytes to say something both builds
 * already know.
 *
 * TWO RULES, both load-bearing:
 *
 *  1. APPEND ONLY. Never renumber or reuse a released id. A peer that does not
 *     recognise an id falls back to NET_ART_NONE and draws a circle, which is
 *     visibly wrong; the alternative — a stale id meaning a different sprite —
 *     is invisibly wrong.
 *
 *  2. `base_px` must match the real PNG's width. It converts the wire
 *     diameter into a draw scale. NET_ASSET_VERSION (sprint N2) is what stops
 *     two peers disagreeing about these rows.
 */
static const NetArtSpec k_art[NET_ART_COUNT] = {
    [NET_ART_NONE]    = { NULL,                             0 },
    [NET_ART_PLAYER]  = { "textures/entities/player.png",   32 },
    [NET_ART_ZOMBIE]  = { "textures/entities/zombie.png",   32 },
    [NET_ART_BULLET]  = { "textures/entities/bullet.png",    8 },
    [NET_ART_GRENADE] = { "textures/entities/grenade.png",  32 },
    [NET_ART_ROCKET]  = { "textures/entities/rocket.png",   32 },
    [NET_ART_MEDKIT]  = { "textures/entities/medkit.png",   32 },
    [NET_ART_AMMO]    = { "textures/entities/ammo.png",     32 },
    [NET_ART_SPEED]   = { "textures/entities/speed.png",    32 },
    [NET_ART_SWORD]   = { "textures/entities/sword_blade.png", 40 },
};

const char *net_art_path(uint8_t art) {
    if (art >= NET_ART_COUNT) return NULL;
    return k_art[art].path;
}

int net_art_base_px(uint8_t art) {
    if (art >= NET_ART_COUNT) return 0;
    return k_art[art].base_px;
}

uint8_t net_art_from_path(const char *path) {
    if (!path || !*path) return NET_ART_NONE;
    for (int i = 0; i < NET_ART_COUNT; i++) {
        const NetArtSpec *s = &k_art[i];
        if (s->path && strcmp(s->path, path) == 0) return (uint8_t)i;
    }
    return NET_ART_NONE;
}