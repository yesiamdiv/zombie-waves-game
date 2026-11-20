#include "ui/text_popup.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static TextPopup g_popups[MAX_TEXT_POPUPS];
static bool g_initialized = false;

/* --- private RNG -----------------------------------------------------------
 *
 * Deliberately NOT zw_rand(). The simulation draws from the global stream
 * (waves.c spawn angle/ring/variant/size/jitter, items.c type roll, bot.c
 * strafe), so one cosmetic draw from it shifts every later sim draw and two
 * same-seed runs stop matching -- the single-player byte-identity gate fails
 * for a reason that looks impossible. Keeping the state here makes that
 * mistake unavailable rather than merely discouraged.
 *
 * Numerical Recipes LCG constants; quality is irrelevant for a spread angle,
 * isolation is the entire point. */
#define TEXT_POPUP_RNG_SEED 0x5eed1234u

static uint32_t g_rng = TEXT_POPUP_RNG_SEED;

static uint32_t popup_rand(void) {
    g_rng = g_rng * 1664525u + 1013904223u;
    return g_rng >> 8; /* high bits; the low bits of an LCG cycle short */
}

/* Uniform in [0,1). */
static float popup_randf(void) {
    return (float)(popup_rand() & 0xffffffu) / (float)0x1000000u;
}

static void popup_release(TextPopup *p) {
    if (p->tex) {
        SDL_DestroyTexture(p->tex);
        p->tex = NULL;
    }
    p->active = false;
}

void text_popup_init(void) {
    for (int i = 0; i < MAX_TEXT_POPUPS; i++) popup_release(&g_popups[i]);
    memset(g_popups, 0, sizeof(g_popups));
    g_rng = TEXT_POPUP_RNG_SEED;
    g_initialized = true;
}

void text_popup_shutdown(void) {
    for (int i = 0; i < MAX_TEXT_POPUPS; i++) popup_release(&g_popups[i]);
    memset(g_popups, 0, sizeof(g_popups));
    g_initialized = false;
}

int text_popup_active_count(void) {
    int n = 0;
    for (int i = 0; i < MAX_TEXT_POPUPS; i++)
        if (g_popups[i].active) n++;
    return n;
}

const TextPopup *text_popup_at(int index) {
    if (index < 0 || index >= MAX_TEXT_POPUPS) return NULL;
    return g_popups[index].active ? &g_popups[index] : NULL;
}

/* Free slot, else the live popup with the least life left. Dropping the new
 * popup instead would silently hide exactly the feedback the player is
 * currently generating -- a burst of damage numbers would show its first 256
 * hits and nothing after. */
static TextPopup *popup_claim(void) {
    TextPopup *oldest = NULL;
    float oldest_frac = -1.0f;
    for (int i = 0; i < MAX_TEXT_POPUPS; i++) {
        if (!g_popups[i].active) return &g_popups[i];
        float frac = g_popups[i].lifetime > 0.0f
                         ? g_popups[i].age / g_popups[i].lifetime
                         : 1.0f;
        if (frac > oldest_frac) {
            oldest_frac = frac;
            oldest = &g_popups[i];
        }
    }
    return oldest;
}

void text_popup_spawn(float x, float y, const char *text, SDL_Color color,
                      TextPopupStyle style) {
    if (!g_initialized || !text || text[0] == '\0') return;

    TextPopup *p = popup_claim();
    if (!p) return;

    /* A recycled slot may still own a cached texture for the previous string. */
    if (p->tex) {
        SDL_DestroyTexture(p->tex);
        p->tex = NULL;
    }

    p->active = true;
    p->x = x;
    p->y = y;
    snprintf(p->text, sizeof(p->text), "%s", text);
    p->color = color;
    p->lifetime = 1.0f;
    p->age = 0.0f;
    p->scale = 1.0f;
    p->alpha = 1.0f;
    p->style = style;
    p->tex_w = 0.0f;
    p->tex_h = 0.0f;

    switch (style) {
        case TEXT_POPUP_UP:
            p->vx = 0.0f;
            p->vy = -60.0f;
            break;
        case TEXT_POPUP_UP_LEFT:
            p->vx = -30.0f;
            p->vy = -60.0f;
            break;
        case TEXT_POPUP_UP_RIGHT:
            p->vx = 30.0f;
            p->vy = -60.0f;
            break;
        case TEXT_POPUP_RANDOM: {
            /* Upward fan, +-50 degrees off vertical. Private RNG only. */
            float spread = (popup_randf() * 2.0f - 1.0f) * 50.0f;
            float angle = (spread - 90.0f) * (float)M_PI / 180.0f;
            p->vx = cosf(angle) * 55.0f;
            p->vy = sinf(angle) * 70.0f;
            break;
        }
        case TEXT_POPUP_RADIAL: {
            /* Deterministic by slot index: identical on every machine, which
             * is what makes it legal on a net client. */
            int idx = (int)(p - g_popups);
            float angle = (float)(idx % 360) * (float)M_PI / 180.0f;
            p->vx = cosf(angle) * 50.0f;
            p->vy = sinf(angle) * 50.0f;
            break;
        }
    }
}

