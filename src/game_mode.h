#ifndef GAME_MODE_H
#define GAME_MODE_H

#include <stdbool.h>

/* Top-level play mode. Multiplayer modes are chosen when hosting a game.
 *
 *   GAME_MODE_SINGLE        - classic solo survival (game over on death)
 *   GAME_MODE_MULTI_TDM     - co-op; each player has a spawn beacon and
 *                             respawns there after MULTI_RESPAWN_TIME seconds
 *   GAME_MODE_MULTI_HARDCORE - co-op; a dead player is permanently eliminated
 *
 * All modes share the same world / waves / shop simulation; only the
 * death-and-respawn rules differ between the multiplayer modes.
 */
typedef enum {
    GAME_MODE_SINGLE = 0,
    GAME_MODE_MULTI_TDM,
    GAME_MODE_MULTI_HARDCORE,
    GAME_MODE_COUNT
} GameMode;

/* Fixed respawn delay (seconds) in TDM mode before a dead player comes back at
 * their beacon. */
#define MULTI_RESPAWN_TIME 5.0f

static inline bool game_mode_is_multi(GameMode mode) {
    return mode == GAME_MODE_MULTI_TDM || mode == GAME_MODE_MULTI_HARDCORE;
}

static inline const char *game_mode_name(GameMode mode) {
    switch (mode) {
        case GAME_MODE_SINGLE:         return "single";
        case GAME_MODE_MULTI_TDM:      return "multi-tdm";
        case GAME_MODE_MULTI_HARDCORE: return "multi-hardcore";
        default:                       return "unknown";
    }
}

#endif