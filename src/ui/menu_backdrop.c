#include "ui/menu_backdrop.h"

#include <math.h>

#include "world/theme.h"

#define BACKDROP_BANDS 48
#define BACKDROP_CELL 96.0f

void menu_backdrop_draw(SDL_Renderer *renderer, ThemeID theme, float t,
                        int screen_w, int screen_h, float red_wash) {
    const Theme *th = theme_get(theme);
    const float h = (float)screen_h;
    const float band_h = h / (float)BACKDROP_BANDS + 1.0f;

    for (int i = 0; i < BACKDROP_BANDS; ++i) {
        const float f = (float)i / (float)(BACKDROP_BANDS - 1);
        SDL_SetRenderDrawColorFloat(renderer,
            th->ground_color.r * 0.26f * (1.0f - f) + 0.02f * f,
            th->ground_color.g * 0.26f * (1.0f - f) + 0.02f * f,
            th->ground_color.b * 0.26f * (1.0f - f) + 0.03f * f,
            1.0f);
        SDL_RenderFillRect(renderer, &(SDL_FRect){0.0f, f * h,
                                                  (float)screen_w, band_h});
    }

    const float drift = fmodf(t * 7.0f, BACKDROP_CELL);
    SDL_FColor grid = th->grid_color;
    SDL_SetRenderDrawColorFloat(renderer, grid.r * 0.9f, grid.g * 0.9f,
                                grid.b * 0.9f, 0.12f);
    for (float x = -BACKDROP_CELL + drift; x < (float)screen_w; x += BACKDROP_CELL) {
        SDL_RenderLine(renderer, x, 0.0f, x, h);
    }
    for (float y = 0.0f; y < h; y += BACKDROP_CELL) {
        SDL_RenderLine(renderer, 0.0f, y, (float)screen_w, y);
    }

    if (red_wash > 0.0f) {
        SDL_SetRenderDrawColorFloat(renderer, 0.32f, 0.02f, 0.02f, red_wash);
        SDL_RenderFillRect(renderer, &(SDL_FRect){0.0f, 0.0f,
                                                  (float)screen_w, h});
    }
}

void menu_backdrop_overlay(SDL_Renderer *renderer, ThemeID theme,
                           int screen_w, int screen_h, float alpha) {
    const Theme *th = theme_get(theme);
    const float h = (float)screen_h;
    const float band_h = h / (float)BACKDROP_BANDS + 1.0f;

    for (int i = 0; i < BACKDROP_BANDS; ++i) {
        const float f = (float)i / (float)(BACKDROP_BANDS - 1);
        SDL_SetRenderDrawColorFloat(renderer,
            th->ground_color.r * 0.20f,
            th->ground_color.g * 0.20f,
            th->ground_color.b * 0.24f,
            alpha * (0.82f + 0.18f * f));
        SDL_RenderFillRect(renderer, &(SDL_FRect){0.0f, f * h,
                                                  (float)screen_w, band_h});
    }
}
