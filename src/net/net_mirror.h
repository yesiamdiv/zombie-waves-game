#ifndef NET_MIRROR_H
#define NET_MIRROR_H

#include "net.h"

typedef struct {
    NetSnapshot older;      /* t=0 interpolation basis */
    NetSnapshot newer;      /* t=1 interpolation basis */
    bool has_older;
    bool has_newer;
} NetMirror;

void net_mirror_reset(NetMirror *m);

/* Feed the newest 20 Hz snapshot. The previous newest becomes the older basis
 * (two-snapshot interpolation). Safe to call with older==newer sim_time. */
void net_mirror_push(NetMirror *m, const NetSnapshot *snap);

/* True once at least one snapshot has been pushed. */
bool net_mirror_ready(const NetMirror *m);

/* Interpolated sample for `id`. `t` in [0,1] blends older->newer (0 = older
 * snapshot, 1 = newer). Entities only in the newer snapshot return their newer
 * state regardless of t. Returns false when the id is absent. */
bool net_mirror_sample(const NetMirror *m, uint16_t id, float t,
                       NetEntitySnap *out);

/* Roster slot owning `id`, or -1 when `id` is not a networked player. */
int net_mirror_slot_for(const NetMirror *m, uint16_t id);

/* Push a new snapshot and then strip any entities whose ids appear in `dead`.
 * The host relays GE_ENTITY_DEATH on ch0 to let clients tidy the mirror
 * ahead of the next 20 Hz snapshot (§6.3). Safe when ndead==0. */
void net_mirror_push_removing(NetMirror *m, const NetSnapshot *snap,
                              const uint16_t *dead, int ndead);

float net_mirror_newer_time(const NetMirror *m);
float net_mirror_older_time(const NetMirror *m);

#endif