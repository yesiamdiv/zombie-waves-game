#include "graphics/anim.h"

#include "graphics/sprite.h"
#include "net/net_art.h"

/* The walk frames are one contiguous block of art ids, so the frame number
 * maps to an id by adding. If the block is ever split, this fires at compile
 * time instead of the client quietly showing the wrong frame. */
_Static_assert((int)NET_ART_ZOMBIE_WALK7 ==
                   (int)NET_ART_ZOMBIE_WALK0 + NET_ART_ZOMBIE_WALK_COUNT - 1,
               "the walk frames must be one contiguous block of art ids");
_Static_assert((int)NET_ART_PLAYER_WALK7 ==
                   (int)NET_ART_PLAYER_WALK0 + NET_ART_PLAYER_WALK_COUNT - 1,
               "the player walk frames must be one contiguous block of art ids");

/* Frame -> id for one contiguous block, with a negative frame wrapping too.
 * Shared by both cycles so they cannot wrap differently. */
static uint8_t frame_art_id(int base, int count, int frame) {
    int f = frame % count;
    if (f < 0) f += count;
    return (uint8_t)(base + f);
}

uint8_t anim_walk_art_id(int frame) {
    return frame_art_id(NET_ART_ZOMBIE_WALK0, NET_ART_ZOMBIE_WALK_COUNT, frame);
}

uint8_t anim_player_walk_art_id(int frame) {
    return frame_art_id(NET_ART_PLAYER_WALK0, NET_ART_PLAYER_WALK_COUNT, frame);
}

const char *anim_walk_path(int frame) {
    return net_art_path(anim_walk_art_id(frame));
}

SDL_Texture *anim_walk_texture(int frame) {
    const char *path = anim_walk_path(frame);
    return path ? sprite_tex(path) : NULL;
}

const char *anim_player_walk_path(int frame) {
    return net_art_path(anim_player_walk_art_id(frame));
}

SDL_Texture *anim_player_walk_texture(int frame) {
    const char *path = anim_player_walk_path(frame);
    return path ? sprite_tex(path) : NULL;
}
