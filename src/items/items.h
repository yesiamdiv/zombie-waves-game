#ifndef ITEMS_H
#define ITEMS_H

#include "ecs/ecs.h"
#include "world/world.h"

/* Upper bound on live pickup entities so item spawns stay bounded even during
 * long survival runs (playtest finding B6). */
#define MAX_ALIVE_ITEMS 8

Entity items_spawn(World *ecs, Vec2 pos, ItemType type);
void items_spawn_random(World *ecs, Vec2 pos);
int items_count_alive(World *ecs);
void items_check_pickup(World *ecs, Entity player);

#endif
