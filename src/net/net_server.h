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

    bool slot_used[NET_MAX_PLAYERS];                 /* host slot 0 is "used" */
    char slot_names[NET_MAX_PLAYERS][NET_NAME_CAP];  /* display names */
    ENetPeer *slot_peers[NET_MAX_PLAYERS];           /* NULL for local slot */

    uint64_t joins;
    uint64_t rejects;
    uint64_t bad_packets;
    char status[96];
} NetServer;

/* Initialize ENet globally (idempotent). Returns 0 or -1. */
int net_init(void);

/* `net_server_init` then start listening on `port`. Slot 0 becomes the host's
 * local player named `host_name`. Returns 0 or -1 (bind failure). */
int net_server_host(NetServer *s, uint16_t port, const char *host_name,
                    uint32_t seed, uint32_t world_gen);
void net_server_shutdown(NetServer *s);

/* Pump ENet events once per frame (connection accept, handshake, roster
 * broadcasts). Call both in the lobby and during play. */
void net_server_update(NetServer *s);

/* Number of occupied slots (local host included). */
int net_server_player_count(const NetServer *s);

/* Snap the current roster into `out`. Returns the count written. */
int net_server_build_player_list(const NetServer *s, NetPlayerInfo *out, int max);

#endif