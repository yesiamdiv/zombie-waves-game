#include "world/world.h"
#include "world/camera.h"
#include "assets/asset_manager.h"
#include <stdlib.h>
#include <string.h>

void world_init(GameWorld *world) {
    world->width = WORLD_TILES_X;
    world->height = WORLD_TILES_Y;
    world->world_pixel_w = WORLD_TILES_X * WORLD_GRID_SIZE;
    world->world_pixel_h = WORLD_TILES_Y * WORLD_GRID_SIZE;
    world->theme = THEME_GRASSLAND;

    /* Start with all ground */
    for (int y = 0; y < WORLD_TILES_Y; y++) {
        for (int x = 0; x < WORLD_TILES_X; x++) {
            world->tiles[y][x] = TILE_GROUND;
        }
    }

    /* Border walls */
    for (int x = 0; x < WORLD_TILES_X; x++) {
        world->tiles[0][x] = TILE_WALL;
        world->tiles[WORLD_TILES_Y - 1][x] = TILE_WALL;
    }
    for (int y = 0; y < WORLD_TILES_Y; y++) {
        world->tiles[y][0] = TILE_WALL;
        world->tiles[y][WORLD_TILES_X - 1] = TILE_WALL;
    }

    /* Cross roads */
    int mid_x = WORLD_TILES_X / 2;
    int mid_y = WORLD_TILES_Y / 2;
    for (int x = 2; x < WORLD_TILES_X - 2; x++) {
        world->tiles[mid_y][x] = TILE_ROAD;
        world->tiles[mid_y - 1][x] = TILE_ROAD;
    }
    for (int y = 2; y < WORLD_TILES_Y - 2; y++) {
        world->tiles[y][mid_x] = TILE_ROAD;
        world->tiles[y][mid_x - 1] = TILE_ROAD;
    }

    /* Some buildings / walls scattered */
    int building_positions[][2] = {
        {8, 8}, {8, 20}, {20, 8}, {35, 10}, {10, 35},
        {30, 30}, {40, 20}, {20, 40}, {15, 15}, {35, 35},
    };
    for (int b = 0; b < 10; b++) {
        int bx = building_positions[b][0];
        int by = building_positions[b][1];
        int bw = 2 + (b % 3);
        int bh = 2 + (b % 2);
        for (int dy = 0; dy < bh; dy++) {
            for (int dx = 0; dx < bw; dx++) {
                int tx = bx + dx;
                int ty = by + dy;
                if (tx > 1 && tx < WORLD_TILES_X - 2 && ty > 1 && ty < WORLD_TILES_Y - 2) {
                    world->tiles[ty][tx] = TILE_WALL;
                }
            }
        }
    }

    /* A pond */
    for (int dy = -2; dy <= 2; dy++) {
        for (int dx = -3; dx <= 3; dx++) {
            int tx = 25 + dx;
            int ty = 25 + dy;
            if (tx > 1 && tx < WORLD_TILES_X - 2 && ty > 1 && ty < WORLD_TILES_Y - 2) {
                if (dx * dx + dy * dy <= 9) {
                    world->tiles[ty][tx] = TILE_WATER;
                }
            }
        }
    }

    LOG_INFO("World generated: %dx%d tiles, %.0fx%.0f pixels",
             world->width, world->height,
             world->world_pixel_w, world->world_pixel_h);
}

