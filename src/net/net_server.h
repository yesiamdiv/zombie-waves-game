#ifndef NET_SERVER_H
#define NET_SERVER_H

#include "net.h"
#include <enet/enet.h>

typedef enum {
    NET_SERVER_OFFLINE = 0,
    NET_SERVER_HOSTING
} NetServerState;

/* Listen-server (in-process host). Slot 0 is always the host's local player;
 * remote peers occupy slots 1..NET_MAX_PLAYERS-1 in join order. The host plays
 * with zero network latency; remote input arrives over channel 0 then flows
 * through the normal AI-injection path (P2). */
typedef struct {
    NetServerState state;
    ENetHost *host;
    uint16_t port;
    char host_name[NET_NAME_CAP];
    uint32_t seed;
    uint32_t world_gen;
    /* R13 merge: map the host selected, advertised in the HELLO so every client
     * builds the same world. Defaults to 0 (Grassland) when the host never
     * opened the picker (e.g. --auto-start). */
    uint8_t map_index;

    bool slot_used[NET_MAX_PLAYERS];                 /* host slot 0 is "used" */
    char slot_names[NET_MAX_PLAYERS][NET_NAME_CAP];  /* display names */
    ENetPeer *slot_peers[NET_MAX_PLAYERS];           /* NULL for local slot */

    /* Latest input received from each remote slot (P2). The host applies it
     * to the slot's InputState every sim tick; `input_valid` tracks whether a
     * slot has ever voiced in. Slot 0 (local) is never written here. */
    NetInput inputs[NET_MAX_PLAYERS];
    bool input_valid[NET_MAX_PLAYERS];

    /* Pending co-op shop purchases, drained by the host once per frame. The
     * net layer deliberately does not touch PlayerInventory -- it only
     * collects validated requests, exactly as it collects inputs. One slot
     * deep per roster slot is enough: the client re-sends on failure and a
     * stale repeat would cost the player money for something they already
     * have, so a newer request overwrites an unconsumed older one. */
    uint8_t shop_item[NET_MAX_PLAYERS];
    bool shop_valid[NET_MAX_PLAYERS];

    uint64_t joins;
    uint64_t rejects;
    uint64_t bad_packets;
    uint64_t rx_inputs;
    uint64_t shop_requests;
    uint64_t snaps_sent;
    uint64_t events_sent;
    char status[96];
} NetServer;

/* Initialize ENet globally (idempotent). Returns 0 or -1. */
int net_init(void);

/* `net_server_init` then start listening on `port`. Slot 0 becomes the host's
 * local player named `host_name`. Returns 0 or -1 (bind failure). */
int net_server_host(NetServer *s, uint16_t port, const char *host_name,
                    uint32_t seed, uint32_t world_gen, uint8_t map_index);
void net_server_shutdown(NetServer *s);

/* Pump ENet events once per frame (connection accept, handshake, roster
 * broadcasts). Call both in the lobby and during play. */
void net_server_update(NetServer *s);

/* Take the oldest unconsumed purchase request for `slot` (or -1). Host side,
 * once per frame, before the sim tick. `slot` 0 is the local host and never has
 * a pending request -- the host's own purchases are applied directly. */
int net_server_take_shop_request(NetServer *s, int slot, uint8_t *item_out);

/* Send the outcome of a purchase the host just resolved. `result` is a
 * NET_SHOP_RES_* code. Reliable, so it cannot be lost the way an unreliable
 * packet would be. */
void net_server_send_shop_result(NetServer *s, int slot, uint8_t item, uint8_t result);

/* Number of occupied slots (local host included). */
int net_server_player_count(const NetServer *s);

/* Snap the current roster into `out`. Returns the count written. */
int net_server_build_player_list(const NetServer *s, NetPlayerInfo *out, int max);

/* Latest input heard from a remote slot. Returns false when the slot is the
 * local host slot (slot 0) or has never voiced in. */
bool net_server_get_input(const NetServer *s, int slot, NetInput *out);

/* Encode and send a 20 Hz snapshot to every remote peer (unreliable ch1). */
void net_server_broadcast_snapshot(NetServer *s, const NetSnapshot *snap);

/* Encode and send a relayed GE_* event batch to every remote peer (ch0,
 * reliable/ordered - deaths/waves must not be lost). */
void net_server_broadcast_events(NetServer *s, const NetRelayedEvent *events,
                                 int count);

#endif