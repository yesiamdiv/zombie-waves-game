#ifndef WORLD_H
#define WORLD_H

#include "core/mathutil.h"
#include "core/log.h"
#include "camera.h"
#include "world/theme.h"
#include "world/map_def.h"
#include <SDL3/SDL.h>

#define WORLD_GRID_SIZE 64

/* Default classic map size (used by world_init and the grassland preset). */
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

/* `tiles` is a heap buffer owned by the world. A GameWorld must therefore be
 * zero-initialized before it is passed to world_init()/world_init_from_string()
 * or world_free() - those call world_clear(), which frees `tiles` and then
 * memsets the struct. Declare it as `GameWorld world = {0};` (or as a
 * static/global, which is zero-initialized) rather than leaving it on the
 * stack uninitialized. */
typedef struct {
    TileType *tiles;   /* width * height, row-major */
    int width;
    int height;
    float world_pixel_w;
    float world_pixel_h;
    ThemeID theme;

    /* Optional explicit spawn tile from a map's 'S' marker. */
    int spawn_x;
    int spawn_y;
    bool has_spawn;
} GameWorld;

/* Classic 50x50 grassland layout (headless/tests and hand-built default). */
void world_init(GameWorld *world);

/* Parse a map file (via asset_path) into the world. Returns false on failure,
 * leaving the world untouched. */
bool world_load_map(GameWorld *world, const MapDef *def);

/* Parse a rectangular ASCII map from an in-memory string. Exported for unit
 * tests and the file loader. Returns false on malformed input. */
bool world_init_from_string(GameWorld *world, const char *text, ThemeID theme);

void world_free(GameWorld *world);

void world_draw(SDL_Renderer *renderer, GameWorld *world, Camera *cam);
TileType world_get_tile(GameWorld *world, int x, int y);
bool world_is_walkable(GameWorld *world, float wx, float wy);
bool world_in_bounds(GameWorld *world, float wx, float wy);
Vec2 world_get_spawn_point(GameWorld *world);

#endif