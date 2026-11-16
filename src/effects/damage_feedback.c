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

void damage_feedback_reset(const EventBus *bus) {
    /* Re-arm rather than zero. Sids keep counting across a reset, so starting
     * from 0 would replay the previous match's tail as a burst of popups over
     * the new one's first frame. */
    g_next_sid = bus ? bus->next_sid : 0;
    g_consumed = 0;
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

void damage_feedback_tick(const EventBus *bus, float dt,
                          float anchor_x, float anchor_y) {
    (void)dt;
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
            default:
                break;
        }
    }
}

void damage_feedback_draw(SDL_Renderer *renderer, Camera *cam) {
    (void)renderer;
    (void)cam;
    /* E4's muzzle flash lands here. */
}
