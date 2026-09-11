#include "ai/bot.h"
#include "core/log.h"
#include <stdlib.h>

void bot_init(BotState *bot) {
    bot->detection_range = 600.0f;
    bot->standoff_range  = 260.0f;
    bot->flee_range      = 70.0f;
    bot->fire_cooldown   = 0.0f;
    bot->fire_rate       = 0.12f;
    bot->strafe_dir      = 1.0f;
    bot->strafe_timer    = 0.0f;
    LOG_INFO("Bot initialized (detect=%.0f standoff=%.0f flee=%.0f)",
             bot->detection_range, bot->standoff_range, bot->flee_range);
}

void bot_update(BotState *bot, const GameView *view, double dt, AIControls *out) {
    ai_controls_reset(out);

    if (!view->player_alive) return;

    bot->fire_cooldown -= (float)dt;
    bot->strafe_timer -= (float)dt;
    if (bot->strafe_timer <= 0.0f) {
        bot->strafe_dir = (rand() % 2) ? 1.0f : -1.0f;
        bot->strafe_timer = 1.5f + (float)(rand() % 100) / 50.0f;
    }

    bool heal_seek = view->player_health < view->player_max_health * 0.75f &&
                     view->nearest_item_dist < 500.0f;

    if (view->zombies_alive <= 0) {
        /* Nothing to fight: if hurt, go pick up a nearby medkit. */
        if (heal_seek) {
            Vec2 dir = vec2_sub(view->nearest_item, view->player_pos);
            float dist = vec2_length(dir);
            Vec2 to_it = dist > 0.001f ? vec2_scale(dir, 1.0f / dist)
                                       : vec2(1.0f, 0.0f);
            /* Drift sideways so wall-slide gets us around obstacles (B1). */
            Vec2 perp = vec2(-to_it.y * bot->strafe_dir, to_it.x * bot->strafe_dir);
            Vec2 move = vec2_add(vec2_scale(to_it, 1.0f), vec2_scale(perp, 0.3f));
            float ml = vec2_length(move);
            if (ml > 0.001f) move = vec2_scale(move, 1.0f / ml);

            out->move_up    = move.y < -0.2f;
            out->move_down  = move.y >  0.2f;
            out->move_left  = move.x < -0.2f;
            out->move_right = move.x >  0.2f;
            out->aim_x = view->nearest_item.x;
            out->aim_y = view->nearest_item.y;
            out->has_aim = true;
        }
        return;
    }

    Vec2 target = view->nearest_zombie;
    bool seek_item = heal_seek && view->nearest_item_dist < view->nearest_zombie_dist;
    if (seek_item) {
        /* Low on HP and a medkit is closer than the fight: grab it first. */
        target = view->nearest_item;
    }

    Vec2 dir = vec2_sub(target, view->player_pos);
    float dist = vec2_length(dir);
    Vec2 to_target = dist > 0.001f ? vec2_scale(dir, 1.0f / dist)
                                   : vec2(1.0f, 0.0f);

    /* Always engage the nearest zombie, even beyond the ranged-fire window,
     * so a straggler that spawned at ring edge is hunted down instead of
     * soft-locking the wave (B1). */
    if (dist > bot->standoff_range + 20.0f) {
        /* Approach with a perpendicular drift so diagonal motion engages the
         * movement system's wall-slide and the bot can slip around obstacles
         * the way a human would, instead of wedging on wall faces. */
        Vec2 perp = vec2(-to_target.y * bot->strafe_dir, to_target.x * bot->strafe_dir);
        Vec2 move = vec2_add(vec2_scale(to_target, 1.0f), vec2_scale(perp, 0.3f));
        float ml = vec2_length(move);
        if (ml > 0.001f) move = vec2_scale(move, 1.0f / ml);

        out->move_up    = move.y < -0.2f;
        out->move_down  = move.y >  0.2f;
        out->move_left  = move.x < -0.2f;
        out->move_right = move.x >  0.2f;
    } else if (dist < bot->flee_range) {
        /* back away */
        out->move_up    = to_target.y >  0.2f;
        out->move_down  = to_target.y < -0.2f;
        out->move_left  = to_target.x >  0.2f;
        out->move_right = to_target.x < -0.2f;
    } else {
        /* strafe perpendicular */
        Vec2 perp = vec2(-to_target.y * bot->strafe_dir, to_target.x * bot->strafe_dir);
        out->move_up    = perp.y < -0.2f;
        out->move_down  = perp.y >  0.2f;
        out->move_left  = perp.x < -0.2f;
        out->move_right = perp.x >  0.2f;
    }

    /* Aim at and shoot the zombie, not the item. */
    out->aim_x = view->nearest_zombie.x;
    out->aim_y = view->nearest_zombie.y;
    out->has_aim = true;

    float zdist = vec2_distance(view->nearest_zombie, view->player_pos);
    if (!seek_item && zdist < bot->detection_range - 50.0f && bot->fire_cooldown <= 0.0f) {
        out->shoot = true;
        bot->fire_cooldown = bot->fire_rate;
    }
}