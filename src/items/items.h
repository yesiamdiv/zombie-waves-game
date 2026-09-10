#ifndef ITEMS_H
#define ITEMS_H

#include "ecs/ecs.h"
#include "world/world.h"

Entity items_spawn(World *ecs, Vec2 pos, ItemType type);
void items_spawn_random(World *ecs, Vec2 pos);
void items_check_pickup(World *ecs, Entity player);

#endif
