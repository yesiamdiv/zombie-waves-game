#include "world/map_registry.h"

static const MapDef maps[] = {
    {"Grassland", "maps/grassland.map", THEME_GRASSLAND},
    {"Desert",    "maps/desert.map",    THEME_DESERT},
    {"Snow",      "maps/snow.map",      THEME_SNOW},
    {"City",      "maps/city.map",      THEME_CITY},
};

int map_registry_count(void) {
    return (int)(sizeof(maps) / sizeof(maps[0]));
}

const MapDef *map_registry_get(int index) {
    if (index < 0 || index >= map_registry_count()) {
        return &maps[0];
    }
    return &maps[index];
}