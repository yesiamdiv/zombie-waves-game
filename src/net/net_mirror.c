#include "net.h"
#include "net_mirror.h"

void net_mirror_reset(NetMirror *m) {
    if (!m) return;
    *m = (NetMirror){0};
}

void net_mirror_push(NetMirror *m, const NetSnapshot *snap) {
    if (!m || !snap) return;
    if (m->has_newer) {
        m->older = m->newer;
        m->has_older = true;
    }
    m->newer = *snap;
    m->has_newer = true;
    m->now_sim_time = snap->sim_time;
    /* Record first appearance for spawn-in (host and client). */
    for (int i = 0; i < snap->count; i++) {
        uint16_t id = snap->entities[i].id;
        bool seen = false;
        for (int s2 = 0; s2 < NET_MIRROR_SPAWN_MAX; s2++) {
            if (m->spawn[s2].in_use && m->spawn[s2].id == id) { seen = true; break; }
        }
        if (seen) continue;
        for (int s2 = 0; s2 < NET_MIRROR_SPAWN_MAX; s2++) {
            if (!m->spawn[s2].in_use) {
                m->spawn[s2].in_use = true;
                m->spawn[s2].id = id;
                m->spawn[s2].first_sim_time = snap->sim_time;
                break;
            }
        }
    }
}

void net_mirror_push_removing(NetMirror *m, const NetSnapshot *snap,
                              const uint16_t *dead, int ndead) {
    if (snap) net_mirror_push(m, snap);
    if (!m || !dead || ndead <= 0) return;

    for (int which = 0; which < 2; which++) {
        NetSnapshot *s = which == 0 ? &m->older : &m->newer;
        if (!s->count) continue;
        int w = 0;
        for (int i = 0; i < s->count; i++) {
            uint16_t id = s->entities[i].id;
            bool drop = false;
            for (int j = 0; j < ndead; j++) {
                if (dead[j] == id) { drop = true; break; }
            }
            if (!drop) s->entities[w++] = s->entities[i];
        }
        s->count = w;

        /* v9/NT4: attributes are keyed by the same entity id, so a dead id's
         * attribute must go with it. Leaving it behind would hand a recycled
         * id the old sword's angle as the "older" interpolation basis, and the
         * new blade would visibly snap through the dead one's spin. */
        int aw = 0;
        for (int i = 0; i < s->attr_count; i++) {
            uint16_t id = s->attrs[i].id;
            bool drop = false;
            for (int j = 0; j < ndead; j++) {
                if (dead[j] == id) { drop = true; break; }
            }
            if (!drop) s->attrs[aw++] = s->attrs[i];
        }
        s->attr_count = (uint16_t)aw;
    }
}

bool net_mirror_ready(const NetMirror *m) {
    return m && m->has_newer;
}

bool net_mirror_sample(const NetMirror *m, uint16_t id, float t,
                       NetEntitySnap *out) {
    if (!m || !out) return false;

    const NetEntitySnap *in_newer = NULL;
    const NetEntitySnap *in_older = NULL;

    if (m->has_newer) {
        for (int i = 0; i < m->newer.count; i++) {
            if (m->newer.entities[i].id == id) {
                in_newer = &m->newer.entities[i];
                break;
            }
        }
    }
    if (m->has_older) {
        for (int i = 0; i < m->older.count; i++) {
            if (m->older.entities[i].id == id) {
                in_older = &m->older.entities[i];
                break;
            }
        }
    }

    if (!in_newer) return false;

    float blend = (t < 0.0f) ? 0.0f : (t > 1.0f ? 1.0f : t);
    if (!in_older) blend = 1.0f;

    *out = *in_newer;
    if (in_older) {
        out->pos.x = in_older->pos.x + (in_newer->pos.x - in_older->pos.x) * blend;
        out->pos.y = in_older->pos.y + (in_newer->pos.y - in_older->pos.y) * blend;
    }
    return true;
}

static const NetEntityAttr *mirror_find_attr(const NetSnapshot *s,
                                             uint16_t id) {
    if (!s) return NULL;
    for (int i = 0; i < s->attr_count; i++) {
        if (s->attrs[i].id == id) return &s->attrs[i];
    }
    return NULL;
}

bool net_mirror_sample_attr(const NetMirror *m, uint16_t id, float t,
                            NetEntityAttr *out) {
    if (!m || !out || !m->has_newer) return false;
    const NetEntityAttr *an = mirror_find_attr(&m->newer, id);
    if (!an) return false;
    *out = *an;
    /* Only the sword has a defined interpolation. Any future attribute starts
     * as hold-newest until its own blending rule is written. */
    if (an->kind != NET_ENT_SWORD || an->len < 2) return true;

    const NetEntityAttr *ao = m->has_older ? mirror_find_attr(&m->older, id)
                                           : NULL;
    if (!ao || ao->kind != NET_ENT_SWORD || ao->len < 2) return true;

    float blend = (t < 0.0f) ? 0.0f : (t > 1.0f ? 1.0f : t);
    int32_t a0 = net_attr_angle_cdeg(ao);
    int32_t a1 = net_attr_angle_cdeg(an);
    /* Shortest path around the circle: +170deg -> -170deg is +20deg, not
     * -340deg. Without this the blade visibly unwinds at the seam. */
    int32_t d = a1 - a0;
    if (d > 18000) d -= 36000;
    else if (d < -18000) d += 36000;
    float f = (float)d * blend;
    int32_t v = a0 + (int32_t)(f >= 0.0f ? f + 0.5f : f - 0.5f);
    while (v > 18000) v -= 36000;
    while (v <= -18000) v += 36000;

    uint16_t bits = (uint16_t)(int16_t)v;
    out->bytes[0] = (uint8_t)(bits & 0xFFu);
    out->bytes[1] = (uint8_t)((bits >> 8) & 0xFFu);
    return true;
}

bool net_mirror_player_state(const NetMirror *m, int slot, NetPlayerState *out) {
    if (!m || !out || slot < 0 || slot >= NET_MAX_PLAYERS) return false;
    if (!m->has_newer) return false;
    const NetPlayerState *ps = &m->newer.player_states[slot];
    /* Never hand back a slot the host considers unused: a client that starts
     * drawing "0 points, dead" before the roster arrives looks broken. */
    if (!(ps->flags & NET_PST_IN_USE)) return false;
    *out = *ps;
    return true;
}

int net_mirror_slot_for(const NetMirror *m, uint16_t id) {
    if (!m || !m->has_newer) return -1;
    for (int s = 0; s < NET_MAX_PLAYERS; s++) {
        if (m->newer.slot_entities[s] == id) return s;
    }
    return -1;
}

float net_mirror_newer_time(const NetMirror *m) {
    return m && m->has_newer ? m->newer.sim_time : 0.0f;
}

float net_mirror_older_time(const NetMirror *m) {
    return m && m->has_older ? m->older.sim_time : 0.0f;
}
float net_mirror_spawn_age(const NetMirror *m, uint16_t id, float now_sim_time) {
    if (!m) return 1e9f;
    float t = now_sim_time >= 0.0f ? now_sim_time : m->now_sim_time;
    for (int i = 0; i < NET_MIRROR_SPAWN_MAX; i++) {
        if (m->spawn[i].in_use && m->spawn[i].id == id) {
            float a = t - m->spawn[i].first_sim_time;
            if (a < 0.0f) a = 0.0f;
            return a;
        }
    }
    /* First time seeing this entity id through this query: treat as just born. */
    /* Caller should prefer recording on push; this is a fallback. */
    return 0.0f;
}
