#include "systems/systems.h"

#include "ecs/ecs.h"
#include "graphics/anim.h"
#include "graphics/sprite.h"

/* The animation system: advance the frame, then SHOW the frame.
 *
 * Showing it means putting the frame's texture into the entity's sprite. That
 * is deliberate. The sprite is what the renderer draws AND what the snapshot
 * builder reports - it reads the texture's name and turns it into an art id.
 * So the host's screen and the client's screen show the same frame because
 * there is one answer to "which frame is this", not because two code paths
 * happen to agree. A second mapping, anywhere else, is a bug waiting for the
 * day the two disagree (docs/knowledge/animation-transport.md). */

/* Put frame `frame` of the walk cycle on the entity's sprite. The texture
 * comes from the asset manager, so this is a cached lookup. When the art is
 * missing (headless builds, an incomplete asset tree) the sprite is left
 * exactly as it was: the entity keeps the flat shape it always had rather
 * than disappearing. */
static void anim_show_frame(World *ecs, uint32_t slot) {
    if (!(ecs->component_masks[slot] & (1u << COMP_SPRITE))) return;

    CAnimation *anim = &ecs->animations[slot];
    SDL_Texture *tex = anim_walk_texture(anim->current_frame);
    if (!tex) return;

    CSprite *spr = &ecs->sprites[slot];
    if (spr->sprite.shape == SPRITE_SHAPE_TEXTURE && spr->sprite.texture == tex) {
        return;   /* already showing this frame */
    }
    spr->sprite.shape = SPRITE_SHAPE_TEXTURE;
    spr->sprite.texture = tex;
    spr->sprite.src = (SDL_FRect){0.0f, 0.0f, 0.0f, 0.0f};  /* whole texture */
}

void system_animation(World *ecs, float dt) {
    for (uint32_t i = 0; i < ECS_MAX_ENTITIES; i++) {
        if (!ecs->alive[i]) continue;
        if (!(ecs->component_masks[i] & (1u << COMP_ANIMATION))) continue;

        CAnimation *anim = &ecs->animations[i];
        anim->frame_timer += dt;
        if (anim->frame_duration > 0.0f && anim->frame_timer >= anim->frame_duration) {
            anim->frame_timer -= anim->frame_duration;
            anim->current_frame++;
            if (anim->current_frame >= anim->total_frames) {
                anim->current_frame = 0;
            }
        }
        anim_show_frame(ecs, i);
    }
}
