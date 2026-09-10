#include "systems/systems.h"
#include "events/event_bus.h"
#include <stdlib.h>

void system_render(World *ecs, SDL_Renderer *renderer, Camera *cam) {
    for (uint32_t i = 0; i < ECS_MAX_ENTITIES; i++) {
        if (!ecs->alive[i]) continue;
        if (!(ecs->component_masks[i] & (1u << COMP_POSITION))) continue;
        if (!(ecs->component_masks[i] & (1u << COMP_SPRITE))) continue;

        CPosition *pos = &ecs->positions[i];
        CSprite *spr = &ecs->sprites[i];

        if (!camera_is_visible(cam, pos->pos, 50.0f)) continue;

        Vec2 screen = camera_world_to_screen(cam, pos->pos);
        float draw_alpha = spr->base_alpha;
        float draw_scale = spr->scale;

        /* Flash white when zombie is hurt */
        if (ecs->component_masks[i] & (1u << COMP_ZOMBIE_TAG)) {
            CZombieTag *ztag = &ecs->zombie_tags[i];
            if (ztag->hurt_timer > 0) {
                draw_alpha = 0.5f + ztag->hurt_timer * 3.0f;
            }
        }

        sprite_draw(renderer, &spr->sprite, screen.x, screen.y,
                     draw_scale * cam->zoom, 0.0f, draw_alpha);

        /* Draw health bar for damaged entities */
        if ((ecs->component_masks[i] & (1u << COMP_HEALTH)) &&
            (ecs->component_masks[i] & (1u << COMP_ZOMBIE_TAG))) {
            CHealth *hp = &ecs->healths[i];
            if (hp->current < hp->max) {
                float bar_w = 24.0f * cam->zoom;
                float bar_h = 4.0f * cam->zoom;
                float bar_x = screen.x - bar_w * 0.5f;
                float bar_y = screen.y - 20.0f * cam->zoom;

                /* Background */
                SDL_SetRenderDrawColorFloat(renderer, 0.2f, 0.2f, 0.2f, 0.8f);
                SDL_FRect bg = {bar_x, bar_y, bar_w, bar_h};
                SDL_RenderFillRect(renderer, &bg);

                /* Health */
                float ratio = hp->current / hp->max;
                SDL_SetRenderDrawColorFloat(renderer, 1.0f - ratio, ratio, 0.2f, 0.9f);
                SDL_FRect fill = {bar_x, bar_y, bar_w * ratio, bar_h};
                SDL_RenderFillRect(renderer, &fill);
            }
        }

        /* Draw player health bar */
        if (ecs->component_masks[i] & (1u << COMP_PLAYER_TAG)) {
            if (ecs->component_masks[i] & (1u << COMP_HEALTH)) {
                CHealth *hp = &ecs->healths[i];
                float bar_w = 30.0f * cam->zoom;
                float bar_h = 5.0f * cam->zoom;
                float bar_x = screen.x - bar_w * 0.5f;
                float bar_y = screen.y - 24.0f * cam->zoom;

                SDL_SetRenderDrawColorFloat(renderer, 0.15f, 0.15f, 0.15f, 0.9f);
                SDL_FRect bg = {bar_x, bar_y, bar_w, bar_h};
                SDL_RenderFillRect(renderer, &bg);

                float ratio = hp->current / hp->max;
                SDL_SetRenderDrawColorFloat(renderer, 0.2f, 0.9f, 0.3f, 0.9f);
                SDL_FRect fill = {bar_x, bar_y, bar_w * ratio, bar_h};
                SDL_RenderFillRect(renderer, &fill);
            }
        }
    }
}

void system_animation(World *ecs, float dt) {
    for (uint32_t i = 0; i < ECS_MAX_ENTITIES; i++) {
        if (!ecs->alive[i]) continue;
        if (!(ecs->component_masks[i] & (1u << COMP_ANIMATION))) continue;

        CAnimation *anim = &ecs->animations[i];
        anim->frame_timer += dt;
        if (anim->frame_timer >= anim->frame_duration) {
            anim->frame_timer -= anim->frame_duration;
            anim->current_frame++;
            if (anim->current_frame >= anim->total_frames) {
                anim->current_frame = 0;
            }
        }
    }
}

void system_cleanup(World *ecs, WaveSystem *waves) {
    /* Check for dead entities (health <= 0) and destroy them */
    for (uint32_t i = 0; i < ECS_MAX_ENTITIES; i++) {
        if (!ecs->alive[i]) continue;
        if (!(ecs->component_masks[i] & (1u << COMP_HEALTH))) continue;

        CHealth *hp = &ecs->healths[i];
        if (hp->current > 0) continue;

        CPosition *pos = &ecs->positions[i];

        /* Classify the dead entity */
        if (ecs->component_masks[i] & (1u << COMP_ZOMBIE_TAG)) {
            LOG_DEBUG("Zombie %u killed", i);
            event_emit(g_events, GE_KILL, i, GEK_ZOMBIE, pos->pos.x, pos->pos.y,
                       0, 0, 0, 0);
            if (waves) {
                waves_on_zombie_killed(waves);
            }

            /* Death particles */
            CSprite *spr = &ecs->sprites[i];
            for (int p = 0; p < 8; p++) {
                Entity particle = ecs_create_entity(ecs);
                if (particle == ECS_NULL_ENTITY) continue;

                ecs_add_component(ecs, particle, COMP_POSITION);
                ecs_add_component(ecs, particle, COMP_SPRITE);
                ecs_add_component(ecs, particle, COMP_PARTICLE);

                float angle = (float)p / 8.0f * 2.0f * M_PI;
                *ecs_get_position(ecs, particle) = (CPosition){{pos->pos.x, pos->pos.y}};
                *ecs_get_particle(ecs, particle) = (CParticle){
                    .lifetime = 0.5f + (float)(rand() % 5) * 0.1f,
                    .max_lifetime = 1.0f,
                    .vel = vec2_from_angle(angle, 80.0f + (float)(rand() % 60)),
                    .size_decay = 1.0f
                };
                *ecs_get_sprite(ecs, particle) = (CSprite){
                    .sprite = sprite_circle(3.0f + (float)(rand() % 3), COLOR_DARK_RED),
                    .scale = 1.0f,
                    .base_alpha = 1.0f
                };
                event_emit(g_events, GE_ENTITY_SPAWN, particle, GEK_PARTICLE,
                           pos->pos.x, pos->pos.y, 0, 0, 0, 0);
            }
        } else if (ecs->component_masks[i] & (1u << COMP_PLAYER_TAG)) {
            LOG_INFO("Player %u died", i);
            event_emit(g_events, GE_KILL, i, GEK_PLAYER, pos->pos.x, pos->pos.y,
                       0, 0, 0, 0);
        } else {
            event_emit(g_events, GE_ENTITY_DEATH, i, GEK_NONE,
                       pos->pos.x, pos->pos.y, 0, 0, 0, 0);
        }

        ecs_destroy_entity(ecs, i);
    }
}
