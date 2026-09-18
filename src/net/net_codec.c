#include "net_codec.h"
#include <string.h>

/* Wire bytes are explicitly little-endian so a packet survives a cross-endian
 * machine unchanged (LAN co-op; the host and clients may not share endianness).
 * Every read is bounds-checked; every matching encode/decode must round-trip
 * byte-identically (unit-tested in tests/tests.c). */

static int put_u8(uint8_t *buf, int cap, int *off, uint8_t v) {
    if (*off + 1 > cap) return -1;
    buf[(*off)++] = v;
    return 0;
}

static int put_u16(uint8_t *buf, int cap, int *off, uint16_t v) {
    if (*off + 2 > cap) return -1;
    buf[(*off)++] = (uint8_t)(v & 0xFF);
    buf[(*off)++] = (uint8_t)((v >> 8) & 0xFF);
    return 0;
}

static int put_u32(uint8_t *buf, int cap, int *off, uint32_t v) {
    if (*off + 4 > cap) return -1;
    for (int i = 0; i < 4; i++) buf[(*off)++] = (uint8_t)((v >> (8 * i)) & 0xFF);
    return 0;
}

static int put_bytes(uint8_t *buf, int cap, int *off, const void *src, int n) {
    if (n < 0 || *off + n > cap) return -1;
    memcpy(buf + *off, src, (size_t)n);
    *off += n;
    return 0;
}

static int get_u8(const uint8_t *buf, int len, int *off, uint8_t *v) {
    if (*off + 1 > len) return -1;
    *v = buf[(*off)++];
    return 0;
}

static int get_u16(const uint8_t *buf, int len, int *off, uint16_t *v) {
    if (*off + 2 > len) return -1;
    *v = (uint16_t)(buf[*off] | ((uint16_t)buf[*off + 1] << 8));
    *off += 2;
    return 0;
}

static int get_u32(const uint8_t *buf, int len, int *off, uint32_t *v) {
    if (*off + 4 > len) return -1;
    *v = 0;
    for (int i = 0; i < 4; i++) *v |= (uint32_t)buf[(*off)++] << (8 * i);
    return 0;
}

/* IEEE-754 f32 packed as little-endian u32 bits so cross-endian hosts/clients
 * agree without the downstream code knowing the wire order. */
static int put_f32(uint8_t *buf, int cap, int *off, float v) {
    uint32_t bits;
    memcpy(&bits, &v, sizeof(bits));
    return put_u32(buf, cap, off, bits);
}

static int get_f32(const uint8_t *buf, int len, int *off, float *v) {
    uint32_t bits;
    if (get_u32(buf, len, off, &bits) != 0) return -1;
    memcpy(v, &bits, sizeof(bits));
    return 0;
}

/* name is stored as u8 length followed by the raw bytes (no NUL on the wire). */
static int put_name(uint8_t *buf, int cap, int *off, const char *name) {
    size_t n = name ? strlen(name) : 0;
    if (n > NET_NAME_MAX) n = NET_NAME_MAX;
    if (put_u8(buf, cap, off, (uint8_t)n) != 0) return -1;
    return put_bytes(buf, cap, off, name, (int)n);
}

static int get_name(const uint8_t *buf, int len, int *off, char *out, int out_cap) {
    uint8_t n;
    if (get_u8(buf, len, off, &n) != 0) return -1;
    if (out_cap < (int)n + 1) return -1;      /* need room for the NUL */
    if (*off + n > len) return -1;            /* truncated on the wire */
    memcpy(out, buf + *off, n);
    out[n] = '\0';
    *off += n;
    return 0;
}

int net_hdr_encode(uint8_t out[NET_HDR_SIZE], const NetHeader *h) {
    if (!out || !h) return -1;
    out[0] = h->version;
    out[1] = h->kind;
    out[2] = (uint8_t)(h->seq & 0xFF);
    out[3] = (uint8_t)((h->seq >> 8) & 0xFF);
    out[4] = h->from_slot;
    out[5] = h->flags;
    uint8_t sum = 0;
    for (int i = 0; i < NET_HDR_SIZE - 1; i++) sum = (uint8_t)(sum + out[i]);
    out[6] = sum;
    return 0;
}