void text_popup_spawn_pickup(float x, float y, const char *text) {
    SDL_Color color = {100, 255, 100, 255};
    text_popup_spawn(x, y, text, color, TEXT_POPUP_UP);
}

void text_popup_spawn_damage(float x, float y, int damage) {
    char buf[32];
    snprintf(buf, sizeof(buf), "-%d", damage);
    SDL_Color color = {255, 100, 100, 255};
    text_popup_spawn(x, y, buf, color, TEXT_POPUP_RANDOM);
}

void text_popup_spawn_points(float x, float y, int points) {
    char buf[32];
    snprintf(buf, sizeof(buf), "+%d", points);
    SDL_Color color = {255, 215, 0, 255};
    text_popup_spawn(x, y, buf, color, TEXT_POPUP_UP);
}

/* Pop-in then settle: 0.6 -> 1.15 over the first 12% of life, easing back to
 * 0.95. The overshoot is what makes a damage number read as an impact rather
 * than as text that appeared. */
static float popup_scale_for(float t) {
    const float pop = 0.12f;
    if (t < pop) {
        float k = t / pop;
        return 0.6f + 0.55f * k;
    }
    float k = (t - pop) / (1.0f - pop);
    return 1.15f - 0.20f * k;
}

/* Opaque for the first 55%, then linear to zero: the number is readable for
 * most of its life instead of fading from the instant it appears. */
static float popup_alpha_for(float t) {
    const float hold = 0.55f;
    if (t <= hold) return 1.0f;
    return 1.0f - (t - hold) / (1.0f - hold);
}

void text_popup_update(float dt) {
    if (!g_initialized) return;
    for (int i = 0; i < MAX_TEXT_POPUPS; i++) {
        TextPopup *p = &g_popups[i];
        if (!p->active) continue;

        p->age += dt;
        if (p->age >= p->lifetime) {
            popup_release(p);
            continue;
        }

        p->x += p->vx * dt;
        p->y += p->vy * dt;
        /* Drag, so the motion decelerates into the fade rather than sliding
         * off at constant speed. */
        p->vx *= 1.0f - 2.0f * dt;
        p->vy *= 1.0f - 2.0f * dt;

        float t = p->lifetime > 0.0f ? p->age / p->lifetime : 1.0f;
        if (t > 1.0f) t = 1.0f;
        p->scale = popup_scale_for(t);
        p->alpha = popup_alpha_for(t);
    }
}

static bool popup_ensure_texture(TextPopup *p, SDL_Renderer *renderer,
                                 TTF_Font *font) {
    if (p->tex) return true;
    SDL_Color opaque = {p->color.r, p->color.g, p->color.b, 255};
    SDL_Surface *surface =
        TTF_RenderText_Blended(font, p->text, SDL_strlen(p->text), opaque);
    if (!surface) return false;
    p->tex = SDL_CreateTextureFromSurface(renderer, surface);
    if (p->tex) {
        p->tex_w = (float)surface->w;
        p->tex_h = (float)surface->h;
    }
    SDL_DestroySurface(surface);
    return p->tex != NULL;
}

void text_popup_draw(SDL_Renderer *renderer, Camera *cam, TTF_Font *font) {
    if (!g_initialized || !renderer || !cam || !font) return;

    for (int i = 0; i < MAX_TEXT_POPUPS; i++) {
        TextPopup *p = &g_popups[i];
        if (!p->active) continue;
        if (p->alpha <= 0.0f) continue;

        Vec2 world = {p->x, p->y};
        if (!camera_is_visible(cam, world, 64.0f)) continue;
        if (!popup_ensure_texture(p, renderer, font)) continue;

        Vec2 screen = camera_world_to_screen(cam, world);
        float w = p->tex_w * p->scale * cam->zoom;
        float h = p->tex_h * p->scale * cam->zoom;
        /* Anchored centre-bottom on the event position, so the text rises out
         * of the thing it describes. */
        SDL_FRect dst = {screen.x - w * 0.5f, screen.y - h, w, h};

        SDL_SetTextureAlphaModFloat(p->tex, p->alpha);
        SDL_SetTextureBlendMode(p->tex, SDL_BLENDMODE_BLEND);
        SDL_RenderTexture(renderer, p->tex, NULL, &dst);
    }
}
