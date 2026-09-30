#include "systems/systems.h"
#include "net/net_mirror.h"
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

        /* Sweeping sword: draw a blade that pivots at the handle point (inner
         * circle) and extends to the tip (outer circle). */
        if (ecs->component_masks[i] & (1u << COMP_SWORD_TAG)) {
            const CSwordTag *sword = &ecs->sword_tags[i];
            float length = sword->outer_radius - sword->radius;
            float width = spr->sprite.as.rect.w > 0.0f ? spr->sprite.as.rect.w : 8.0f;
            sprite_draw_blade(renderer, spr->sprite.texture,
                              screen.x, screen.y, sword->angle,
                              length, width, cam->zoom,
                              spr->sprite.color, spr->base_alpha);
            continue;
        }

        float draw_alpha = spr->base_alpha;
        float draw_scale = spr->scale;
        float draw_rot = 0.0f;

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

/* Render the interpolated snapshot mirror (render-only net clients).
 * `render_time` is the local render clock; entities are drawn at the blend
 * between the mirror's older/newer 20 Hz snapshots. Visuals are reconstructed
 * on the client from `kind` (mirror does not carry sprite data). */
void system_render_mirror(SDL_Renderer *renderer, Camera *cam,
                          const NetMirror *mirror, float render_time) {
    if (!renderer || !cam || !mirror || !net_mirror_ready(mirror)) return;

    float t_new = net_mirror_newer_time(mirror);
    float t_old = net_mirror_older_time(mirror);
    float t = 1.0f;
    if (t_new > t_old) {
        t = (render_time - t_old) / (t_new - t_old);
        if (t < 0.0f) t = 0.0f;
        else if (t > 1.0f) t = 1.0f;
    }

    const NetSnapshot *n = &mirror->newer;
    for (int i = 0; i < n->count; i++) {
        NetEntitySnap e;
        if (!net_mirror_sample(mirror, n->entities[i].id, t, &e)) continue;
        if (!camera_is_visible(cam, e.pos, 60.0f)) continue;

        Vec2 screen = camera_world_to_screen(cam, e.pos);
        float z = cam->zoom;
        SDL_FColor color = COLOR_WHITE;
        float size = 8.0f;

        switch (e.kind) {
            case NET_ENT_PLAYER: {
                int slot = net_mirror_slot_for(mirror, e.id);
                color = net_slot_color(slot < 0 ? 1 : slot);
                size = 10.0f;
                break;
            }
            case NET_ENT_ZOMBIE:
                color = COLOR_GREEN;
                size = 11.0f;
                break;
            case NET_ENT_BULLET:
                color = color_rgb(1.0f, 0.9f, 0.3f);
                size = 3.0f;
                break;
            case NET_ENT_GRENADE:
                color = COLOR_DARK_GREEN;
                size = 5.0f;
                break;
            case NET_ENT_ROCKET:
                color = COLOR_ORANGE;
                size = 5.0f;
                break;
            case NET_ENT_ITEM:
                color = COLOR_CYAN;
                size = 7.0f;
                break;
            default:
                continue;
        }

        Sprite s = sprite_circle(size, color);
        sprite_draw(renderer, &s, screen.x, screen.y, z, 0.0f, 1.0f);

        /* Health bar over damaged/player entities. */
        if (e.kind == NET_ENT_PLAYER || e.kind == NET_ENT_ZOMBIE) {
            float bar_h = 5.0f * z;
            float bar_w = (e.kind == NET_ENT_PLAYER ? 30.0f : 24.0f) * z;
            float ratio = e.hp / (e.kind == NET_ENT_PLAYER ? 200.0f : 100.0f);
            SDL_SetRenderDrawColorFloat(renderer, 0.15f, 0.15f, 0.15f, 0.9f);
            SDL_RenderFillRect(renderer, &(SDL_FRect){
                screen.x - bar_w * 0.5f, screen.y - 24.0f * z, bar_w, bar_h});
            SDL_SetRenderDrawColorFloat(renderer, 0.2f, 0.9f, 0.3f, 0.9f);
            SDL_RenderFillRect(renderer, &(SDL_FRect){
                screen.x - bar_w * 0.5f, screen.y - 24.0f * z,
                bar_w * ratio, bar_h});
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
