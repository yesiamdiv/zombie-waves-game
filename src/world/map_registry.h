#ifndef MAP_REGISTRY_H
#define MAP_REGISTRY_H

#include "world/map_def.h"

int map_registry_count(void);
const MapDef *map_registry_get(int index);

#endif