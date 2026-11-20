#include "effects/wave_banner.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* Timeline, in seconds from the announcement.
 *
 *   0.00 - 0.30  POP:   scales past 1 and settles, so the banner arrives
 *   0.30 - 1.60  HOLD:  long enough to actually read the number
 *   1.60 - 2.35  EXIT:  shrinks and flies into the HUD wave readout
 *
 * The hold dominates on purpose. The exit is the part people notice, but a
 * transition that starts leaving before it has been read is just a flicker. */
#define BANNER_POP_END   0.30f
#define BANNER_HOLD_END  1.60f
#define BANNER_LIFETIME  2.35f

/* Where the banner flies to: the HUD's "Wave: n" row, which hud.c draws at
 * (20, 50) in screen space. Stated once here with that provenance, because the
 * effect is only legible if it lands ON the readout -- if the HUD moves that
 * row, this number moves with it. */
#define BANNER_TARGET_X 20.0f
#define BANNER_TARGET_Y 50.0f

#define BANNER_SHARDS 14

typedef struct {
    bool active;
    float age;
    char text[64];
    char sub[64];
    SDL_Texture *tex;
    float tex_w, tex_h;
    SDL_Texture *sub_tex;
    float sub_w, sub_h;
} WaveBanner;

static WaveBanner g_banner;

static void banner_drop_textures(void) {
    if (g_banner.tex) {
        SDL_DestroyTexture(g_banner.tex);
        g_banner.tex = NULL;
    }
    if (g_banner.sub_tex) {
        SDL_DestroyTexture(g_banner.sub_tex);
        g_banner.sub_tex = NULL;
    }
}

void wave_banner_reset(void) {
    banner_drop_textures();
    memset(&g_banner, 0, sizeof(g_banner));
}

void wave_banner_show(int wave_number, int zombie_count) {
    /* Replace, never queue: if wave n+1 arrives while n's banner is still on
     * screen, the player needs the current number, not a backlog. */
    banner_drop_textures();
    memset(&g_banner, 0, sizeof(g_banner));

    g_banner.active = true;
    g_banner.age = 0.0f;
    snprintf(g_banner.text, sizeof(g_banner.text), "WAVE %d", wave_number);
    if (zombie_count > 0) {
        snprintf(g_banner.sub, sizeof(g_banner.sub), "%d INCOMING",
                 zombie_count);
    }
}

bool wave_banner_active(void) {
    return g_banner.active;
}

const char *wave_banner_text(void) {
    return g_banner.active ? g_banner.text : NULL;
}

float wave_banner_exit_progress(void) {
    if (!g_banner.active) return 0.0f;
    if (g_banner.age <= BANNER_HOLD_END) return 0.0f;
    float t = (g_banner.age - BANNER_HOLD_END) /
              (BANNER_LIFETIME - BANNER_HOLD_END);
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return t;
}

void wave_banner_update(float dt) {
    if (!g_banner.active) return;
    g_banner.age += dt;
    if (g_banner.age >= BANNER_LIFETIME) {
        wave_banner_reset();
    }
}

/* Overshoot on the way in: 0.55 -> 1.08 -> 1.0. */
static float banner_pop_scale(float age) {
    if (age >= BANNER_POP_END) return 1.0f;
    float k = age / BANNER_POP_END;
    if (k < 0.7f) {
        float a = k / 0.7f;
        return 0.55f + 0.53f * a;
    }
    float a = (k - 0.7f) / 0.3f;
    return 1.08f - 0.08f * a;
}

static bool ensure_text_texture(SDL_Renderer *renderer, TTF_Font *font,
                                const char *text, SDL_Texture **tex,
                                float *w, float *h) {
    if (*tex) return true;
    if (!text || text[0] == '\0') return false;
    SDL_Color white = {255, 255, 255, 255};
    SDL_Surface *s = TTF_RenderText_Blended(font, text, SDL_strlen(text), white);
    if (!s) return false;
    *tex = SDL_CreateTextureFromSurface(renderer, s);
    if (*tex) {
        *w = (float)s->w;
        *h = (float)s->h;
    }
    SDL_DestroySurface(s);
    return *tex != NULL;
}

/* The twinkle: shards thrown off the banner as it leaves, each on a direction
 * fixed by its index. Deterministic by construction -- no zw_rand() call, so this
 * cannot perturb the simulation stream and every machine throws the same
 * shards. */
