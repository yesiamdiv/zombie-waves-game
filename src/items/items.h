#ifndef ITEMS_H
#define ITEMS_H

#include "ecs/ecs.h"
#include "world/world.h"
#include "weapons/weapons.h"

/* Upper bound on live pickup entities so item spawns stay bounded even during
 * long survival runs (playtest finding B6). */
#define MAX_ALIVE_ITEMS 8

/* Pickup bob (AN5). Vertical only, cosmetic: it offsets the DISPLAYED
 * position, never CItemTag.base_pos, so the pickup window does not move. */
#define ITEM_BOB_PX 2.5f
#define ITEM_BOB_HZ 1.5f

Entity items_spawn(World *ecs, Vec2 pos, ItemType type);
void items_spawn_random(World *ecs, Vec2 pos);
int items_count_alive(World *ecs);
void items_check_pickup(World *ecs, Entity player, PlayerInventory *inv);
/* Advance the cosmetic pickup bob. Display-only: see CItemTag.base_pos. */
void items_tick_bob(World *ecs, float dt);

#endif
