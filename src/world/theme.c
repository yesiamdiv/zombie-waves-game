#include "world/theme.h"

static const Theme themes[THEME_COUNT] = {
    [THEME_GRASSLAND] = {
        .name = "Grassland",
        .ground = {"textures/tiles/grass_ground0.png",
                   "textures/tiles/grass_ground1.png",
                   "textures/tiles/grass_ground2.png"},
        .wall  = "textures/tiles/grass_wall.png",
        .water = "textures/tiles/grass_water.png",
        .road  = "textures/tiles/grass_road.png",
        .ground_color = {0.290f, 0.463f, 0.212f, 1.0f},
        .wall_color   = {0.494f, 0.478f, 0.463f, 1.0f},
        .water_color  = {0.227f, 0.424f, 0.659f, 1.0f},
        .road_color   = {0.251f, 0.247f, 0.231f, 1.0f},
        .grid_color   = {0.149f, 0.200f, 0.169f, 1.0f},
        .player_color = {0.3f, 0.4f, 1.0f, 1.0f},   /* blue soldier */
        .zombie_tints = {{0.3f, 0.6f, 0.2f, 1.0f},  /* green */
                         {0.5f, 0.4f, 0.2f, 1.0f},  /* brown */
                         {0.4f, 0.2f, 0.3f, 1.0f}}, /* purple */
    },
    [THEME_DESERT] = {
        .name = "Desert",
        .ground = {"textures/tiles/desert_ground0.png",
                   "textures/tiles/desert_ground1.png",
                   "textures/tiles/desert_ground2.png"},
        .wall  = "textures/tiles/desert_wall.png",
        .water = "textures/tiles/desert_water.png",
        .road  = "textures/tiles/desert_road.png",
        .ground_color = {0.769f, 0.675f, 0.463f, 1.0f},
        .wall_color   = {0.675f, 0.518f, 0.345f, 1.0f},
        .water_color  = {0.251f, 0.463f, 0.502f, 1.0f},
        .road_color   = {0.675f, 0.580f, 0.408f, 1.0f},
        .grid_color   = {0.561f, 0.478f, 0.322f, 1.0f},
        .player_color = {0.85f, 0.6f, 0.35f, 1.0f}, /* tan desert trooper */
        .zombie_tints = {{0.72f, 0.55f, 0.3f, 1.0f},  /* sand */
                         {0.62f, 0.42f, 0.24f, 1.0f}, /* rust brown */
                         {0.5f, 0.32f, 0.35f, 1.0f}}, /* mauve */
    },
    [THEME_SNOW] = {
        .name = "Snow",
        .ground = {"textures/tiles/snow_ground0.png",
                   "textures/tiles/snow_ground1.png",
                   "textures/tiles/snow_ground2.png"},
        .wall  = "textures/tiles/snow_wall.png",
        .water = "textures/tiles/snow_water.png",
        .road  = "textures/tiles/snow_road.png",
        .ground_color = {0.910f, 0.933f, 0.965f, 1.0f},
        .wall_color   = {0.659f, 0.722f, 0.792f, 1.0f},
        .water_color  = {0.376f, 0.502f, 0.667f, 1.0f},
        .road_color   = {0.588f, 0.620f, 0.659f, 1.0f},
        .grid_color   = {0.745f, 0.784f, 0.847f, 1.0f},
        .player_color = {0.6f, 0.75f, 1.0f, 1.0f},    /* ice-blue arctic */
        .zombie_tints = {{0.6f, 0.68f, 0.72f, 1.0f},  /* pale grey */
                         {0.5f, 0.6f, 0.55f, 1.0f},   /* frost green */
                         {0.68f, 0.7f, 0.8f, 1.0f}},  /* blue-white */
    },
    [THEME_CITY] = {
        .name = "City",
        .ground = {"textures/tiles/city_ground0.png",
                   "textures/tiles/city_ground1.png",
                   "textures/tiles/city_ground2.png"},
        .wall  = "textures/tiles/city_wall.png",
        .water = "textures/tiles/city_water.png",
        .road  = "textures/tiles/city_road.png",
        .ground_color = {0.408f, 0.400f, 0.392f, 1.0f},
        .wall_color   = {0.557f, 0.306f, 0.227f, 1.0f},
        .water_color  = {0.235f, 0.259f, 0.290f, 1.0f},
        .road_color   = {0.204f, 0.196f, 0.192f, 1.0f},
        .grid_color   = {0.204f, 0.196f, 0.184f, 1.0f},
        .player_color = {0.3f, 0.45f, 0.7f, 1.0f},   /* urban blue */
        .zombie_tints = {{0.45f, 0.5f, 0.4f, 1.0f},  /* concrete */
                         {0.55f, 0.4f, 0.3f, 1.0f},  /* brick */
                         {0.4f, 0.35f, 0.5f, 1.0f}}, /* asphalt */
    },
};

const Theme *theme_get(ThemeID id) {
    if (id < 0 || id >= THEME_COUNT) id = THEME_GRASSLAND;
    return &themes[id];
}

int theme_count(void) {
    return THEME_COUNT;
}

const char *theme_name(ThemeID id) {
    return theme_get(id)->name;
}