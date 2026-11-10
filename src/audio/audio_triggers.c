#include "audio/audio_triggers.h"

#include <string.h>

/* GE_PLAYER_HEALTH carries the hp *after* the change; the relay drops hp
 * before (net_client re-emits with ia=0), so hurt-vs-heal is decided from the
 * last value seen for that entity. The host uses the same cache on purpose: if
 * it used its own `ia` the two peers would hear different sounds for the same
 * event (docs/SYSTEM_DESIGN_RULES.md §3). */
#define HEALTH_CACHE 8

typedef struct {
    Entity id;
    float hp;
    bool used;
} HealthEntry;

static HealthEntry health_cache[HEALTH_CACHE];
static int health_next;

void audio_triggers_reset(void) {
    memset(health_cache, 0, sizeof(health_cache));
    health_next = 0;
}

static HealthEntry *health_find(Entity id) {
    for (int i = 0; i < HEALTH_CACHE; i++) {
        if (health_cache[i].used && health_cache[i].id == id) {
            return &health_cache[i];
        }
    }
    return NULL;
}

static void health_store(Entity id, float hp) {
    HealthEntry *e = health_find(id);
    if (!e) {
        e = &health_cache[health_next];
        health_next = (health_next + 1) % HEALTH_CACHE;
        e->id = id;
        e->used = true;
    }
    e->hp = hp;
}

static AudioSfx health_sfx(const GameEvent *ev) {
    if (ev->kind != GEK_PLAYER) return AUDIO_SFX_NONE;

    float after = ev->a;
    float max = ev->b;
    /* First sighting of an entity: assume it started at full health, so an
     * event below max is damage and one at max is nothing to play. */
    float before = max > 0.0f ? max : after;
    HealthEntry *e = health_find(ev->entity);
    if (e) before = e->hp;

    health_store(ev->entity, after);
    return after < before - 0.5f ? AUDIO_SFX_PLAYER_HURT : AUDIO_SFX_NONE;
}

AudioSfx audio_sfx_for_event(const GameEvent *ev) {
    if (!ev) return AUDIO_SFX_NONE;

    switch (ev->type) {
    case GE_PLAYER_SHOT:
        switch (ev->kind) {
        case GEK_BULLET: return AUDIO_SFX_SHOT_BULLET;
        case GEK_GRENADE: return AUDIO_SFX_SHOT_GRENADE;
        case GEK_ROCKET: return AUDIO_SFX_SHOT_ROCKET;
        default: return AUDIO_SFX_NONE;
        }
    case GE_DAMAGE:
        switch (ev->kind) {
        case GEK_ZOMBIE: return AUDIO_SFX_HIT_ZOMBIE;
        case GEK_PLAYER: return AUDIO_SFX_HIT_PLAYER;
        default: return AUDIO_SFX_NONE;
        }
    case GE_KILL:
        /* The death of the thing that was killed, not the bullet that did it. */
        switch (ev->kind) {
        case GEK_ZOMBIE: return AUDIO_SFX_DEATH_ZOMBIE;
        case GEK_PLAYER: return AUDIO_SFX_DEATH_PLAYER;
        default: return AUDIO_SFX_NONE;
        }
    case GE_EXPLOSION:
        /* NT2: grenades and rockets both report their blast here, at the
         * impact point. Do not also play GE_ENTITY_DEATH: that fires for
         * bullets, particles and the rocket's expiry, and would double the
         * grenade. */
        return AUDIO_SFX_EXPLOSION;
    case GE_WAVE_START: return AUDIO_SFX_WAVE_START;
    case GE_WAVE_END: return AUDIO_SFX_WAVE_END;
    case GE_ITEM_PICKUP: return AUDIO_SFX_PICKUP;
    case GE_POINTS: return AUDIO_SFX_POINTS;
    case GE_PLAYER_HEALTH: return health_sfx(ev);
    default:
        /* GE_ENTITY_SPAWN is host-only, GE_SHOP_PURCHASE never leaves the
         * host, GE_INPUT/GE_POSITION_SAMPLE are plumbing. No sound, and no
         * new event type was added to make one. */
        return AUDIO_SFX_NONE;
    }
}

bool audio_sfx_is_2d(AudioSfx sfx) {
    switch (sfx) {
    case AUDIO_SFX_WAVE_START:
    case AUDIO_SFX_WAVE_END:
    case AUDIO_SFX_POINTS:
        return true;
    default:
        return false;
    }
}
