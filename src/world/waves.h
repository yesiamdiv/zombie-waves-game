#ifndef WAVES_H
#define WAVES_H

#include "ecs/ecs.h"
#include "world/world.h"
#include <stdbool.h>

struct Player; /* forward decl; waves.c includes players.h */

typedef struct {
    int wave_number;
    int zombies_alive;
    int zombies_spawned;
    int zombies_to_spawn;
    int zombies_per_wave;
    float spawn_timer;
    float spawn_interval;
    float wave_cooldown;
    float wave_cooldown_timer;
    float wave_elapsed;        /* seconds since the current wave started */
    bool wave_active;
    bool between_waves;
    int total_kills;
    float difficulty_multiplier;
    Vec2 spawn_points[8];
    int spawn_point_count;
} WaveSystem;

void waves_init(WaveSystem *ws, GameWorld *world);
void waves_update(WaveSystem *ws, World *ecs, GameWorld *world,
                  struct Player *players, int player_count, float dt);
Entity waves_spawn_zombie(World *ecs, Vec2 pos);
void waves_start_next_wave(WaveSystem *ws);
void waves_on_zombie_killed(WaveSystem *ws);

#endif
