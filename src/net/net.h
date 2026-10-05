#ifndef NET_H
#define NET_H

#include <stdbool.h>
#include <stdint.h>
#include "core/mathutil.h"
#include "graphics/sprite.h"
#include "ecs/ecs.h"
#include "world/waves.h"
#include "net_art.h"
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

/* N2: bumped 4 -> 5. The JOIN body gained an `asset_version` byte and the
 * HELLO body gained another, appended last in each so earlier fields do not
 * shift. A v4 peer's JOIN would still parse (it reads a name and stops) but a
 * v5 host's HELLO would leave a v4 client one byte short. Reject loudly rather
 * than connect and disagree about art.
 *
 * History: 2 -> 3, HELLO gained `map_index` (a v2 peer read that byte as the
 * first character of the host name). 3 -> 4, `NetEntitySnap` gained the
 * appearance block, 26 -> 31 bytes per entry. */
#define NET_WIRE_VERSION 7
/* Maximum name length carried on the wire; buffers should be NET_NAME_CAP
 * (NET_NAME_MAX chars + NUL) to avoid silent truncation. */
#define NET_NAME_MAX 32
#define NET_NAME_CAP (NET_NAME_MAX + 1)
#define NET_MAX_PLAYERS 4               /* matches MAX_PLAYERS (players.h) */
#define NET_DEFAULT_PORT 5123
/* R13 merge: bumped 1 -> 2. Worlds are no longer `world_init`-only; they come
 * from a map file plus a theme, and the map index now travels in the HELLO.
 * A peer that thinks the world is `world_init()`-shaped would agree on tiles
 * while disagreeing on everything the map contributes. */
#define NET_WORLD_GEN_VERSION 2         /* map-file-driven world; guards */

/* N2. Two peers must ship the same ART, not merely the same protocol. Without
 * this, a client built from a different asset tree renders different pictures
 * and neither side has any idea why — which is exactly the silent failure of
 * B15-B18, where a lost texture became a flat circle on one machine only.
 *
 * Bump this whenever anything in assets/ is added, removed, resized or
 * replaced, including the net art table in net_art.c (ids resolve to a pixel
 * size that the wire maths depends on). The REJECT reason is distinct from the
 * wire reason so the message can say which kind of mismatch it is. */
#define NET_ASSET_VERSION 1

#define NET_CH_CTRL 0                   /* reliable / ordered handshake+events */
#define NET_CH_SNAP 1                   /* unreliable / sequenced game traffic */

#define NET_HDR_SIZE 7

#define NET_FLAG_NONE 0
#define NET_FLAG_VERSION_MISMATCH 0x01  /* set on REJECT for old/new clients */
#define NET_FLAG_WORLD_MISMATCH 0x02
#define NET_FLAG_ASSET_MISMATCH 0x04  /* set on REJECT for art content drift */

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

/* Semantic entity kinds carried in a snapshot.
 *
 * This comment used to read "client reconstructs visuals from `kind`
 * deterministically - no sprite data crosses the wire". That was the R13-I5
 * bug: it was never deterministic, because zombie size is rand()'d and the
 * colour variant is a local in waves.c that never reaches the ECS. Appearance
 * now travels explicitly as `art`/`size_q`/`tint` in NetEntitySnap. `kind`
 * carries SEMANTICS (what this entity is) and no longer appearance. */
enum {
    NET_ENT_NONE   = 0,
    NET_ENT_PLAYER = 1,
    NET_ENT_ZOMBIE = 2,
    NET_ENT_BULLET = 3,
    NET_ENT_GRENADE = 4,
    NET_ENT_ROCKET = 5,
    NET_ENT_ITEM   = 6
};

/* Appearance of a snapshot entity (docs/NET_PROTOCOL_DESIGN.md ADR-1/2/3/4).
 *
 * R13-I5 was a client guessing a circle size per entity kind because the
 * snapshot said nothing about how anything looked. The host is the only party
 * that knows, so the host now says it explicitly and the client reproduces it.
 * The NET_ART_* ids live in net_art.h so the table does not depend on this
 * header.
 *
 * `art` indexes the shared art table rather than sending a path: both peers
 * resolve the id the same way, new artwork is a one-line append, and it costs
 * nothing per frame.
 *
 * `size_q` is a world-space diameter in HALF units (size = size_q / 2.0f),
 * deliberately not a texture-relative scale: a diameter is an absolute physical
 * size, so it survives art being rescaled or replaced. An integer decodes
 * bit-identically on every client, which a float invites "close enough" drift
 * into.
 *
 * `tint` is the host's literal RGB from CSprite.color. Sending resolved colour
 * rather than a theme index means the client needs no theme lookup at all,
 * which is the whole point: derivation is what caused this bug. */
#define NET_SNAP_MAX_ENTITIES 1024

/* One live entity in a snapshot. Entity ids are stable ECS array indices, so
 * the client keeps a mirror keyed by id (interpolation buffer, P2c). */