static void draw_twinkle(SDL_Renderer *renderer, float cx, float cy,
                         float spread, float exit_t) {
    if (exit_t <= 0.0f) return;

    SDL_BlendMode prev = SDL_BLENDMODE_BLEND;
    SDL_GetRenderDrawBlendMode(renderer, &prev);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_ADD);

    for (int i = 0; i < BANNER_SHARDS; i++) {
        /* Golden-angle stepping, so the shards are spread evenly instead of
         * landing on the spokes of a wheel the way i * (2pi/N) would. */
        float angle = (float)i * 2.39996f;
        /* Alternate the throw distance so the burst has depth. */
        float reach = (i % 3 == 0) ? 1.0f : (i % 3 == 1 ? 0.72f : 0.45f);
        float dist = spread * reach * exit_t;
        float x = cx + cosf(angle) * dist;
        float y = cy + sinf(angle) * dist * 0.55f;   /* flattened: it is a banner */

        /* Each shard brightens then snuffs out, rather than all of them fading
         * together -- that difference is what makes it read as twinkling. */
        float phase = exit_t * 3.0f + (float)i * 0.37f;
        float spark = 0.5f + 0.5f * sinf(phase * 6.2831853f);
        float alpha = spark * (1.0f - exit_t) * 0.9f;
        if (alpha <= 0.0f) continue;

        float size = (3.0f + 2.0f * spark) * (1.0f - exit_t * 0.5f);
        SDL_SetRenderDrawColorFloat(renderer, 1.0f, 0.93f, 0.6f, alpha);
        SDL_RenderFillRect(renderer, &(SDL_FRect){x - size * 0.5f,
                                                  y - size * 0.5f, size, size});
    }

    SDL_SetRenderDrawBlendMode(renderer, prev);
}

void wave_banner_draw(SDL_Renderer *renderer, TTF_Font *font, int screen_w,
                      int screen_h) {
    if (!g_banner.active || !renderer || !font) return;
    if (!ensure_text_texture(renderer, font, g_banner.text, &g_banner.tex,
                             &g_banner.tex_w, &g_banner.tex_h)) {
        return;
    }
    if (g_banner.sub[0] != '\0') {
        ensure_text_texture(renderer, font, g_banner.sub, &g_banner.sub_tex,
                            &g_banner.sub_w, &g_banner.sub_h);
    }

    float home_x = (float)screen_w * 0.5f;
    float home_y = (float)screen_h * 0.32f;

    float exit_t = wave_banner_exit_progress();
    /* Ease-in on the way out: it leaves reluctantly, then quickly. */
    float ease = exit_t * exit_t;

    float scale = banner_pop_scale(g_banner.age) * (1.0f - 0.82f * ease);
    float cx = home_x + (BANNER_TARGET_X - home_x) * ease;
    float cy = home_y + (BANNER_TARGET_Y - home_y) * ease;

    /* Alpha stays up almost to the end. The banner is meant to arrive in the
     * HUD, not to dissolve on the way -- the last 15% only hides the final
     * frame, where the shrunk text would otherwise pop out of existence. */
    float alpha = 1.0f;
    if (exit_t > 0.85f) alpha = (1.0f - exit_t) / 0.15f;
    if (g_banner.age < 0.12f) alpha = g_banner.age / 0.12f;

    float w = g_banner.tex_w * scale;
    float h = g_banner.tex_h * scale;

    /* The shards come off the banner's current size and position, so the
     * twinkle follows the text in rather than hanging where it used to be. */
    draw_twinkle(renderer, cx, cy, w * 0.75f, exit_t);

    SDL_SetTextureBlendMode(g_banner.tex, SDL_BLENDMODE_BLEND);
    SDL_SetTextureAlphaModFloat(g_banner.tex, alpha);
    SDL_RenderTexture(renderer, g_banner.tex, NULL,
                      &(SDL_FRect){cx - w * 0.5f, cy - h * 0.5f, w, h});

    /* The subtitle leaves first: by the time the banner is flying it would be
     * illegible anyway, and two shrinking strings land on top of each other. */
    if (g_banner.sub_tex && exit_t < 0.35f) {
        float sub_alpha = alpha * (1.0f - exit_t / 0.35f);
        float sw = g_banner.sub_w * scale * 0.55f;
        float sh = g_banner.sub_h * scale * 0.55f;
        SDL_SetTextureBlendMode(g_banner.sub_tex, SDL_BLENDMODE_BLEND);
        SDL_SetTextureAlphaModFloat(g_banner.sub_tex, sub_alpha);
        SDL_RenderTexture(renderer, g_banner.sub_tex, NULL,
                          &(SDL_FRect){cx - sw * 0.5f, cy + h * 0.55f, sw, sh});
    }
}
