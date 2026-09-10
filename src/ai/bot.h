#ifndef BOT_H
#define BOT_H

#include <stdbool.h>
#include "ai/ai_types.h"

typedef struct {
    float detection_range;
    float standoff_range;   /* preferred distance to keep from nearest zombie */
    float flee_range;       /* below this, back away */
    float fire_cooldown;
    float fire_rate;
    float strafe_dir;       /* +1 / -1 alternating strafe */
    float strafe_timer;
} BotState;

void bot_init(BotState *bot);
void bot_update(BotState *bot, const GameView *view, double dt, AIControls *out);

#endif