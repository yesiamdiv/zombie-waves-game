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