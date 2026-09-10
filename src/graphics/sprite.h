#ifndef SPRITE_H
#define SPRITE_H

#include <SDL3/SDL.h>
#include <stdbool.h>
#include "core/mathutil.h"

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
    float rotation;   /* radians, for future rotation support */
    float alpha;      /* 0.0 - 1.0 */
} Sprite;

typedef struct {
    SDL_Texture *texture;
    SDL_FRect src_rect;
} SpriteSheet;

Sprite sprite_rect(float w, float h, SDL_FColor color);
Sprite sprite_circle(float radius, SDL_FColor color);
Sprite sprite_none(void);

void sprite_draw(SDL_Renderer *renderer, const Sprite *sprite, float x, float y, float scale, float rotation, float alpha);

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
