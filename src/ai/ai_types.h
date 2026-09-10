#ifndef AI_TYPES_H
#define AI_TYPES_H

#include <SDL3/SDL.h>
#include <stdbool.h>
#include "core/mathutil.h"

typedef enum {
    AI_MODE_NONE = 0,
    AI_MODE_SCRIPT,
    AI_MODE_BOT,
} AIMode;

/* Actions an AI wants the player to take this frame.
 * The driver translates these into real input-injection calls. */
typedef struct {
    bool move_up;
    bool move_down;
    bool move_left;
    bool move_right;
    bool shoot;
    bool action;      /* generic confirm key (menus) */
    bool has_aim;     /* whether aim_x/aim_y are valid */
    float aim_x, aim_y; /* world-space aim target */
} AIControls;

/* Read-only snapshot of the world for an AI to reason about.
 * Built by the host each frame before drivers run. */
typedef struct {
    Vec2 player_pos;
    float player_health;
    float player_max_health;
    bool player_alive;

    Vec2 nearest_zombie;
    float nearest_zombie_dist;
    int zombies_alive;

    Vec2 nearest_item;
    float nearest_item_dist;

    int wave_number;
} GameView;

static inline void ai_controls_reset(AIControls *c) {
    *c = (AIControls){0};
}

static inline void ai_view_reset(GameView *v) {
    v->player_alive = false;
    v->nearest_zombie_dist = 1e9f;
    v->nearest_item_dist = 1e9f;
    v->zombies_alive = 0;
}

#endif