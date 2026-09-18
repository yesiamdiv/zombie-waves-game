#ifndef SPRITE_H
#define SPRITE_H

#include <SDL3/SDL.h>
#include <stdbool.h>
#include "core/mathutil.h"

/* Resolve a texture through the global asset manager; NULL in headless
 * builds or when the art is missing. */
SDL_Texture *sprite_tex(const char *path);

typedef enum {
    SPRITE_SHAPE_NONE = 0,
    SPRITE_SHAPE_RECT,
    SPRITE_SHAPE_CIRCLE,
    SPRITE_SHAPE_TEXTURE,  /* future use */
    SPRITE_SHAPE_COUNT
} SpriteShape;

typedef struct {
    SpriteShape shape;

    union {
        struct { float w, h; } rect;
        struct { float radius; } circle;
    } as;

    SDL_FColor color;
    float rotation;   /* radians */
    float alpha;      /* 0.0 - 1.0 */

    /* SPRITE_SHAPE_TEXTURE: resolved texture plus optional source rect. A
     * zero-area src means "use the whole texture". color acts as a tint (white
     * = no tint) when combined with texture alpha modulation. */
    SDL_Texture *texture;
    SDL_FRect src;
} Sprite;

typedef struct {
    SDL_Texture *texture;
    SDL_FRect src_rect;
} SpriteSheet;

Sprite sprite_rect(float w, float h, SDL_FColor color);
Sprite sprite_circle(float radius, SDL_FColor color);
Sprite sprite_texture(SDL_Texture *texture);
Sprite sprite_texture_rect(SDL_Texture *texture, SDL_FRect src);
Sprite sprite_none(void);

void sprite_draw(SDL_Renderer *renderer, const Sprite *sprite, float x, float y, float scale, float rotation, float alpha);

/* Draw a blade quad pivoted at (x, y): the handle sits on (x, y) and the
 * blade extends `length` world units along `angle`. Used by the sweeping sword
 * so the pivot is the inner-circle point, not the sprite center. If `tex` is
 * non-NULL the quad is textured (tinted by `color`), otherwise it is filled
 * with `color`. */
void sprite_draw_blade(SDL_Renderer *renderer, SDL_Texture *tex,
                       float x, float y, float angle,
                       float length, float width, float scale,
                       SDL_FColor color, float alpha);

SDL_FColor color_rgb(float r, float g, float b);
SDL_FColor color_rgba(float r, float g, float b, float a);
SDL_FColor color_hex(Uint32 hex);

/* Common colors */
extern const SDL_FColor COLOR_RED;
extern const SDL_FColor COLOR_GREEN;
extern const SDL_FColor COLOR_BLUE;
extern const SDL_FColor COLOR_YELLOW;
extern const SDL_FColor COLOR_WHITE;
extern const SDL_FColor COLOR_BLACK;
extern const SDL_FColor COLOR_GRAY;
extern const SDL_FColor COLOR_DARK_GRAY;
extern const SDL_FColor COLOR_ORANGE;
extern const SDL_FColor COLOR_CYAN;
extern const SDL_FColor COLOR_PURPLE;
extern const SDL_FColor COLOR_BROWN;
extern const SDL_FColor COLOR_DARK_GREEN;
extern const SDL_FColor COLOR_DARK_RED;
extern const SDL_FColor COLOR_LIGHT_BLUE;

#endif