int net_hdr_decode(NetHeader *h, const uint8_t in[NET_HDR_SIZE]) {
    if (!h || !in) return -1;
    h->version = in[0];
    h->kind = in[1];
    h->seq = (uint16_t)(in[2] | ((uint16_t)in[3] << 8));
    h->from_slot = in[4];
    h->flags = in[5];
    uint8_t sum = 0;
    for (int i = 0; i < NET_HDR_SIZE - 1; i++) sum = (uint8_t)(sum + in[i]);
    if (sum != in[6]) return -1;   /* corrupted header; drop */
    return 0;
}

/* Decode just the header from the start of a full packet payload. */
static int decode_hdr_from_packet(NetHeader *h, const uint8_t *buf, int len) {
    if (!buf || len < NET_HDR_SIZE) return -1;
    return net_hdr_decode(h, buf);
}

/* ------------------------------------------------------------------ JOIN */

int net_encode_join(uint8_t *buf, int cap, const NetHeader *h, const char *name) {
    if (!buf || !h || cap < NET_HDR_SIZE) return -1;
    int off = 0;
    if (net_hdr_encode(buf, h) != 0) return -1;
    off = NET_HDR_SIZE;
    if (put_name(buf, cap, &off, name) != 0) return -1;
    return off;
}

int net_decode_join(const uint8_t *buf, int len, NetHeader *h, char *name, int name_cap) {
    if (decode_hdr_from_packet(h, buf, len) != 0) return -1;
    if (h->kind != NET_PKT_JOIN) return -1;
    int off = NET_HDR_SIZE;
    return get_name(buf, len, &off, name, name_cap);
}

/* ----------------------------------------------------------------- HELLO */

int net_encode_hello(uint8_t *buf, int cap, const NetHeader *h, uint8_t slot,
                     uint32_t seed, uint32_t world_gen, const char *host_name) {
    if (!buf || !h || cap < NET_HDR_SIZE) return -1;
    int off = 0;
    if (net_hdr_encode(buf, h) != 0) return -1;
    off = NET_HDR_SIZE;
    if (put_u8(buf, cap, &off, slot) != 0) return -1;
    if (put_u32(buf, cap, &off, seed) != 0) return -1;
    if (put_u32(buf, cap, &off, world_gen) != 0) return -1;
    if (put_name(buf, cap, &off, host_name) != 0) return -1;
    return off;
}

int net_decode_hello(const uint8_t *buf, int len, NetHeader *h, uint8_t *slot,
                     uint32_t *seed, uint32_t *world_gen, char *host_name, int name_cap) {
    if (decode_hdr_from_packet(h, buf, len) != 0) return -1;
    if (h->kind != NET_PKT_HELLO) return -1;
    int off = NET_HDR_SIZE;
    if (get_u8(buf, len, &off, slot) != 0) return -1;
    if (get_u32(buf, len, &off, seed) != 0) return -1;
    if (get_u32(buf, len, &off, world_gen) != 0) return -1;
    if (get_name(buf, len, &off, host_name, name_cap) != 0) return -1;
    return 0;
}

/* --------------------------------------------------------------- REJECT */

int net_encode_reject(uint8_t *buf, int cap, const NetHeader *h, uint8_t reason) {
    if (!buf || !h || cap < NET_HDR_SIZE) return -1;
    int off = 0;
    if (net_hdr_encode(buf, h) != 0) return -1;
    off = NET_HDR_SIZE;
    if (put_u8(buf, cap, &off, reason) != 0) return -1;
    return off;
}

int net_decode_reject(const uint8_t *buf, int len, NetHeader *h, uint8_t *reason) {
    if (decode_hdr_from_packet(h, buf, len) != 0) return -1;
    if (h->kind != NET_PKT_REJECT) return -1;
    int off = NET_HDR_SIZE;
    return get_u8(buf, len, &off, reason);
}

/* --------------------------------------------------------- PLAYER LIST */

int net_encode_player_list(uint8_t *buf, int cap, const NetPlayerInfo *players,
                           int count) {
    if (!buf || cap < NET_HDR_SIZE) return -1;
    if (count < 0 || count > NET_MAX_PLAYERS) return -1;
    if (!players && count != 0) return -1;
    NetHeader h = {NET_WIRE_VERSION, NET_PKT_PLAYER_LIST, 0, 0, 0};
    int off = 0;
    if (net_hdr_encode(buf, &h) != 0) return -1;
    off = NET_HDR_SIZE;
    if (put_u8(buf, cap, &off, (uint8_t)count) != 0) return -1;
    for (int i = 0; i < count; i++) {
        if (put_u8(buf, cap, &off, players[i].slot) != 0) return -1;
        if (put_u8(buf, cap, &off, players[i].color) != 0) return -1;
        if (put_name(buf, cap, &off, players[i].name) != 0) return -1;
    }
    return off;
}

