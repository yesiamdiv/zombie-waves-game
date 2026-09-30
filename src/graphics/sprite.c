#include "graphics/sprite.h"
#include "core/log.h"
#include "assets/asset_manager.h"

SDL_Texture *sprite_tex(const char *path) {
    AssetManager *am = asset_manager_global();
    return am ? asset_manager_get(am, path) : NULL;
}

const SDL_FColor COLOR_RED        = {1.0f, 0.2f, 0.2f, 1.0f};
const SDL_FColor COLOR_GREEN      = {0.2f, 0.9f, 0.2f, 1.0f};
const SDL_FColor COLOR_BLUE       = {0.3f, 0.4f, 1.0f, 1.0f};
const SDL_FColor COLOR_YELLOW     = {1.0f, 1.0f, 0.2f, 1.0f};
const SDL_FColor COLOR_WHITE      = {1.0f, 1.0f, 1.0f, 1.0f};
const SDL_FColor COLOR_BLACK      = {0.0f, 0.0f, 0.0f, 1.0f};
const SDL_FColor COLOR_GRAY       = {0.5f, 0.5f, 0.5f, 1.0f};
const SDL_FColor COLOR_DARK_GRAY  = {0.25f, 0.25f, 0.25f, 1.0f};
const SDL_FColor COLOR_ORANGE     = {1.0f, 0.6f, 0.1f, 1.0f};
const SDL_FColor COLOR_CYAN       = {0.2f, 0.9f, 0.9f, 1.0f};
const SDL_FColor COLOR_PURPLE     = {0.7f, 0.2f, 0.9f, 1.0f};
const SDL_FColor COLOR_BROWN      = {0.55f, 0.35f, 0.15f, 1.0f};
const SDL_FColor COLOR_DARK_GREEN = {0.1f, 0.4f, 0.1f, 1.0f};
const SDL_FColor COLOR_DARK_RED   = {0.6f, 0.1f, 0.1f, 1.0f};
const SDL_FColor COLOR_LIGHT_BLUE = {0.5f, 0.7f, 1.0f, 1.0f};

SDL_FColor color_rgb(float r, float g, float b) {
    return (SDL_FColor){r, g, b, 1.0f};
}

SDL_FColor color_rgba(float r, float g, float b, float a) {
    return (SDL_FColor){r, g, b, a};
}

SDL_FColor color_hex(Uint32 hex) {
    return (SDL_FColor){
        ((hex >> 16) & 0xFF) / 255.0f,
        ((hex >> 8)  & 0xFF) / 255.0f,
        (hex         & 0xFF) / 255.0f,
        1.0f
    };
}

Sprite sprite_none(void) {
    return (Sprite){0};
}

Sprite sprite_rect(float w, float h, SDL_FColor color) {
    Sprite s = {0};
    s.shape = SPRITE_SHAPE_RECT;
    s.as.rect.w = w;
    s.as.rect.h = h;
    s.color = color;
    s.alpha = 1.0f;
    s.rotation = 0.0f;
    return s;
}

Sprite sprite_circle(float radius, SDL_FColor color) {
    Sprite s = {0};
    s.shape = SPRITE_SHAPE_CIRCLE;
    s.as.circle.radius = radius;
    s.color = color;
    s.alpha = 1.0f;
    s.rotation = 0.0f;
    return s;
}

Sprite sprite_texture(SDL_Texture *texture) {
    Sprite s = {0};
    s.shape = SPRITE_SHAPE_TEXTURE;
    s.texture = texture;
    s.color = COLOR_WHITE;
    s.alpha = 1.0f;
    s.rotation = 0.0f;
    s.src = (SDL_FRect){0, 0, 0, 0};
    return s;
}

Sprite sprite_texture_rect(SDL_Texture *texture, SDL_FRect src) {
    Sprite s = sprite_texture(texture);
    s.src = src;
    return s;
}

void sprite_draw_blade(SDL_Renderer *renderer, SDL_Texture *tex,
                       float x, float y, float angle,
                       float length, float width, float scale,
                       SDL_FColor color, float alpha) {
    if (length <= 0.0f || width <= 0.0f) return;

    float len = length * scale;
    float half_w = width * scale * 0.5f;
    Vec2 dir = vec2_from_angle(angle, 1.0f);
    Vec2 perp = vec2(-dir.y, dir.x);

    Vec2 handle = vec2(x, y);
    Vec2 tip = vec2_add(handle, vec2_scale(dir, len));
    Vec2 off = vec2_scale(perp, half_w);

    SDL_FColor vc = color;
    vc.a = alpha;
    if (tex) {
        SDL_SetTextureColorMod(tex, (Uint8)(color.r * 255.0f),
                               (Uint8)(color.g * 255.0f),
                               (Uint8)(color.b * 255.0f));
        vc = (SDL_FColor){1.0f, 1.0f, 1.0f, alpha};
    }
    SDL_Vertex verts[4] = {
        {.position = {(handle.x - off.x), (handle.y - off.y)}, .color = vc, .tex_coord = {0, 0}},
        {.position = {(handle.x + off.x), (handle.y + off.y)}, .color = vc, .tex_coord = {1, 0}},
        {.position = {(tip.x + off.x), (tip.y + off.y)},       .color = vc, .tex_coord = {1, 1}},
        {.position = {(tip.x - off.x), (tip.y - off.y)},       .color = vc, .tex_coord = {0, 1}},
    };
    const int indices[6] = {0, 1, 2, 0, 2, 3};
    SDL_RenderGeometry(renderer, tex, verts, 4, indices, 6);
    if (tex) {
        SDL_SetTextureColorMod(tex, 255, 255, 255);
    }
}

