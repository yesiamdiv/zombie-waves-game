#include "systems/systems.h"
#include "events/event_bus.h"
#include "items/items.h"
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
        float draw_rot = 0.0f;

        /* The sword blade is drawn rotated along its orbital angle. */
        if (ecs->component_masks[i] & (1u << COMP_SWORD_TAG)) {
            draw_rot = ecs->sword_tags[i].angle;
        }

        /* Flash white when zombie is hurt */
        if (ecs->component_masks[i] & (1u << COMP_ZOMBIE_TAG)) {
            CZombieTag *ztag = &ecs->zombie_tags[i];
            if (ztag->hurt_timer > 0) {
                draw_alpha = 0.5f + ztag->hurt_timer * 3.0f;
            }
        }

        sprite_draw(renderer, &spr->sprite, screen.x, screen.y,
                     draw_scale * cam->zoom, draw_rot, draw_alpha);

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

void system_render_beacons(SDL_Renderer *renderer, Camera *cam,
                           const Player *players, int player_count) {
    if (!renderer || !cam || !players) return;

    for (int s = 0; s < player_count; s++) {
        const Player *p = &players[s];
        if (!p->in_use) continue;

        if (!camera_is_visible(cam, p->beacon_pos, 60.0f)) continue;
        Vec2 screen = camera_world_to_screen(cam, p->beacon_pos);
        float z = cam->zoom;

        /* Base pad: dark slab so the beacon reads against any ground. */
        SDL_SetRenderDrawColorFloat(renderer, 0.12f, 0.12f, 0.15f, 0.85f);
        SDL_RenderFillRect(renderer, &(SDL_FRect){
            screen.x - 34.0f * z, screen.y - 34.0f * z, 68.0f * z, 68.0f * z});

        /* Color core: the player's color - the color-coding part. */
        SDL_SetRenderDrawColorFloat(renderer, p->color.r, p->color.g, p->color.b, 0.9f);
        SDL_RenderFillRect(renderer, &(SDL_FRect){
            screen.x - 24.0f * z, screen.y - 24.0f * z, 48.0f * z, 48.0f * z});

        /* Bright center so the marker is visible from a distance. */
        SDL_SetRenderDrawColorFloat(renderer, 1.0f, 1.0f, 1.0f, 1.0f);
        SDL_RenderFillRect(renderer, &(SDL_FRect){
            screen.x - 7.0f * z, screen.y - 7.0f * z, 14.0f * z, 14.0f * z});
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

void system_cleanup(World *ecs, WaveSystem *waves, Player *players, int player_count) {
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

            /* Credit whoever last damaged this zombie - resolved through the
             * slot table so kills land in the right player's inventory. */
            Entity killer = ecs->zombie_tags[i].last_hit_by;
            int slot = players_find_index(players, player_count, killer);
            if (slot < 0) {
                /* No recorded shooter (e.g. an unattributed source): fall back
                 * to the first resident slot. */
                for (int s = 0; s < player_count && slot < 0; s++) {
                    if (players && players[s].in_use) slot = s;
                }
            }
            if (slot >= 0) {
                weapons_award_kill(&players[slot].inventory, 1);
                players[slot].kills++;
            }

            /* Drop a pickup at the kill site (near the action) so the heal
             * economy is actually reachable by the bot/player (B5). Gated by
             * the live-item cap so long survival runs stay bounded (B6). */
            if ((rand() % 100) < 40 && items_count_alive(ecs) < MAX_ALIVE_ITEMS) {
                items_spawn_random(ecs, pos->pos);
            }

            /* Death particles */
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