int net_decode_player_list(const uint8_t *buf, int len, NetHeader *h,
                           NetPlayerInfo *players, int count_max, int *count_out) {
    if (decode_hdr_from_packet(h, buf, len) != 0) return -1;
    if (h->kind != NET_PKT_PLAYER_LIST) return -1;
    int off = NET_HDR_SIZE;
    uint8_t n;
    if (get_u8(buf, len, &off, &n) != 0) return -1;
    if ((int)n > count_max) return -1;
    for (int i = 0; i < n; i++) {
        if (get_u8(buf, len, &off, &players[i].slot) != 0) return -1;
        if (get_u8(buf, len, &off, &players[i].color) != 0) return -1;
        if (get_name(buf, len, &off, players[i].name, NET_NAME_CAP) != 0) return -1;
    }
    *count_out = n;
    return 0;
}

/* ---------------------------------------------------------------- LEAVE */

int net_encode_leave(uint8_t *buf, int cap, const NetHeader *h, uint8_t reason) {
    if (!buf || !h || cap < NET_HDR_SIZE) return -1;
    int off = 0;
    if (net_hdr_encode(buf, h) != 0) return -1;
    off = NET_HDR_SIZE;
    if (put_u8(buf, cap, &off, reason) != 0) return -1;
    return off;
}

int net_decode_leave(const uint8_t *buf, int len, NetHeader *h, uint8_t *reason) {
    if (decode_hdr_from_packet(h, buf, len) != 0) return -1;
    if (h->kind != NET_PKT_LEAVE) return -1;
    int off = NET_HDR_SIZE;
    return get_u8(buf, len, &off, reason);
}

/* ---------------------------------------------------------------- INPUT */

int net_encode_input(uint8_t *buf, int cap, const NetHeader *h,
                     const NetInput *in) {
    if (!buf || !h || !in || cap < NET_HDR_SIZE) return -1;
    int off = 0;
    if (net_hdr_encode(buf, h) != 0) return -1;
    off = NET_HDR_SIZE;
    if (put_u8(buf, cap, &off, in->move_flags) != 0) return -1;
    if (put_u8(buf, cap, &off, in->buttons) != 0) return -1;
    if (put_u8(buf, cap, &off, in->weapon) != 0) return -1;
    if (put_f32(buf, cap, &off, in->aim_x) != 0) return -1;
    if (put_f32(buf, cap, &off, in->aim_y) != 0) return -1;
    return off;
}

int net_decode_input(const uint8_t *buf, int len, NetHeader *h, NetInput *in) {
    if (decode_hdr_from_packet(h, buf, len) != 0) return -1;
    if (h->kind != NET_PKT_INPUT) return -1;
    int off = NET_HDR_SIZE;
    if (get_u8(buf, len, &off, &in->move_flags) != 0) return -1;
    if (get_u8(buf, len, &off, &in->buttons) != 0) return -1;
    if (get_u8(buf, len, &off, &in->weapon) != 0) return -1;
    if (get_f32(buf, len, &off, &in->aim_x) != 0) return -1;
    if (get_f32(buf, len, &off, &in->aim_y) != 0) return -1;
    return 0;
}

/* ------------------------------------------------------------- SNAPSHOT */

