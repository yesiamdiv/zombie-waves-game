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
    /* R13 merge: map the host is playing, learned from the HELLO. The client
     * must load exactly this map, otherwise the two windows disagree about
     * terrain, spawn point and theme. Valid only once CONNECTED. */
    uint8_t map_index;
    bool map_index_valid;
    uint8_t reject_reason;       /* when state == REJECTED */
    uint8_t flags;               /* rejection flags (version/world/asset) */
    /* N2: the two numbers behind a refusal, so the menu can say "host has 5,
     * you have 4" instead of the useless "version mismatch". 0xFFFFFFFF means
     * the peer did not send the value (pre-N2 build). */
    uint32_t reject_theirs;
    uint32_t reject_ours;

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

    /* Co-op shop (B30). The client may only *ask*: `shop_pending` throttles
     * one in-flight request per keypress and `shop_item` remembers what was
     * asked so a late reply can still be labelled. The outcome lands in
     * `shop_result`/`shop_result_item`, read and cleared by the shop UI. */
    uint8_t shop_item;
    uint8_t shop_result_item;
    uint8_t shop_result;
    bool shop_pending;
    bool shop_has_result;

    uint16_t seq;
    bool left;                   /* intentional leave (LEAVE sent) */
    bool server_stopped;         /* host disconnected / left */
    /* R13-I4: authoritative host paused the session. The mirror stops advancing
     * because the simulation really is stopped, so the client shows this instead
     * of freezing silently. */
    bool host_paused;
    /* Test seams: advertise a different version in the JOIN so a mismatched
     * peer can be exercised (0 = use the real version). */
    uint8_t wire_version_override;
    uint32_t asset_version_override;   /* N2 */
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
/* Co-op shop (B30). Ask the host to apply a purchase to *this client's* slot.
 * Reliable and ordered, unlike the per-frame input, because dropping a
 * purchase silently would look like a broken button. Returns 0 on send, -1 if
 * not connected or a request is already in flight. */
int net_client_send_shop_request(NetClient *c, uint8_t item);

/* Host's answer to the last request: NET_SHOP_RES_* in `*result`, and the item
 * it was for in `*item`. Returns 0 when one is waiting (and clears it), -1
 * when nothing has arrived since the last call. */
int net_client_take_shop_result(NetClient *c, uint8_t *item, uint8_t *result);

/* Number of purchase requests still unanswered. */
int net_client_shop_requests_pending(const NetClient *c);

int net_client_rtt_ms(const NetClient *c);

/* Send one input sample to the host (channel 1, unreliable latest-wins).
 * Returns 0/-1 (ignored while not connected to a host). */
int net_client_send_input(NetClient *c, const NetInput *in);

#endif