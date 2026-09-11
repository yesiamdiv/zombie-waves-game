#ifndef CONFIG_H
#define CONFIG_H

/* Settable via --player-damage-mult=<float> (debug/playtest). Lets scripted
 * headless runs force the player into danger so the death/game-over and heal
 * economy paths are reachable deterministically. Owned by main.c. */
extern float g_zombie_damage_mult;

/* Settable via --zombie-speed-mult=<float> (debug/playtest). Speeds up the
 * horde so zombies can close to melee before an elite bot shreds them,
 * exercising the real melee->death->game-over pipeline deterministically. */
extern float g_zombie_speed_mult;

#endif