int net_encode_snapshot(uint8_t *buf, int cap, const NetHeader *h,
                        const NetSnapshot *snap) {
    if (!buf || !h || !snap) return -1;
    if (snap->count < 0 || snap->count > NET_SNAP_MAX_ENTITIES) return -1;
    int off = 0;
    if (net_hdr_encode(buf, h) != 0) return -1;
    off = NET_HDR_SIZE;
    if (put_f32(buf, cap, &off, snap->sim_time) != 0) return -1;
    for (int i = 0; i < NET_MAX_PLAYERS; i++) {
        if (put_u16(buf, cap, &off, snap->slot_entities[i]) != 0) return -1;
    }
    if (put_u16(buf, cap, &off, snap->wave_number) != 0) return -1;
    if (put_u8(buf, cap, &off, snap->wave_active) != 0) return -1;
    if (put_u16(buf, cap, &off, snap->total_kills) != 0) return -1;
    if (put_u8(buf, cap, &off, (uint8_t)snap->count) != 0) return -1;
    for (int i = 0; i < snap->count; i++) {
        const NetEntitySnap *e = &snap->entities[i];
        if (put_u16(buf, cap, &off, e->id) != 0) return -1;
        if (put_u8(buf, cap, &off, e->kind) != 0) return -1;
        if (put_f32(buf, cap, &off, e->pos.x) != 0) return -1;
        if (put_f32(buf, cap, &off, e->pos.y) != 0) return -1;
        if (put_f32(buf, cap, &off, e->vel.x) != 0) return -1;
        if (put_f32(buf, cap, &off, e->vel.y) != 0) return -1;
        if (put_f32(buf, cap, &off, e->hp) != 0) return -1;
        if (put_u8(buf, cap, &off, e->flags) != 0) return -1;
        if (put_u16(buf, cap, &off, e->owner) != 0) return -1;
    }
    return off;
}

int net_decode_snapshot(const uint8_t *buf, int len, NetHeader *h,
                        NetSnapshot *snap, int max_entities) {
    if (decode_hdr_from_packet(h, buf, len) != 0) return -1;
    if (h->kind != NET_PKT_SNAPSHOT) return -1;
    int off = NET_HDR_SIZE;
    if (get_f32(buf, len, &off, &snap->sim_time) != 0) return -1;
    for (int i = 0; i < NET_MAX_PLAYERS; i++) {
        if (get_u16(buf, len, &off, &snap->slot_entities[i]) != 0) return -1;
    }
    if (get_u16(buf, len, &off, &snap->wave_number) != 0) return -1;
    if (get_u8(buf, len, &off, &snap->wave_active) != 0) return -1;
    if (get_u16(buf, len, &off, &snap->total_kills) != 0) return -1;
    uint8_t n;
    if (get_u8(buf, len, &off, &n) != 0) return -1;
    if ((int)n > max_entities) return -1;
    snap->count = n;
    for (int i = 0; i < n; i++) {
        NetEntitySnap *e = &snap->entities[i];
        if (get_u16(buf, len, &off, &e->id) != 0) return -1;
        if (get_u8(buf, len, &off, &e->kind) != 0) return -1;
        if (get_f32(buf, len, &off, &e->pos.x) != 0) return -1;
        if (get_f32(buf, len, &off, &e->pos.y) != 0) return -1;
        if (get_f32(buf, len, &off, &e->vel.x) != 0) return -1;
        if (get_f32(buf, len, &off, &e->vel.y) != 0) return -1;
        if (get_f32(buf, len, &off, &e->hp) != 0) return -1;
        if (get_u8(buf, len, &off, &e->flags) != 0) return -1;
        if (get_u16(buf, len, &off, &e->owner) != 0) return -1;
    }
    return 0;
}

/* ----------------------------------------------------------------------- */

const char *net_pkt_kind_name(int kind) {
    switch (kind) {
        case NET_PKT_JOIN:        return "join";
        case NET_PKT_HELLO:       return "hello";
        case NET_PKT_REJECT:      return "reject";
        case NET_PKT_PLAYER_LIST: return "player_list";
        case NET_PKT_LEAVE:       return "leave";
        case NET_PKT_INPUT:       return "input";
        case NET_PKT_SNAPSHOT:    return "snapshot";
        default:                  return "unknown";
    }
}

const char *net_reject_reason_name(int reason) {
    switch (reason) {
        case NET_REJECT_FULL:    return "lobby full";
        case NET_REJECT_VERSION: return "version mismatch";
        case NET_REJECT_WORLD:   return "world-gen mismatch";
        case NET_REJECT_OTHER:   return "rejected";
        default:                 return "unknown";
    }
}

const char *net_leave_reason_name(int reason) {
    switch (reason) {
        case NET_LEAVE_NORMAL:        return "left";
        case NET_LEAVE_HOST_STOPPED:  return "host stopped";
        default:                      return "unknown";
    }
}