void world_draw(SDL_Renderer *renderer, GameWorld *world, Camera *cam) {
    const Theme *theme = theme_get(world->theme);
    AssetManager *am = asset_manager_global();
    float gs = WORLD_GRID_SIZE * cam->zoom;

    int start_x = (int)((cam->position.x - cam->viewport_w * 0.5f / cam->zoom) / WORLD_GRID_SIZE) - 1;
    int start_y = (int)((cam->position.y - cam->viewport_h * 0.5f / cam->zoom) / WORLD_GRID_SIZE) - 1;
    int end_x = (int)((cam->position.x + cam->viewport_w * 0.5f / cam->zoom) / WORLD_GRID_SIZE) + 2;
    int end_y = (int)((cam->position.y + cam->viewport_h * 0.5f / cam->zoom) / WORLD_GRID_SIZE) + 2;

    start_x = clampi(start_x, 0, world->width - 1);
    start_y = clampi(start_y, 0, world->height - 1);
    end_x = clampi(end_x, 0, world->width - 1);
    end_y = clampi(end_y, 0, world->height - 1);

    SDL_Texture *ground_tex[3] = {0};
    SDL_Texture *wall_tex = NULL, *water_tex = NULL, *road_tex = NULL;
    if (am) {
        for (int i = 0; i < 3; i++) ground_tex[i] = asset_manager_get(am, theme->ground[i]);
        wall_tex  = asset_manager_get(am, theme->wall);
        water_tex = asset_manager_get(am, theme->water);
        road_tex  = asset_manager_get(am, theme->road);
    }

    for (int y = start_y; y <= end_y; y++) {
        for (int x = start_x; x <= end_x; x++) {
            TileType tile = world->tiles[y][x];

            SDL_Texture *tex = NULL;
            SDL_FColor c = theme->ground_color;
            switch (tile) {
                case TILE_WALL:
                    tex = wall_tex;
                    c = theme->wall_color;
                    break;
                case TILE_WATER:
                    tex = water_tex;
                    c = theme->water_color;
                    break;
                case TILE_ROAD:
                    tex = road_tex;
                    c = theme->road_color;
                    break;
                case TILE_GROUND:
                case TILE_SPAWN:
                default:
                    tex = ground_tex[(x * 7 + y * 13) % 3];
                    c = theme->ground_color;
                    break;
            }

            Vec2 screen = camera_world_to_screen(cam, vec2(x * WORLD_GRID_SIZE, y * WORLD_GRID_SIZE));
            SDL_FRect rect = {screen.x, screen.y, gs + 1, gs + 1};

            if (tex) {
                SDL_RenderTexture(renderer, tex, NULL, &rect);
            } else {
                SDL_SetRenderDrawColorFloat(renderer, c.r, c.g, c.b, c.a);
                SDL_RenderFillRect(renderer, &rect);
            }
        }
    }

    /* Grid lines (subtle), navigational aid shared across themes */
    SDL_SetRenderDrawColorFloat(renderer, theme->grid_color.r, theme->grid_color.g,
                                theme->grid_color.b, 0.35f);
    for (int y = start_y; y <= end_y; y++) {
        for (int x = start_x; x <= end_x; x++) {
            Vec2 screen = camera_world_to_screen(cam, vec2(x * WORLD_GRID_SIZE, y * WORLD_GRID_SIZE));
            SDL_FRect rect = {screen.x, screen.y, gs + 1, gs + 1};
            SDL_RenderRect(renderer, &rect);
        }
    }
}

TileType world_get_tile(GameWorld *world, int x, int y) {
    if (x < 0 || x >= world->width || y < 0 || y >= world->height) {
        return TILE_WALL;
    }
    return world->tiles[y][x];
}

bool world_is_walkable(GameWorld *world, float wx, float wy) {
    int tx = (int)(wx / WORLD_GRID_SIZE);
    int ty = (int)(wy / WORLD_GRID_SIZE);
    TileType tile = world_get_tile(world, tx, ty);
    return tile != TILE_WALL && tile != TILE_WATER;
}

bool world_in_bounds(GameWorld *world, float wx, float wy) {
    return wx >= 0 && wx < world->world_pixel_w &&
           wy >= 0 && wy < world->world_pixel_h;
}

Vec2 world_get_spawn_point(GameWorld *world) {
    /* Find a walkable tile near the center */
    int cx = WORLD_TILES_X / 2;
    int cy = WORLD_TILES_Y / 2;

    for (int r = 0; r < 20; r++) {
        for (int dy = -r; dy <= r; dy++) {
            for (int dx = -r; dx <= r; dx++) {
                int tx = cx + dx;
                int ty = cy + dy;
                if (world_is_walkable(world, tx * WORLD_GRID_SIZE + 32,
                                              ty * WORLD_GRID_SIZE + 32)) {
                    return vec2(tx * WORLD_GRID_SIZE + WORLD_GRID_SIZE * 0.5f,
                                ty * WORLD_GRID_SIZE + WORLD_GRID_SIZE * 0.5f);
                }
            }
        }
    }
    return vec2(world->world_pixel_w * 0.5f, world->world_pixel_h * 0.5f);
}
