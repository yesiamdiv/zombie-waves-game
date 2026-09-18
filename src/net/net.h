#ifndef NET_H
#define NET_H

#include <stdbool.h>
#include <stdint.h>
#include "graphics/sprite.h"

/* ---------------------------------------------------------------------------
 * Wire protocol for the multiplayer layer (docs/MULTIPLAYER_PLAN.md §5-§8).
 *
 * Transport: ENet over UDP. Channel 0 = reliable/ordered control traffic
 * (handshake + events), channel 1 = unreliable/sequenced game traffic
 * (inputs up, snapshots down). Every payload is a fixed 7-byte header plus a
 * packet-specific body; the header carries a self checksum (byte sum) so a
 * corrupted unreliable packet is dropped at the decoder (matches the spike's
 * GamePacket discipline).
 * ------------------------------------------------------------------------- */

#define NET_WIRE_VERSION 2
/* Maximum name length carried on the wire; buffers should be NET_NAME_CAP
 * (NET_NAME_MAX chars + NUL) to avoid silent truncation. */
#define NET_NAME_MAX 32
#define NET_NAME_CAP (NET_NAME_MAX + 1)
#define NET_MAX_PLAYERS 4               /* matches MAX_PLAYERS (players.h) */
#define NET_DEFAULT_PORT 5123
#define NET_WORLD_GEN_VERSION 1         /* world_init is rand()-free; guards */

#define NET_CH_CTRL 0                   /* reliable / ordered handshake+events */
#define NET_CH_SNAP 1                   /* unreliable / sequenced game traffic */

#define NET_HDR_SIZE 7

#define NET_FLAG_NONE 0
#define NET_FLAG_VERSION_MISMATCH 0x01  /* set on REJECT for old/new clients */
#define NET_FLAG_WORLD_MISMATCH 0x02

/* Packet kinds (channel 0) */
enum {
    NET_PKT_JOIN        = 1, /* client -> host: version probe + player name */
    NET_PKT_HELLO       = 2, /* host -> client: accept (slot, seed, world) */
    NET_PKT_REJECT      = 3, /* host -> client: explain the refusal */
    NET_PKT_PLAYER_LIST = 4, /* host -> clients: lobby roster on changes */
    NET_PKT_LEAVE       = 5  /* either side: intentional leave (reason) */
};

/* REJECT reasons */
enum {
    NET_REJECT_NONE = 0,
    NET_REJECT_FULL = 1,      /* all slots taken */
    NET_REJECT_VERSION = 2,   /* NET_WIRE_VERSION mismatch */
    NET_REJECT_WORLD = 3,     /* NET_WORLD_GEN_VERSION mismatch */
    NET_REJECT_OTHER = 4
};

/* LEAVE reasons */
enum {
    NET_LEAVE_NORMAL = 0,
    NET_LEAVE_HOST_STOPPED = 1
};

/* Fixed header layout: version, kind, seq, from_slot, flags, checksum.
 * `seq` is a client-frame number (wraps); the checksum covers bytes 0..5. */
typedef struct {
    uint8_t version;
    uint8_t kind;
    uint16_t seq;
    uint8_t from_slot;
    uint8_t flags;
} NetHeader;

/* One lobby roster entry: slot id, display name, slot color index. */
typedef struct {
    uint8_t slot;
    char name[NET_NAME_CAP];
    uint8_t color;                 /* index into net_slot_color() */
} NetPlayerInfo;

/* Slot -> player color (multiplayer uses a fixed 4-color palette so both
 * sides agree without sending RGBA over the wire). */
static inline SDL_FColor net_slot_color(int slot) {
    switch (slot % 4) {
        case 0: return COLOR_BLUE;
        case 1: return COLOR_RED;
        case 2: return COLOR_GREEN;
        case 3: return COLOR_YELLOW;
        default: return COLOR_WHITE;
    }
}

/* Convenience: human-readable names for logs/tests. */
const char *net_pkt_kind_name(int kind);
const char *net_reject_reason_name(int reason);
const char *net_leave_reason_name(int reason);

/* Initialize ENet globally (idempotent). Returns 0 or -1. Called automatically
 * by net_server_host()/net_client_init(); safe to call early too. */
int net_init(void);

#define NET_VERSION_STR(MINOR) #MINOR
#define NET_WIRE_VER_STR "2"

#endif