#include "effects/damage_feedback.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ecs/ecs.h"
#include "ui/text_popup.h"

/* The lowest sid not yet turned into feedback. The bus ring is written during
 * gameplay and drained by event_bus_flush(), so a watermark is what keeps one
 * hit from spawning a popup every frame until the drain.
 *
 * It is the NEXT sid rather than the LAST one on purpose. event_emit() hands
 * out `bus->next_sid++`, so the very first event of a session carries sid 0; a
 * "last consumed" watermark initialised to 0 silently swallows it, and the
 * first hit of every match shows nothing. (audio.c's last_sid has that shape
 * and therefore that bug; it is one dropped sound at startup, out of scope
 * here, but it is why this one is written the other way round.) */
static Uint64 g_next_sid = 0;
static int g_consumed = 0;

/* E4: muzzle flashes.
 *
 * A flash is held for its own short lifetime rather than drawn for whatever
 * happens to be in the event ring this frame. The P3 flash this replaces did
 * the latter, from inside render.c's per-entity loop -- so every shot was
 * drawn once per entity on screen (a quadratic redraw that over-brightened
 * the flash exactly when the screen was busiest), and its duration was
 * whatever the ring happened to hold rather than a decision. */
#define MAX_MUZZLE_FLASHES 32
#define MUZZLE_FLASH_LIFETIME 0.055f

typedef struct {
    float x, y;       /* world */
    float dx, dy;     /* unit aim direction; the flare is drawn along it */
    float age;
    bool active;
} MuzzleFlash;

static MuzzleFlash g_flashes[MAX_MUZZLE_FLASHES];

void damage_feedback_reset(const EventBus *bus) {
    /* Re-arm rather than zero. Sids keep counting across a reset, so starting
     * from 0 would replay the previous match's tail as a burst of popups over
     * the new one's first frame. */
    g_next_sid = bus ? bus->next_sid : 0;
    g_consumed = 0;
    memset(g_flashes, 0, sizeof(g_flashes));
}

int damage_feedback_events_consumed(void) {
    return g_consumed;
}

/* E2: damage numbers.
 *
 * GE_DAMAGE carries the hit position in x/y and the damage the host applied in
 * `a`, so the number shown is the host's, never a recomputation. Only zombie
 * hits get a number: damage to the player is already told by the HUD vignette
 * (assets' lane), and a number over your own feet during a swarm is noise on
 * top of the signal that matters. */
static void feedback_damage(const GameEvent *ev) {
    if (ev->kind != GEK_ZOMBIE) return;
    int dmg = (int)(ev->a + 0.5f);
    if (dmg <= 0) return;
    text_popup_spawn_damage(ev->x, ev->y, dmg);
}

/* E3: points.
 *
 * GE_POINTS carries the running total in `a` and the amount just awarded in
 * `b`, and -- unlike every other event here -- no world position: weapons.c
 * emits it at (0,0) because points belong to the player, not to a place. So
 * it is anchored on the local player, which is also the only reading that
 * works on a client, where the award may come from a kill on the far side of
 * the map. */
static void feedback_points(const GameEvent *ev, float anchor_x,
                            float anchor_y) {
    int awarded = (int)(ev->b + 0.5f);
    if (awarded <= 0) return;
    /* Above the head, clear of the damage numbers rising off zombies. */
    text_popup_spawn_points(anchor_x, anchor_y - 18.0f, awarded);
}

/* E3: pickups.
 *
 * `a` is the ItemType and `b` the value the host applied. Naming the item
 * rather than showing "+25" is the point: the player already sees their health
 * bar move, what they cannot tell at a glance in a swarm is WHICH of three
 * identical-sized blobs they just ran over. */
static const char *pickup_label(int type, float value, char *buf,
                                size_t buflen) {
    int v = (int)(value + 0.5f);
    switch (type) {
        case ITEM_HEALTH:
            snprintf(buf, buflen, "+%d HP", v);
            break;
        case ITEM_AMMO:
            snprintf(buf, buflen, "+%d AMMO", v);
            break;
        case ITEM_SPEED_BOOST:
            snprintf(buf, buflen, "SPEED+");
            break;
        default:
            /* A new ItemType must still produce feedback rather than silence;
             * an unnamed pickup is better than a pickup that looks like it did
             * nothing. */
            snprintf(buf, buflen, "PICKUP");
            break;
    }
    return buf;
}

static void feedback_pickup(const GameEvent *ev) {
    char buf[32];
    text_popup_spawn_pickup(ev->x, ev->y,
                            pickup_label((int)(ev->a + 0.5f), ev->b, buf,
                                         sizeof(buf)));
}

/* E4: the flash itself.
 *
 * GE_PLAYER_SHOT carries the muzzle position in x/y and the aim direction in
 * a/b (player_input.c passes the normalised bullet direction), so the flare
 * can point where the shot went instead of being a symmetrical blob. Grenades
 * and rockets emit the same event with their own throw/launch direction, and
 * a flash reads correctly for all three.
 *
 * The oldest flash is recycled when the buffer is full: during sustained fire
 * the newest shot is the one that must be visible. */
