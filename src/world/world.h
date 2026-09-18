#ifndef WORLD_H
#define WORLD_H

#include "core/mathutil.h"
#include "core/log.h"
#include "camera.h"
#include "world/theme.h"
#include <SDL3/SDL.h>

#define WORLD_GRID_SIZE 64
#define WORLD_TILES_X 50
#define WORLD_TILES_Y 50

typedef enum {
    TILE_GROUND = 0,
    TILE_WALL,
    TILE_WATER,
    TILE_ROAD,
    TILE_SPAWN,
    TILE_COUNT
} TileType;

typedef struct {
    TileType tiles[WORLD_TILES_Y][WORLD_TILES_X];
    int width;
    int height;
    float world_pixel_w;
    float world_pixel_h;
    ThemeID theme;
} GameWorld;

void world_init(GameWorld *world);
void world_draw(SDL_Renderer *renderer, GameWorld *world, Camera *cam);
TileType world_get_tile(GameWorld *world, int x, int y);
bool world_is_walkable(GameWorld *world, float wx, float wy);
bool world_in_bounds(GameWorld *world, float wx, float wy);
Vec2 world_get_spawn_point(GameWorld *world);

#endif
