#ifndef INPUT_H
#define INPUT_H

#include <SDL3/SDL.h>
#include <stdbool.h>

typedef struct {
    bool keys[SDL_SCANCODE_COUNT];
    bool keys_pressed[SDL_SCANCODE_COUNT];
    bool keys_released[SDL_SCANCODE_COUNT];

    float mouse_x;
    float mouse_y;
    float mouse_world_x;
    float mouse_world_y;
    bool mouse_buttons[5];
    bool mouse_pressed[5];
    bool mouse_released[5];

    bool quit_requested;

    /* True when an AI driver is writing to this state (for logging/debug). */
    bool ai_controlled;
} InputState;

void input_init(InputState *input);
void input_process_event(InputState *input, const SDL_Event *event);
void input_update(InputState *input);
bool input_key_pressed(const InputState *input, SDL_Scancode key);
bool input_key_held(const InputState *input, SDL_Scancode key);
bool input_key_released(const InputState *input, SDL_Scancode key);
bool input_mouse_pressed(const InputState *input, int button);

/* --- Input injection API (for AI drivers, scripts, automated tests). ---
 * These write synthetic input into the same InputState the systems read, so
 * human and AI input go through an identical pipeline. Call once per frame
 * per key/button. Edge (pressed/released) flags are tracked internally. */
void input_inject_key(InputState *input, SDL_Scancode key, bool down);
void input_inject_click(InputState *input, int button, bool down);
void input_inject_aim(InputState *input, float screen_x, float screen_y,
                      float world_x, float world_y);

#endif