static void feedback_muzzle_flash(const GameEvent *ev) {
    MuzzleFlash *slot = NULL;
    float oldest_age = -1.0f;
    for (int i = 0; i < MAX_MUZZLE_FLASHES; i++) {
        if (!g_flashes[i].active) { slot = &g_flashes[i]; break; }
        if (g_flashes[i].age > oldest_age) {
            oldest_age = g_flashes[i].age;
            slot = &g_flashes[i];
        }
    }
    if (!slot) return;

    float dx = ev->a, dy = ev->b;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 0.0001f) {
        /* No usable direction (an emitter that only knows where, not which
         * way). Point it along +x rather than drawing a zero-length quad. */
        dx = 1.0f;
        dy = 0.0f;
    } else {
        dx /= len;
        dy /= len;
    }

    slot->active = true;
    slot->age = 0.0f;
    slot->x = ev->x;
    slot->y = ev->y;
    slot->dx = dx;
    slot->dy = dy;
}

static void muzzle_flashes_age(float dt) {
    for (int i = 0; i < MAX_MUZZLE_FLASHES; i++) {
        if (!g_flashes[i].active) continue;
        g_flashes[i].age += dt;
        if (g_flashes[i].age >= MUZZLE_FLASH_LIFETIME) {
            g_flashes[i].active = false;
        }
    }
}

int damage_feedback_flash_count(void) {
    int n = 0;
    for (int i = 0; i < MAX_MUZZLE_FLASHES; i++)
        if (g_flashes[i].active) n++;
    return n;
}

void damage_feedback_tick(const EventBus *bus, float dt,
                          float anchor_x, float anchor_y) {
    muzzle_flashes_age(dt);
    if (!bus) return;

    int count = bus->count;
    for (int i = 0; i < count; i++) {
        const GameEvent *ev = &bus->ring[(bus->head + i) % EV_MAX_EVENTS];
        if (ev->sid < g_next_sid) continue;
        g_next_sid = ev->sid + 1;
        g_consumed++;

        switch (ev->type) {
            case GE_DAMAGE:
                feedback_damage(ev);
                break;
            case GE_POINTS:
                feedback_points(ev, anchor_x, anchor_y);
                break;
            case GE_ITEM_PICKUP:
                feedback_pickup(ev);
                break;
            case GE_PLAYER_SHOT:
                feedback_muzzle_flash(ev);
                break;
            default:
                break;
        }
    }
}

/* Additive quad pivoted at the muzzle and extending along the aim direction,
 * plus a brighter, shorter core. Procedural rather than a PNG: it costs no
 * asset, no NET_ASSET_VERSION bump, and nothing to load on a client.
 *
 * The taper is over length, not alpha alone -- a flash that only fades looks
 * like a light being dimmed, while one that also shortens reads as a muzzle
 * blast collapsing back into the barrel. */
static void draw_flash_quad(SDL_Renderer *renderer, Camera *cam,
                            const MuzzleFlash *f, float length, float width,
                            float r, float g, float b, float alpha) {
    Vec2 origin = camera_world_to_screen(cam, vec2(f->x, f->y));
    float z = cam->zoom;
    /* Perpendicular to the aim direction gives the quad its width. */
    float px = -f->dy, py = f->dx;
    float lx = f->dx * length * z, ly = f->dy * length * z;
    float wx = px * width * 0.5f * z, wy = py * width * 0.5f * z;

    SDL_Vertex v[4];
    SDL_FColor col = {r, g, b, alpha};
    v[0].position = (SDL_FPoint){origin.x + wx, origin.y + wy};
    v[1].position = (SDL_FPoint){origin.x - wx, origin.y - wy};
    v[2].position = (SDL_FPoint){origin.x + lx - wx * 0.35f,
                                 origin.y + ly - wy * 0.35f};
    v[3].position = (SDL_FPoint){origin.x + lx + wx * 0.35f,
                                 origin.y + ly + wy * 0.35f};
    for (int i = 0; i < 4; i++) {
        v[i].color = col;
        v[i].tex_coord = (SDL_FPoint){0.0f, 0.0f};
    }
    const int idx[6] = {0, 1, 2, 2, 3, 0};
    SDL_RenderGeometry(renderer, NULL, v, 4, idx, 6);
}

void damage_feedback_draw(SDL_Renderer *renderer, Camera *cam) {
    if (!renderer || !cam) return;

    SDL_BlendMode prev = SDL_BLENDMODE_BLEND;
    SDL_GetRenderDrawBlendMode(renderer, &prev);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_ADD);

    for (int i = 0; i < MAX_MUZZLE_FLASHES; i++) {
        const MuzzleFlash *f = &g_flashes[i];
        if (!f->active) continue;
        if (!camera_is_visible(cam, vec2(f->x, f->y), 48.0f)) continue;

        float t = f->age / MUZZLE_FLASH_LIFETIME;
        if (t > 1.0f) t = 1.0f;
        float decay = 1.0f - t;

        draw_flash_quad(renderer, cam, f, 18.0f * decay, 9.0f * decay,
                        1.0f, 0.86f, 0.45f, 0.85f * decay);
        draw_flash_quad(renderer, cam, f, 10.0f * decay, 4.5f * decay,
                        1.0f, 1.0f, 0.92f, 0.95f * decay);
    }

    SDL_SetRenderDrawBlendMode(renderer, prev);
}
