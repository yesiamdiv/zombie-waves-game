#include "assets/asset_manager.h"
#include "core/log.h"

#include <SDL3_image/SDL_image.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool file_exists(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    fclose(f);
    return true;
}

const char *asset_path(const char *rel) {
    static char buf[1024];
    const char *base = SDL_GetBasePath();
    if (base && base[0]) {
        snprintf(buf, sizeof(buf), "%sassets/%s", base, rel);
        if (file_exists(buf)) return buf;
    }
    snprintf(buf, sizeof(buf), "assets/%s", rel);
    return buf;
}

static AssetManager *g_assets = NULL;

void asset_manager_set_global(AssetManager *am) {
    g_assets = am;
}

AssetManager *asset_manager_global(void) {
    return g_assets;
}

void asset_manager_init(AssetManager *am, SDL_Renderer *renderer) {
    am->renderer = renderer;
    am->head = NULL;
    g_assets = am;
}

void asset_manager_shutdown(AssetManager *am) {
    AssetEntry *e = am->head;
    while (e) {
        AssetEntry *next = e->next;
        if (e->texture) SDL_DestroyTexture(e->texture);
        free(e);
        e = next;
    }
    am->head = NULL;
    am->renderer = NULL;
    g_assets = NULL;
}

SDL_Texture *asset_manager_get(AssetManager *am, const char *name) {
    if (!am || !am->renderer || !name) return NULL;

    for (AssetEntry *e = am->head; e; e = e->next) {
        if (strcmp(e->name, name) == 0) return e->texture;
    }

    const char *path = asset_path(name);
    if (!file_exists(path)) {
        LOG_WARN("asset not found: %s", path);
        return NULL;
    }

    SDL_Texture *tex = IMG_LoadTexture(am->renderer, path);
    if (!tex) {
        LOG_WARN("IMG_LoadTexture('%.500s') failed: %s", path, SDL_GetError());
        return NULL;
    }

    /* Pixel art must stay chunky when scaled; nearest keeps it crisp. */
    SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);

    AssetEntry *e = (AssetEntry *)malloc(sizeof(AssetEntry));
    if (!e) {
        SDL_DestroyTexture(tex);
        return NULL;
    }
    snprintf(e->name, sizeof(e->name), "%s", name);
    e->texture = tex;
    e->next = am->head;
    am->head = e;

    LOG_DEBUG("asset loaded: %s -> %s", name, path);
    return tex;
}