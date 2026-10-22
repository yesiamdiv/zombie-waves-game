#include "graphics/anim.h"

#include "graphics/sprite.h"
#include "net/net_art.h"

/* The walk frames are one contiguous block of art ids, so the frame number
 * maps to an id by adding. If the block is ever split, this fires at compile
 * time instead of the client quietly showing the wrong frame. */
_Static_assert((int)NET_ART_ZOMBIE_WALK7 ==
                   (int)NET_ART_ZOMBIE_WALK0 + NET_ART_ZOMBIE_WALK_COUNT - 1,
               "the walk frames must be one contiguous block of art ids");

uint8_t anim_walk_art_id(int frame) {
    int f = frame % NET_ART_ZOMBIE_WALK_COUNT;
    if (f < 0) f += NET_ART_ZOMBIE_WALK_COUNT;
    return (uint8_t)(NET_ART_ZOMBIE_WALK0 + f);
}

const char *anim_walk_path(int frame) {
    return net_art_path(anim_walk_art_id(frame));
}

SDL_Texture *anim_walk_texture(int frame) {
    const char *path = anim_walk_path(frame);
    return path ? sprite_tex(path) : NULL;
}
