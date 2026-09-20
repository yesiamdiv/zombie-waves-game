#ifndef NET_CLIENT_H
#define NET_CLIENT_H

#include "net.h"
#include <enet/enet.h>

typedef enum {
    NET_CLIENT_OFFLINE = 0,
    NET_CLIENT_CONNECTING,
    NET_CLIENT_CONNECTED,
    NET_CLIENT_REJECTED   /* server refused us; `reject_reason` explains */
} NetClientState;

/* Render-only client. Drives only the handshake in P1; P2 adds snapshot
 * reception (channel 1) and net-input transmission. Identifies itself with a
 * name; the host assigns a slot + sends seed/world-gen in the HELLO. */
typedef struct {
    NetClientState state;
    ENetHost *host;
    ENetPeer *server;
    char name[NET_NAME_CAP];
    char host_name[NET_NAME_CAP];
    char host_addr[64];          /* "ip:port" for lobby overlay/logs */
    uint8_t slot;                /* slot assigned by the host */
    uint32_t seed;               /* world seed to adopt (P3 sim parity) */
    uint32_t world_gen;
    uint8_t reject_reason;       /* when state == REJECTED */
    uint8_t flags;               /* rejection flags (version/world mismatch) */

    NetPlayerInfo roster[NET_MAX_PLAYERS];
    int roster_count;

    /* Latest snapshot (P2). `snap_valid` becomes true after the first
     * snapshot; interpolation (`snap_frame`) lives at the mirror layer. */
    NetSnapshot snap;
    bool snap_valid;
    uint32_t snap_seq;
    uint32_t last_mirror_seq;    /* last snapshot drained into the mirror */

    /* Relayed GE_* events (P2, ch0). Deaths tidy the mirror ahead of the next
     * snapshot; wave starts feed the client HUD. Cleared after each drain. */
    uint16_t dead_ids[NET_EVENTS_MAX_BATCH];
    int dead_count;
    int pending_wave;
    int pending_wave_count;
    bool has_pending_wave;

    uint16_t seq;
    bool left;                   /* intentional leave (LEAVE sent) */
    bool server_stopped;         /* host disconnected / left */
    /* Test seam: advertise a different NET_WIRE_VERSION in the JOIN so a stale
     * client build can be exercised (0 = use the real wire version). */
    uint8_t wire_version_override;
    char status[96];
} NetClient;

/* Parse "ip:port" (port optional, defaults NET_DEFAULT_PORT). Returns 0/-1. */
int net_parse_host_port(const char *addr, char *ip, int ip_cap, uint16_t *port);

int net_client_init(NetClient *c);

/* Start connecting to `ip:port`, presenting `name`. State -> CONNECTING. */
int net_client_connect(NetClient *c, const char *ip, uint16_t port,
                       const char *name);
void net_client_shutdown(NetClient *c);

/* Pump ENet events once per frame: JOIN on connect, HELLO/REJECT/PLAYER_LIST
 * handling. Call in CONNECTING and CONNECTED states. */
void net_client_update(NetClient *c);

/* Round trip to the host in ms, or -1 when not connected. */
int net_client_rtt_ms(const NetClient *c);

/* Send one input sample to the host (channel 1, unreliable latest-wins).
 * Returns 0/-1 (ignored while not connected to a host). */
int net_client_send_input(NetClient *c, const NetInput *in);

#endif