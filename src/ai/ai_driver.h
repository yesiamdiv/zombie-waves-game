#ifndef AI_DRIVER_H
#define AI_DRIVER_H

#include <stdbool.h>
#include "core/input.h"
#include "world/camera.h"
#include "ecs/ecs.h"
#include "world/waves.h"
#include "ai/ai_types.h"
#include "ai/script.h"
#include "ai/bot.h"

typedef struct {
    AIMode mode;
    ScriptPlayer script;
    BotState bot;
    int script_loops;       /* 1 = play once, >1 repeat */
    int loop_count;
    bool done;              /* script finished (or bot in end state) */
    bool quit_requested;    /* script requested application quit */
} AIDriver;

void ai_driver_init(AIDriver *drv);
int ai_driver_load_script(AIDriver *drv, const char *path);
void ai_driver_set_script_loops(AIDriver *drv, int loops);
void ai_driver_set_bot(AIDriver *drv);

/* Advance the driver and produce intended player controls for this frame. */
void ai_driver_update(AIDriver *drv, double dt, const GameView *view, AIControls *out);

/* Write AIControls into the real InputState via injection (all game states). */
void ai_apply_controls(InputState *input, const AIControls *controls, const Camera *cam);

/* Build a read-only snapshot of the world for the driver to reason about,
 * anchored on the specific player entity the driver controls (so bots/AI
 * reason about their own health/position, not "the first player in storage"). */
void ai_build_view(GameView *view, World *ecs, WaveSystem *waves,
                   Entity local_player);

#endif