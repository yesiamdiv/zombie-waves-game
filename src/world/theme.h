#ifndef THEME_H
#define THEME_H

#include <SDL3/SDL.h>

typedef enum {
    THEME_GRASSLAND = 0,
    THEME_DESERT,
    THEME_SNOW,
    THEME_CITY,
    THEME_COUNT
} ThemeID;

/* An environment's look: which textures/colors each tile type uses. Texture
 * paths are asset-manager keys relative to assets/; artists replace the PNGs
 * 1:1. Colors are the render fallback when a texture is missing or headless. */
typedef struct {
    const char *name;
    const char *ground[3];  /* variants to reduce tiling repetition */
    const char *wall;
    const char *water;
    const char *road;
    SDL_FColor ground_color;
    SDL_FColor wall_color;
    SDL_FColor water_color;
    SDL_FColor road_color;
    SDL_FColor grid_color;
} Theme;

const Theme *theme_get(ThemeID id);
int theme_count(void);
const char *theme_name(ThemeID id);

#endif