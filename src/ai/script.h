#ifndef SCRIPT_H
#define SCRIPT_H

#include <SDL3/SDL.h>
#include <stdbool.h>
#include "core/mathutil.h"
#include "ai/ai_types.h"

#define SCRIPT_MAX_ACTIONS 4096

typedef enum {
    SCRIPT_KEY,
    SCRIPT_CLICK,
    SCRIPT_AIM,
    SCRIPT_SHOP,
    SCRIPT_WEAPON,
    SCRIPT_QUIT,
} ScriptActionType;

typedef struct {
    double time;
    ScriptActionType type;
    int arg1;   /* scancode or mouse button (1-5) */
    int arg2;   /* down = 1, up = 0 */
    float fx, fy; /* aim target (world coords) */
} ScriptAction;

typedef struct {
    ScriptAction actions[SCRIPT_MAX_ACTIONS];
    int count;
    int index;        /* next unprocessed action */
    double elapsed;
    bool done;
    bool quit_requested;

    /* persistent synthetic input state */
    bool keys[SDL_SCANCODE_COUNT];
    bool buttons[5];
    Vec2 aim;
    bool has_aim;
    bool shop_held;
    int  weapon;   /* 1-4 = keep selecting this weapon, 0 = none */
} ScriptPlayer;

/* Parses a script file. Returns 0 on success. */
int script_player_load(ScriptPlayer *sp, const char *path);
void script_player_reset(ScriptPlayer *sp);

/* Advances the playback clock and fills `out` with the current intended
 * controls. `done` is set when all actions have been consumed. */
void script_player_update(ScriptPlayer *sp, double dt, AIControls *out);

#endif