void sprite_draw(SDL_Renderer *renderer, const Sprite *sprite, float x, float y, float scale, float rotation, float alpha) {
    if (!sprite || sprite->shape == SPRITE_SHAPE_NONE) return;

    float draw_alpha = alpha * sprite->alpha;
    SDL_SetRenderDrawColorFloat(renderer, sprite->color.r, sprite->color.g,
                                 sprite->color.b, draw_alpha);

    switch (sprite->shape) {
        case SPRITE_SHAPE_RECT: {
            float w = sprite->as.rect.w * scale;
            float h = sprite->as.rect.h * scale;
            SDL_FRect rect = {x - w * 0.5f, y - h * 0.5f, w, h};
            SDL_RenderFillRect(renderer, &rect);

            /* darker border */
            SDL_SetRenderDrawColorFloat(renderer,
                sprite->color.r * 0.6f,
                sprite->color.g * 0.6f,
                sprite->color.b * 0.6f,
                draw_alpha);
            SDL_RenderRect(renderer, &rect);
            break;
        }

        case SPRITE_SHAPE_CIRCLE: {
            float radius = sprite->as.circle.radius * scale;
            int segments = 24;
            float rot = rotation + sprite->rotation;

            float points_x[65];
            float points_y[65];
            int count = 0;

            for (int i = 0; i <= segments; i++) {
                float angle = (float)i / (float)segments * 2.0f * M_PI + rot;
                points_x[count] = x + cosf(angle) * radius;
                points_y[count] = y + sinf(angle) * radius;
                count++;
            }

            /* filled circle via scanlines is expensive with SDL, use lines */
            for (int i = 0; i < segments; i++) {
                SDL_RenderLine(renderer, points_x[i], points_y[i],
                               points_x[i + 1], points_y[i + 1]);
            }

            /* fill with smaller lines (approximate) */
            for (int r = (int)radius; r > 0; r -= 2) {
                for (int i = 0; i < segments; i++) {
                    float angle1 = (float)i / (float)segments * 2.0f * M_PI + rot;
                    float angle2 = (float)(i + 1) / (float)segments * 2.0f * M_PI + rot;
                    SDL_RenderLine(renderer,
                        x + cosf(angle1) * r, y + sinf(angle1) * r,
                        x + cosf(angle2) * r, y + sinf(angle2) * r);
                }
            }

            /* outline */
            SDL_SetRenderDrawColorFloat(renderer,
                sprite->color.r * 0.5f,
                sprite->color.g * 0.5f,
                sprite->color.b * 0.5f,
                draw_alpha);
            for (int i = 0; i < segments; i++) {
                SDL_RenderLine(renderer, points_x[i], points_y[i],
                               points_x[i + 1], points_y[i + 1]);
            }
            break;
        }

        case SPRITE_SHAPE_TEXTURE: {
            if (!sprite->texture) break;

            float tw = 0.0f, th = 0.0f;
            SDL_GetTextureSize(sprite->texture, &tw, &th);
            SDL_FRect src = sprite->src;
            if (src.w <= 0.0f) {
                src = (SDL_FRect){0.0f, 0.0f, tw, th};
            }

            float w = src.w * scale;
            float h = src.h * scale;
            float rot_deg = (rotation + sprite->rotation) * 180.0f / (float)M_PI;
            SDL_FRect dst = {x - w * 0.5f, y - h * 0.5f, w, h};
            SDL_FPoint center = {w * 0.5f, h * 0.5f};

            SDL_SetTextureColorMod(sprite->texture,
                (Uint8)(sprite->color.r * 255.0f),
                (Uint8)(sprite->color.g * 255.0f),
                (Uint8)(sprite->color.b * 255.0f));
            SDL_SetTextureAlphaMod(sprite->texture,
                (Uint8)(draw_alpha * 255.0f));

            SDL_RenderTextureRotated(renderer, sprite->texture, &src, &dst,
                                     rot_deg, &center, SDL_FLIP_NONE);

            SDL_SetTextureAlphaMod(sprite->texture, 255);
            SDL_SetTextureColorMod(sprite->texture, 255, 255, 255);
            break;
        }

        default:
            break;
    }
}
