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

    if (view->zombies_alive <= 0 || view->nearest_zombie_dist > bot->detection_range) {
        return;  /* nothing to fight */
    }

    if (!view->player_alive) return;

    Vec2 dir = vec2_sub(view->nearest_zombie, view->player_pos);
    float dist = vec2_length(dir);
    Vec2 to_zombie = dist > 0.001f ? vec2_scale(dir, 1.0f / dist)
                                   : vec2(1.0f, 0.0f);

    if (dist > bot->standoff_range + 20.0f) {
        /* approach */
        out->move_up    = to_zombie.y < -0.2f;
        out->move_down  = to_zombie.y >  0.2f;
        out->move_left  = to_zombie.x < -0.2f;
        out->move_right = to_zombie.x >  0.2f;
    } else if (dist < bot->flee_range) {
        /* back away */
        out->move_up    = to_zombie.y >  0.2f;
        out->move_down  = to_zombie.y < -0.2f;
        out->move_left  = to_zombie.x >  0.2f;
        out->move_right = to_zombie.x < -0.2f;
    } else {
        /* strafe perpendicular */
        Vec2 perp = vec2(-to_zombie.y * bot->strafe_dir, to_zombie.x * bot->strafe_dir);
        out->move_up    = perp.y < -0.2f;
        out->move_down  = perp.y >  0.2f;
        out->move_left  = perp.x < -0.2f;
        out->move_right = perp.x >  0.2f;
    }

    out->aim_x    = view->nearest_zombie.x;
    out->aim_y    = view->nearest_zombie.y;
    out->has_aim  = true;

    if (dist < bot->detection_range - 50.0f && bot->fire_cooldown <= 0.0f) {
        out->shoot = true;
        bot->fire_cooldown = bot->fire_rate;
    }
}