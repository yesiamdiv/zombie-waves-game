#include "ai/script.h"
#include "core/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static SDL_Scancode name_to_scancode(const char *name) {
    if (!name || !*name) return SDL_SCANCODE_UNKNOWN;
    SDL_Scancode sc = SDL_GetScancodeFromName(name);
    if (sc == SDL_SCANCODE_UNKNOWN) {
        LOG_WARN("Script: unknown key name '%s'", name);
    }
    return sc;
}

int script_player_load(ScriptPlayer *sp, const char *path) {
    memset(sp, 0, sizeof(ScriptPlayer));

    FILE *f = fopen(path, "r");
    if (!f) {
        LOG_ERROR("Script: cannot open '%s'", path);
        return -1;
    }

    char line[256];
    int lineno = 0;
    while (fgets(line, sizeof(line), f) && sp->count < SCRIPT_MAX_ACTIONS) {
        lineno++;
        char *s = line;
        while (*s == ' ' || *s == '\t') s++;
        if (*s == '#' || *s == '\n' || *s == '\0') continue;

        if (*s == '@') {
            s++;
            char cmd[32] = {0};
            double t = 0;
            if (sscanf(s, "%lf %31s", &t, cmd) < 2) {
                LOG_WARN("Script %s:%d: malformed line", path, lineno);
                continue;
            }

            ScriptAction action = {0};
            action.time = t;

            if (strcmp(cmd, "key") == 0) {
                char keyname[64] = {0};
                char state[8] = {0};
                if (sscanf(s, "%lf %*s %63s %7s", &t, keyname, state) < 3) {
                    LOG_WARN("Script %s:%d: key needs <name> <down|up>", path, lineno);
                    continue;
                }
                action.type = SCRIPT_KEY;
                action.arg1 = name_to_scancode(keyname);
                action.arg2 = (strcmp(state, "down") == 0) ? 1 :
                              (strcmp(state, "up") == 0) ? 0 : 0;
                if (action.arg2 == 0 && strcmp(state, "up") != 0) {
                    LOG_WARN("Script %s:%d: key state must be 'down' or 'up'",
                             path, lineno);
                    continue;
                }
            } else if (strcmp(cmd, "click") == 0) {
                int button = 0;
                char state[8] = {0};
                if (sscanf(s, "%lf %*s %d %7s", &t, &button, state) < 3) {
                    LOG_WARN("Script %s:%d: click needs <button> <down|up>", path, lineno);
                    continue;
                }
                action.type = SCRIPT_CLICK;
                action.arg1 = button;
                action.arg2 = (strcmp(state, "down") == 0) ? 1 : 0;
            } else if (strcmp(cmd, "aim") == 0) {
                float x = 0, y = 0;
                if (sscanf(s, "%lf %*s %f %f", &t, &x, &y) < 3) {
                    LOG_WARN("Script %s:%d: aim needs <x> <y>", path, lineno);
                    continue;
                }
                action.type = SCRIPT_AIM;
                action.fx = x;
                action.fy = y;
            } else if (strcmp(cmd, "quit") == 0) {
                action.type = SCRIPT_QUIT;
            } else {
                LOG_WARN("Script %s:%d: unknown command '%s'", path, lineno, cmd);
                continue;
            }

            sp->actions[sp->count++] = action;
        } else {
            LOG_WARN("Script %s:%d: line must start with '@'", path, lineno);
        }
    }

    fclose(f);
    LOG_INFO("Script loaded '%s': %d actions (line end %d)", path, sp->count, lineno);
    return sp->count > 0 ? 0 : -1;
}

void script_player_reset(ScriptPlayer *sp) {
    sp->index = 0;
    sp->elapsed = 0;
    sp->done = false;
    sp->quit_requested = false;
    memset(sp->keys, 0, sizeof(sp->keys));
    memset(sp->buttons, 0, sizeof(sp->buttons));
    sp->has_aim = false;
}

static void release_all(ScriptPlayer *sp) {
    memset(sp->keys, 0, sizeof(sp->keys));
    memset(sp->buttons, 0, sizeof(sp->buttons));
}

void script_player_update(ScriptPlayer *sp, double dt, AIControls *out) {
    ai_controls_reset(out);

    if (sp->done) {
        /* preserve released state; nothing more happens */
        return;
    }

    sp->elapsed += dt;

    while (sp->index < sp->count && sp->actions[sp->index].time <= sp->elapsed) {
        ScriptAction *a = &sp->actions[sp->index];
        switch (a->type) {
            case SCRIPT_KEY:
                if (a->arg1 >= 0 && a->arg1 < SDL_SCANCODE_COUNT) {
                    sp->keys[a->arg1] = a->arg2 != 0;
                }
                break;
            case SCRIPT_CLICK:
                if (a->arg1 >= 1 && a->arg1 <= 5) {
                    sp->buttons[a->arg1 - 1] = a->arg2 != 0;
                }
                break;
            case SCRIPT_AIM:
                sp->aim = vec2(a->fx, a->fy);
                sp->has_aim = true;
                break;
            case SCRIPT_QUIT:
                sp->quit_requested = true;
                sp->done = true;
                break;
        }
        sp->index++;
    }

    if (sp->index >= sp->count) {
        sp->done = true;
        LOG_TRACE("Script playback finished (%.2fs)", sp->elapsed);
        release_all(sp);
    }

    out->move_up     = sp->keys[SDL_SCANCODE_W];
    out->move_down   = sp->keys[SDL_SCANCODE_S];
    out->move_left   = sp->keys[SDL_SCANCODE_A];
    out->move_right  = sp->keys[SDL_SCANCODE_D];
    out->action      = sp->keys[SDL_SCANCODE_SPACE];
    out->shoot       = sp->buttons[0];
    out->has_aim     = sp->has_aim;
    out->aim_x       = sp->aim.x;
    out->aim_y       = sp->aim.y;
}