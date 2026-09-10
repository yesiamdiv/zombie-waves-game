#include "core/input.h"
#include "core/log.h"
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
                }
                input->keys[event->key.scancode] = true;
            }
            break;

        case SDL_EVENT_KEY_UP:
            if (event->key.scancode < SDL_SCANCODE_COUNT) {
                input->keys[event->key.scancode] = false;
                input->keys_released[event->key.scancode] = true;
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
