#include "world/world.h"
#include "world/camera.h"
#include "world/map_registry.h"
#include "assets/asset_manager.h"
#include <stdlib.h>
#include <string.h>

#define TILE_AT(w, x, y) ((w)->tiles[(y) * (w)->width + (x)])

static void world_clear(GameWorld *world) {
    free(world->tiles);
    memset(world, 0, sizeof(GameWorld));
}

static bool parse_map_text(GameWorld *world, const char *text, ThemeID theme) {
    world_clear(world);

    /* Pass 1: measure the grid (rows x cols), ignoring a trailing blank line. */
    int line_len[4096];
    int line_count = 0;
    int max_cols = 0;
    const char *p = text;
    while (*p) {
        int col = 0;
        while (*p && *p != '\n') {
            if (*p != '\r') col++;
            p++;
        }
        if (*p == '\n') p++;
        if (line_count < 4096) line_len[line_count] = col;
        line_count++;
        if (col > max_cols) max_cols = col;
    }
    if (line_count > 0 && line_len[line_count - 1] == 0) {
        line_count--; /* trailing newline produced an empty final line */
    }

    if (line_count < 3 || max_cols < 3) return false;

    world->width = max_cols;
    world->height = line_count;
    world->world_pixel_w = (float)max_cols * WORLD_GRID_SIZE;
    world->world_pixel_h = (float)line_count * WORLD_GRID_SIZE;
    world->theme = theme;

    world->tiles = (TileType *)calloc((size_t)max_cols * (size_t)line_count,
                                      sizeof(TileType));
    if (!world->tiles) return false;

    /* Pass 2: fill the grid. Short rows pad with ground. */
    p = text;
    for (int y = 0; y < line_count; y++) {
        int x = 0;
        while (*p && *p != '\n') {
            char c = *p++;
            if (c == '\r') continue;
            TileType tile = TILE_GROUND;
            switch (c) {
                case '#': tile = TILE_WALL; break;
                case '~': tile = TILE_WATER; break;
                case '+': tile = TILE_ROAD; break;
                case 'S':
                    tile = TILE_GROUND;
                    world->has_spawn = true;
                    world->spawn_x = x;
                    world->spawn_y = y;
                    break;
                default: tile = TILE_GROUND; break;
            }
            if (x < max_cols) TILE_AT(world, x, y) = tile;
            x++;
        }
        if (*p == '\n') p++;
    }

    LOG_INFO("Map parsed: %dx%d tiles, theme %s, spawn (%d, %d)",
             world->width, world->height, theme_name(theme),
             world->spawn_x, world->spawn_y);
    return true;
}

bool world_init_from_string(GameWorld *world, const char *text, ThemeID theme) {
    return parse_map_text(world, text, theme);
}

bool world_load_map(GameWorld *world, const MapDef *def) {
    const char *path = asset_path(def->file);
    FILE *f = fopen(path, "rb");
    if (!f) {
        LOG_WARN("cannot open map file: %s", path);
        return false;
    }

    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return false; }
    long size = ftell(f);
    if (size <= 0 || fseek(f, 0, SEEK_SET) != 0) { fclose(f); return false; }

    char *buf = (char *)malloc((size_t)size + 1);
    if (!buf) { fclose(f); return false; }
    size_t got = fread(buf, 1, (size_t)size, f);
    buf[got] = '\0';
    fclose(f);

    bool ok = parse_map_text(world, buf, def->theme);
    free(buf);
    return ok;
}

void world_init(GameWorld *world) {
    world_clear(world);

    world->width = WORLD_TILES_X;
    world->height = WORLD_TILES_Y;
    world->world_pixel_w = WORLD_TILES_X * WORLD_GRID_SIZE;
    world->world_pixel_h = WORLD_TILES_Y * WORLD_GRID_SIZE;
    world->theme = THEME_GRASSLAND;

    world->tiles = (TileType *)calloc((size_t)WORLD_TILES_X * (size_t)WORLD_TILES_Y,
                                      sizeof(TileType));
    if (!world->tiles) {
        LOG_FATAL("world_init: out of memory");
        return;
    }

    /* Border walls */
    for (int x = 0; x < WORLD_TILES_X; x++) {
        TILE_AT(world, x, 0) = TILE_WALL;
        TILE_AT(world, x, WORLD_TILES_Y - 1) = TILE_WALL;
    }
    for (int y = 0; y < WORLD_TILES_Y; y++) {
        TILE_AT(world, 0, y) = TILE_WALL;
        TILE_AT(world, WORLD_TILES_X - 1, y) = TILE_WALL;
    }

    /* Cross roads */
    int mid_x = WORLD_TILES_X / 2;
    int mid_y = WORLD_TILES_Y / 2;
    for (int x = 2; x < WORLD_TILES_X - 2; x++) {
        TILE_AT(world, x, mid_y) = TILE_ROAD;
        TILE_AT(world, x, mid_y - 1) = TILE_ROAD;
    }
    for (int y = 2; y < WORLD_TILES_Y - 2; y++) {
        TILE_AT(world, mid_x, y) = TILE_ROAD;
        TILE_AT(world, mid_x - 1, y) = TILE_ROAD;
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
                    TILE_AT(world, tx, ty) = TILE_WALL;
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
                    TILE_AT(world, tx, ty) = TILE_WATER;
                }
            }
        }
    }

    LOG_INFO("World generated: %dx%d tiles, %.0fx%.0f pixels",
             world->width, world->height,
             world->world_pixel_w, world->world_pixel_h);
}

void world_free(GameWorld *world) {
    world_clear(world);
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
            TileType tile = TILE_AT(world, x, y);

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
    return TILE_AT(world, x, y);
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
    if (world->has_spawn && world->spawn_x >= 0 && world->spawn_x < world->width &&
        world->spawn_y >= 0 && world->spawn_y < world->height) {
        return vec2((float)world->spawn_x * WORLD_GRID_SIZE + WORLD_GRID_SIZE * 0.5f,
                    (float)world->spawn_y * WORLD_GRID_SIZE + WORLD_GRID_SIZE * 0.5f);
    }

    /* Find a walkable tile near the center */
    int cx = world->width / 2;
    int cy = world->height / 2;

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