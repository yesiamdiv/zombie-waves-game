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

        /* The pose follows the state (AN5). A zombie hitting you should not
         * look like it is walking in place:
         *   CHASE  - the cycle advances (the only state that walks);
         *   ATTACK - hold the lunge frame, timer reset so leaving the state
         *            resumes from the next frame;
         *   HURT   - freeze whatever frame it was on (the white flash already
         *            says "hurt");
         *   IDLE   - hold frame 0, the stationary pose;
         *   DEAD   - freeze (the entity is destroyed by system_cleanup, so
         *            this only matters if it lingers for a frame).
         * An animated entity without a zombie tag just advances the cycle. */
        const CZombieTag *ztag = NULL;
        if (ecs->component_masks[i] & (1u << COMP_ZOMBIE_TAG)) {
            ztag = &ecs->zombie_tags[i];
        }

        if (ztag && ztag->state == ZOMBIE_ATTACK) {
            if (anim->current_frame != ANIM_ATTACK_FRAME) {
                anim->current_frame = ANIM_ATTACK_FRAME;
                anim->frame_timer = 0.0f;
            }
            anim_show_frame(ecs, i);
            continue;
        }
        if (ztag && (ztag->state == ZOMBIE_HURT || ztag->state == ZOMBIE_DEAD)) {
            anim_show_frame(ecs, i);   /* freeze on the current frame */
            continue;
        }
        if (ztag && ztag->state == ZOMBIE_IDLE) {
            if (anim->current_frame != 0) {
                anim->current_frame = 0;
                anim->frame_timer = 0.0f;
            }
            anim_show_frame(ecs, i);
            continue;
        }

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
