#ifndef ASSET_MANAGER_H
#define ASSET_MANAGER_H

#include <SDL3/SDL.h>
#include <stdbool.h>

#define ASSET_MAX_NAME 128

typedef struct AssetEntry {
    char name[ASSET_MAX_NAME];
    SDL_Texture *texture;
    struct AssetEntry *next;
} AssetEntry;

typedef struct {
    SDL_Renderer *renderer;
    AssetEntry *head;
} AssetManager;

void asset_manager_init(AssetManager *am, SDL_Renderer *renderer);
void asset_manager_shutdown(AssetManager *am);

/* Return the cached (and lazily loaded) texture for a relative name like
 * "textures/ground.png". Returns NULL in headless builds or on load failure. */
SDL_Texture *asset_manager_get(AssetManager *am, const char *name);

/* Resolve a repo-relative asset path to a full filesystem path. Tries the
 * executable's base path first, then the process working directory. */
const char *asset_path(const char *rel);

#endif