#include "effects/damage_feedback.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

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

void damage_feedback_tick(const EventBus *bus, float dt,
                          float anchor_x, float anchor_y) {
    (void)dt;
    (void)anchor_x;
    (void)anchor_y;
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
