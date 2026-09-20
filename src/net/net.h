#ifndef NET_H
#define NET_H

#include <stdbool.h>
#include <stdint.h>
#include "core/mathutil.h"
#include "graphics/sprite.h"
#include "ecs/ecs.h"
#include "world/waves.h"
#include "players.h"

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
    NET_PKT_LEAVE       = 5, /* either side: intentional leave (reason) */
    /* Packet kinds (channel 1 - unreliable game traffic). */
    NET_PKT_INPUT      = 6,  /* client -> host: control state (latest-wins) */
    NET_PKT_SNAPSHOT   = 7,  /* host -> clients: 20 Hz entity world state */
    /* Reliable helper traffic (channel 0, after the handshake). */
    NET_PKT_EVENTS     = 8   /* host -> clients: curated GE_* event batches */
};

/* Input bitmasks for NET_PKT_INPUT. move_flags holds the 4 direction bits;
 * buttons holds context actions (shoot held / shop toggle). */
#define NET_INPUT_MOVE_UP    0x01
#define NET_INPUT_MOVE_DOWN  0x02
#define NET_INPUT_MOVE_LEFT  0x04
#define NET_INPUT_MOVE_RIGHT 0x08
#define NET_INPUT_BTN_SHOOT   0x01
#define NET_INPUT_BTN_SHOP    0x02

/* One client->host input sample. Sent >= 30 Hz on channel 1; the host keeps
 * only the latest per slot and maps it onto the AI-injection path. */
typedef struct {
    uint8_t move_flags;     /* NET_INPUT_MOVE_* */
    uint8_t buttons;        /* NET_INPUT_BTN_* */
    uint8_t weapon;         /* WEAPON_* enum value (0..3); static while held */
    float aim_x;            /* world-space aim */
    float aim_y;
} NetInput;

/* Semantic entity kinds carried in a snapshot (client reconstructs visuals
 * from `kind` deterministically - no sprite data crosses the wire). */
enum {
    NET_ENT_NONE   = 0,
    NET_ENT_PLAYER = 1,
    NET_ENT_ZOMBIE = 2,
    NET_ENT_BULLET = 3,
    NET_ENT_GRENADE = 4,
    NET_ENT_ROCKET = 5,
    NET_ENT_ITEM   = 6
};

#define NET_SNAP_MAX_ENTITIES 1024

/* One live entity in a snapshot. Entity ids are stable ECS array indices, so
 * the client keeps a mirror keyed by id (interpolation buffer, P2c). */
typedef struct {
    uint16_t id;
    uint8_t kind;
    Vec2 pos;
    Vec2 vel;
    float hp;
    uint8_t flags;
    uint16_t owner;         /* bullet/grenade/rocket source player, else 0 */
} NetEntitySnap;

/* 20 Hz host->client snapshot (channel 1). `slot_entities` maps each roster
 * slot to its live player entity id (0 when the slot has no entity), so a
 * client can identify which snapshot entity is its own player. */
typedef struct {
    float sim_time;         /* host simulation clock (seconds) */
    uint16_t slot_entities[NET_MAX_PLAYERS];
    uint16_t wave_number;
    uint8_t wave_active;
    uint16_t total_kills;
    int count;
    NetEntitySnap entities[NET_SNAP_MAX_ENTITIES];
} NetSnapshot;

#define NET_SNAP_HZ 20           /* host snapshot cadence (matches the plan) */
#define NET_INPUT_HZ 30          /* client input cadence (matches the plan) */
#define NET_SNAP_ENTRY_BYTES 26  /* NetEntitySnap wire size */
#define NET_SNAP_MAX_BYTES \
    (NET_HDR_SIZE + 18 + NET_SNAP_MAX_ENTITIES * NET_SNAP_ENTRY_BYTES)

/* One relayed gameplay event (subset of GE_*, §6.3). Payload semantics follow
 * the local emitters: DEATH -> id/kind/pos; PLAYER_HEALTH -> id, a=hp after,
 * b=hp max; WAVE_START -> a=wave, b=zombies_to_spawn. */
typedef struct {
    uint8_t type;    /* GameEventType */
    uint8_t kind;    /* EventKind */
    uint16_t id;     /* entity id (0 when none) */
    float x, y;
    float a, b;
} NetRelayedEvent;

#define NET_EVENTS_MAX_BATCH 48
#define NET_EVENTS_MAX_BYTES \
    (NET_HDR_SIZE + 1 + NET_EVENTS_MAX_BATCH * 20)

/* Snapshot builder: iterate the live ECS world, push all semantic entities
 * into `out` for broadcast, and stamp the slot->entity map. Must be called on
 * the host at the 20 Hz snapshot cadence (not every frame). Returns count. */
int net_snapshot_build(const World *ecs, const Player *players, int player_count,
                       float sim_time, const WaveSystem *waves, NetSnapshot *out);

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