#include "core/input.h"
#include "core/log.h"
#include "events/event_bus.h"
#include <string.h>

void input_init(InputState *input) {
    memset(input, 0, sizeof(InputState));
    LOG_DEBUG("Input system initialized");
}

void input_process_event(InputState *input, const SDL_Event *event) {
    switch (event->type) {
        case SDL_EVENT_KEY_DOWN:
            if (event->key.scancode < SDL_SCANCODE_COUNT) {
                if (!input->keys[event->key.scancode]) {
                    input->keys_pressed[event->key.scancode] = true;
                    event_emit(g_events, GE_INPUT, ECS_NULL_ENTITY, GEK_NONE,
                               0, 0, (float)event->key.scancode, 1.0f, 0, 0);
                }
                input->keys[event->key.scancode] = true;
            }
            break;

        case SDL_EVENT_KEY_UP:
            if (event->key.scancode < SDL_SCANCODE_COUNT) {
                input->keys[event->key.scancode] = false;
                input->keys_released[event->key.scancode] = true;
                event_emit(g_events, GE_INPUT, ECS_NULL_ENTITY, GEK_NONE,
                           0, 0, (float)event->key.scancode, 0.0f, 0, 0);
            }
            break;

        case SDL_EVENT_MOUSE_MOTION:
            input->mouse_x = event->motion.x;
            input->mouse_y = event->motion.y;
            break;

        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (event->button.button <= 5) {
                int idx = event->button.button - 1;
                if (!input->mouse_buttons[idx]) {
                    input->mouse_pressed[idx] = true;
                }
                input->mouse_buttons[idx] = true;
            }
            break;

        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (event->button.button <= 5) {
                int idx = event->button.button - 1;
                input->mouse_buttons[idx] = false;
                input->mouse_released[idx] = true;
            }
            break;

        case SDL_EVENT_QUIT:
            input->quit_requested = true;
            break;

        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            input->quit_requested = true;
            break;
    }
}

void input_update(InputState *input) {
    memset(input->keys_pressed, 0, sizeof(input->keys_pressed));
    memset(input->keys_released, 0, sizeof(input->keys_released));
    memset(input->mouse_pressed, 0, sizeof(input->mouse_pressed));
    memset(input->mouse_released, 0, sizeof(input->mouse_released));
    input->ai_controlled = false;
}

bool input_key_pressed(const InputState *input, SDL_Scancode key) {
    if (key >= SDL_SCANCODE_COUNT) return false;
    return input->keys_pressed[key];
}

bool input_key_held(const InputState *input, SDL_Scancode key) {
    if (key >= SDL_SCANCODE_COUNT) return false;
    return input->keys[key];
}

bool input_key_released(const InputState *input, SDL_Scancode key) {
    if (key >= SDL_SCANCODE_COUNT) return false;
    return input->keys_released[key];
}

bool input_mouse_pressed(const InputState *input, int button) {
    if (button < 0 || button > 4) return false;
    return input->mouse_pressed[button];
}

/* --- Input injection (AI / scripts / tests) --- */

void input_inject_key(InputState *input, SDL_Scancode key, bool down) {
    if (!input || key >= SDL_SCANCODE_COUNT) return;

    bool was_down = input->keys[key];
    if (down && !was_down) {
        input->keys_pressed[key] = true;
        event_emit(g_events, GE_INPUT, ECS_NULL_ENTITY, GEK_NONE,
                   0, 0, (float)key, 1.0f, 1, 0);
    } else if (!down && was_down) {
        input->keys_released[key] = true;
        event_emit(g_events, GE_INPUT, ECS_NULL_ENTITY, GEK_NONE,
                   0, 0, (float)key, 0.0f, 1, 0);
    }
    input->keys[key] = down;
}

void input_inject_click(InputState *input, int button, bool down) {
    if (!input || button < 0 || button > 4) return;

    bool was_down = input->mouse_buttons[button];
    if (down && !was_down) {
        input->mouse_pressed[button] = true;
        event_emit(g_events, GE_INPUT, ECS_NULL_ENTITY, GEK_NONE,
                   input->mouse_world_x, input->mouse_world_y,
                   (float)button + 1.0f, 1.0f, 1, 0);
    } else if (!down && was_down) {
        input->mouse_released[button] = true;
        event_emit(g_events, GE_INPUT, ECS_NULL_ENTITY, GEK_NONE,
                   input->mouse_world_x, input->mouse_world_y,
                   (float)button + 1.0f, 0.0f, 1, 0);
    }
    input->mouse_buttons[button] = down;
}

void input_inject_aim(InputState *input, float screen_x, float screen_y,
                      float world_x, float world_y) {
    if (!input) return;
    input->mouse_x = screen_x;
    input->mouse_y = screen_y;
    input->mouse_world_x = world_x;
    input->mouse_world_y = world_y;
}