typedef struct {
    uint16_t id;
    uint8_t kind;
    Vec2 pos;
    Vec2 vel;
    float hp;
    /* v7. Base maximum, so a consumer never has to know the starting number.
     * `hp / 100.0f` was wrong from wave 2: waves.c scales zombie max HP by the
     * difficulty multiplier, so the bar quietly overflowed its own background. */
    float hp_max;
    uint8_t flags;
    uint16_t owner;         /* bullet/grenade/rocket source player, else 0 */
    uint8_t art;            /* NET_ART_*: which sprite the host drew */
    uint8_t size_q;         /* world diameter, half units */
    uint8_t tint[3];        /* host CSprite.color, 0-255 each */
} NetEntitySnap;

/* Per-slot authoritative player state (snapshot v6).
 *
 * Why this exists and is NOT read from the entity list: the client HUD used to
 * draw HP and the inventory from its OWN ECS/Player structs. On a render-only
 * client those are never simulated, so the numbers froze at join-time values
 * and simply disagreed with the host (B22/B23). Worse, `slot_entities[s]` is 0
 * whenever the host has the player dead, so there is no entity to ask.
 *
 * Every field is an integer count of centi-units rather than a float, for the
 * same reason as N1's size_q: an integer decodes bit-identically everywhere, so
 * two peers can never disagree because of rounding in transit. */
enum {
    NET_PST_IN_USE     = 1u << 0,
    NET_PST_ALIVE      = 1u << 1,
    NET_PST_ELIMINATED = 1u << 2
};

typedef struct {
    uint16_t points;          /* inventory currency */
    uint16_t grenades;        /* consumable stock */
    uint16_t launcher_ammo;   /* rockets */
    uint16_t hp_centis;       /* current HP * 100 */
    uint16_t hp_max_centis;   /* max HP * 100 */
    uint16_t respawn_centis;  /* seconds until respawn * 100, while dead */
    uint8_t  weapon;          /* WeaponType currently selected */
    uint8_t  unlocked_mask;   /* bit N set => weapon N is owned */
    uint8_t  flags;           /* NET_PST_* */
    /* v7. Where this slot's spawn beacon actually is. The client used to
     * recompute `spawn + (slot*70, slot*30)` from a copy of the host's
     * formula - it happened to agree, and would have stopped agreeing the
     * first time either side edited it. Positions are f32 like every other
     * position on the wire, because the mirror interpolates them. */
    float beacon_x;
    float beacon_y;
} NetPlayerState;

#define NET_PLAYER_STATE_BYTES 23   /* bytes on the wire, one slot */

/* 20 Hz host->client snapshot (channel 1). `slot_entities` maps each roster
 * slot to its live player entity id (0 when the slot has no entity), so a
 * client can identify which snapshot entity is its own player. */
typedef struct {
    float sim_time;         /* host simulation clock (seconds) */
    uint16_t slot_entities[NET_MAX_PLAYERS];
    /* v6: authoritative per-slot state, indexed by the SAME slot number as
     * slot_entities. Always sent, even for slots that are in use but dead. */
    NetPlayerState player_states[NET_MAX_PLAYERS];
    uint16_t wave_number;
    uint8_t wave_active;
    uint16_t total_kills;
    /* v7: the rest of what the HUD's wave panel shows. `waves_update()` never
     * runs on a render-only client, so without these the client HUD read
     * permanent zeros for wave/kills/zombies/countdown (B26). */
    uint16_t zombies_alive;
    uint8_t between_waves;
    uint16_t wave_cooldown_centis;   /* seconds until next wave * 100 */
    int count;
    NetEntitySnap entities[NET_SNAP_MAX_ENTITIES];
} NetSnapshot;

#define NET_SNAP_HZ 20           /* host snapshot cadence (matches the plan) */
#define NET_INPUT_HZ 30          /* client input cadence (matches the plan) */
#define NET_SNAP_ENTRY_BYTES 35  /* NetEntitySnap wire size */
/* Snapshot header, byte-exact. Written out per field rather than as a single
 * magic total because the tests assert the encoded length against this, and a
 * wrong constant there fails the build rather than the protocol - but only if
 * somebody keeps it honest.
 *   sim_time(4) slot_entities(2*N) player_states(BYTES*N)
 *   wave_number(2) wave_active(1) total_kills(2)
 *   zombies_alive(2) between_waves(1) wave_cooldown_centis(2) count(1)  */
#define NET_SNAP_HEADER_BYTES                                        \
    (4 + 2 * NET_MAX_PLAYERS + NET_PLAYER_STATE_BYTES * NET_MAX_PLAYERS \
     + 2 + 1 + 2 + 2 + 1 + 2 + 1)
#define NET_SNAP_MAX_BYTES \
    (NET_HDR_SIZE + NET_SNAP_HEADER_BYTES + \
     NET_SNAP_MAX_ENTITIES * NET_SNAP_ENTRY_BYTES)

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
    NET_REJECT_OTHER = 4,
    NET_REJECT_ASSET = 5      /* NET_ASSET_VERSION mismatch (N2) */
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

/* Human-readable refusal text for the menu (sprint N2). Names both versions and
 * the remedy; see the implementation for why the short name is not enough. */
void net_version_conflict_message(char *buf, int cap, int reason,
                                  uint32_t theirs, uint32_t ours);
const char *net_leave_reason_name(int reason);

/* Initialize ENet globally (idempotent). Returns 0 or -1. Called automatically
 * by net_server_host()/net_client_init(); safe to call early too. */
int net_init(void);

#define NET_VERSION_STR(MINOR) #MINOR
#define NET_WIRE_VER_STR "3"

#endif