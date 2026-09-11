#include "ai/ai_driver.h"
#include "core/log.h"

void ai_driver_init(AIDriver *drv) {
    memset(drv, 0, sizeof(AIDriver));
    drv->mode = AI_MODE_NONE;
    drv->script_loops = 1;
    bot_init(&drv->bot);
    LOG_DEBUG("AI driver initialized (mode: none)");
}

int ai_driver_load_script(AIDriver *drv, const char *path) {
    int rc = script_player_load(&drv->script, path);
    if (rc == 0) {
        script_player_reset(&drv->script);
        drv->mode = AI_MODE_SCRIPT;
        LOG_INFO("AI driver: scripted mode ('%s')", path);
    }
    return rc;
}

void ai_driver_set_script_loops(AIDriver *drv, int loops) {
    drv->script_loops = loops > 0 ? loops : 1;
}

void ai_driver_set_bot(AIDriver *drv) {
    drv->mode = AI_MODE_BOT;
    drv->done = false;
    bot_init(&drv->bot);
    LOG_INFO("AI driver: autonomous bot mode");
}

void ai_driver_update(AIDriver *drv, double dt, const GameView *view, AIControls *out) {
    ai_controls_reset(out);

    switch (drv->mode) {
        case AI_MODE_NONE:
            return;

        case AI_MODE_SCRIPT:
            if (drv->done) {
                /* optionally loop */
                if (drv->script_loops > 1 && drv->loop_count < drv->script_loops) {
                    script_player_reset(&drv->script);
                    drv->loop_count++;
                    drv->done = false;
                } else {
                    return;
                }
            }
            script_player_update(&drv->script, dt, out);
            if (drv->script.done) {
                drv->done = true;
            }
            if (drv->script.quit_requested) {
                drv->done = true;
                drv->quit_requested = true;
            }
            break;

        case AI_MODE_BOT:
            bot_update(&drv->bot, view, dt, out);
            break;
    }
}

void ai_apply_controls(InputState *input, const AIControls *controls, const Camera *cam) {
    if (!input || !controls) return;

    input->ai_controlled = true;

    bool space = controls->action;
    input_inject_key(input, SDL_SCANCODE_W, controls->move_up);
    input_inject_key(input, SDL_SCANCODE_S, controls->move_down);
    input_inject_key(input, SDL_SCANCODE_A, controls->move_left);
    input_inject_key(input, SDL_SCANCODE_D, controls->move_right);
    input_inject_key(input, SDL_SCANCODE_SPACE, space);
    input_inject_key(input, SDL_SCANCODE_B, controls->use_shop);
    input_inject_click(input, 0, controls->shoot);

    /* Weapon selection: re-press 1-4 every frame the driver asks for it
     * (idempotent - selecting the same weapon again is a no-op). */
    if (controls->weapon >= 1 && controls->weapon <= 4) {
        SDL_Scancode sel = SDL_SCANCODE_1 + (controls->weapon - 1);
        input_inject_key(input, sel, true);
        input_inject_key(input, sel, false);
    }

    if (controls->has_aim && cam) {
        Vec2 screen = camera_world_to_screen((Camera *)cam, vec2(controls->aim_x, controls->aim_y));
        input_inject_aim(input, screen.x, screen.y, controls->aim_x, controls->aim_y);
    }
}

void ai_build_view(GameView *view, World *ecs, WaveSystem *waves) {
    ai_view_reset(view);

    view->zombies_alive = waves->zombies_alive;
    view->wave_number = waves->wave_number;

    for (uint32_t i = 0; i < ECS_MAX_ENTITIES; i++) {
        if (!ecs->alive[i]) continue;
        if (!(ecs->component_masks[i] & (1u << COMP_POSITION))) continue;

        Vec2 pos = ecs->positions[i].pos;

        if (ecs->component_masks[i] & (1u << COMP_PLAYER_TAG)) {
            view->player_alive = true;
            view->player_pos = pos;
            if (ecs->component_masks[i] & (1u << COMP_HEALTH)) {
                view->player_health = ecs->healths[i].current;
                view->player_max_health = ecs->healths[i].max;
            }
            continue;
        }
    }

    if (!view->player_alive) return;

    for (uint32_t i = 0; i < ECS_MAX_ENTITIES; i++) {
        if (!ecs->alive[i]) continue;
        if (!(ecs->component_masks[i] & (1u << COMP_POSITION))) continue;

        Vec2 pos = ecs->positions[i].pos;

        if (ecs->component_masks[i] & (1u << COMP_ZOMBIE_TAG)) {
            float d = vec2_distance(pos, view->player_pos);
            if (d < view->nearest_zombie_dist) {
                view->nearest_zombie_dist = d;
                view->nearest_zombie = pos;
            }
        } else if (ecs->component_masks[i] & (1u << COMP_ITEM_TAG)) {
            float d = vec2_distance(pos, view->player_pos);
            if (d < view->nearest_item_dist) {
                view->nearest_item_dist = d;
                view->nearest_item = pos;
            }
        }
    }
}