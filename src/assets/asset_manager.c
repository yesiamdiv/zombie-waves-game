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

/* Reverse lookup: which asset name produced this texture?
 *
 * The net snapshot needs to report the sprite the host ACTUALLY drew rather
 * than infer one from the entity kind, so that changing what an entity looks
 * like cannot silently desync the client. Walking the (short) entry list per
 * entity at 20 Hz is far cheaper than being wrong. Returns NULL for a texture
 * this manager did not load, which the caller treats as NET_ART_NONE. */
const char *asset_manager_name_of(SDL_Texture *tex) {
    AssetManager *am = asset_manager_global();
    if (!am || !tex) return NULL;
    for (AssetEntry *e = am->head; e; e = e->next) {
        if (e->texture == tex) return e->name;
    }
    return NULL;
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
    /* Entity art has transparent backgrounds; blend so alpha actually works. */
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);

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