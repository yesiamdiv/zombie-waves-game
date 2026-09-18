/* ============================================================================
 * zombie_net_spike - ENet loopback connectivity + packet codec proof
 *
 * Standalone networking spike (no game code linked). Validates the transport
 * assumptions behind docs/MULTIPLAYER_PLAN.md BEFORE the real net layer is
 * written:
 *
 *   - ENet host listens on one server port (default 50910, 127.0.0.1).
 *   - N client hosts are spun up, each bound to its OWN local UDP port
 *     (default base 52001 -> 52001..52003) so we can see three separate
 *     independent connections/instances on one machine.
 *   - Channel 0 = reliable/ordered control events (server->client HELLO).
 *   - Channel 1 = unreliable/sequenced game traffic (client input up,
 *     server snapshot echo down), matching the plan.
 *   - Every packet is a byte-packed GamePacket (16 bytes) with a self
 *     checksum; the codec encodes/decodes and the receiver verifies
 *     checksum + payload equality after the round trip (server echoes the
 *     client's input back inside a snapshot; the client recomputes the
 *     expected payload from the echoed sequence number and must match).
 *
 * Everything is logged so connection/loss/decode behavior is visible.
 * Returns 0 when all clients connected and every round trip verified.
 *
 * Usage: zombie_net_spike [server_ip] [server_port] [seconds] [num_clients]
 * ========================================================================== */

#include <enet/enet.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_CLIENTS 4

/* Client local ports are allocated contiguously from this base; the server
 * identifies a peer by its source socket port (it never trusts client-supplied
 * identity - same rule as the real listener). */
#define CLIENT_BASE_PORT 52001

/* Wire channels (mirrors the multiplayer plan). */
#define CH_CTRL 0 /* reliable / ordered handshake + events */
#define CH_SNAP 1 /* unreliable / sequenced input + snapshots */

#define WIRE_VERSION 1
enum {
    PKT_INPUT = 1,   /* client -> server: gameplay input */
    PKT_SNAPSHOT = 2,/* server -> client: echo snapshot */
    PKT_HELLO = 3    /* server -> client: reliable handshake */
};

/* -------------------------------------------------------------------------
 * Wire format (byte layout, offsets fixed):
 *   [0] version  [1] kind  [2] seq  [3] client_id
 *   [4..8) x (f32)   [8..12) y (f32)   [12..14) hp (u16)
 *   [14] weapon  [15] checksum (sum of bytes 0..14, low 8 bits)
 * 16 bytes total, matched by the planned snapshot sizing (~30B/entity there).
 * ----------------------------------------------------------------------- */
#define WIRE_SIZE 16

typedef struct {
    uint8_t version;
    uint8_t kind;
    uint8_t seq;
    uint8_t client_id;
    float x, y;
    uint16_t hp;
    uint8_t weapon;
    uint8_t checksum; /* decode-only; computed, not transmitted */
} GamePacket;

static void gpk_encode(uint8_t out[WIRE_SIZE], const GamePacket *p) {
    memset(out, 0, WIRE_SIZE);
    out[0] = p->version;
    out[1] = p->kind;
    out[2] = p->seq;
    out[3] = p->client_id;
    memcpy(out + 4, &p->x, sizeof(p->x));
    memcpy(out + 8, &p->y, sizeof(p->y));
    memcpy(out + 12, &p->hp, sizeof(p->hp));
    out[14] = p->weapon;
    uint8_t sum = 0;
    for (int i = 0; i < WIRE_SIZE - 1; i++) sum = (uint8_t)(sum + out[i]);
    out[15] = sum;
}

/* Returns 0 on success, -1 on bad version/checksum/size. */
static int gpk_decode(GamePacket *p, const uint8_t in[WIRE_SIZE]) {
    p->version = in[0];
    p->kind = in[1];
    p->seq = in[2];
    p->client_id = in[3];
    memcpy(&p->x, in + 4, sizeof(p->x));
    memcpy(&p->y, in + 8, sizeof(p->y));
    memcpy(&p->hp, in + 12, sizeof(p->hp));
    p->weapon = in[14];
    p->checksum = in[15];
    uint8_t sum = 0;
    for (int i = 0; i < WIRE_SIZE - 1; i++) sum = (uint8_t)(sum + in[i]);
    if (p->version != WIRE_VERSION || sum != p->checksum) return -1;
    return 0;
}

/* Deterministic input payload derived from the frame sequence number, so a
 * receiver can independently recompute the expected values for verification. */
static void gpk_fill_input(GamePacket *p, uint8_t client_id, uint8_t seq) {
    p->version = WIRE_VERSION;
    p->kind = PKT_INPUT;
    p->seq = seq;
    p->client_id = client_id;
    p->x = 100.0f + (float)seq * 0.5f;
    p->y = 200.0f - (float)seq * 0.25f;
    p->hp = (uint16_t)(100 - (seq % 50));
    p->weapon = seq % 4;
}

/* Verify an echoed snapshot payload against what `seq` implies. */
static bool gpk_echo_matches(const GamePacket *p) {
    float ex = 100.0f + (float)p->seq * 0.5f;
    float ey = 200.0f - (float)p->seq * 0.25f;
    uint16_t ehp = (uint16_t)(100 - (p->seq % 50));
    uint8_t ew = p->seq % 4;
    return p->x == ex && p->y == ey && p->hp == ehp && p->weapon == ew;
}

/* ----------------------------------------------------------------------- */

static double now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1e6;
}

static void peer_addr_str(const ENetPeer *p, char *out, size_t n) {
    char ip[16] = "?";
    if (p) enet_address_get_host_ip((ENetAddress *)&p->address, ip, sizeof(ip));
    snprintf(out, n, "%s:%u", ip, p ? p->address.port : 0);
}

/* Per-peer traffic/loss bookkeeping. seq is uint8 (wrapping). */
typedef struct {
    int have_seq;
    uint8_t last_seq;
    uint64_t rx;
    uint64_t lost;
    uint64_t decode_errors;
} PeerStats;

static void peer_stats_observe(PeerStats *st, uint8_t seq) {
    st->rx++;
    if (st->have_seq) {
        int gap = (int)seq - (int)st->last_seq;
        if (gap <= 0) gap += 256;
        st->lost += (uint64_t)(gap - 1);
    }
    st->last_seq = seq;
    st->have_seq = 1;
}

/* ----------------------------------------------------------------------- */

typedef struct {
    int id;                 /* 1..N */
    enet_uint16 bind_port;  /* this instance's OWN local UDP port */
    ENetHost *host;
    ENetPeer *server;
    bool connected;
    bool hello_ok;
    uint64_t tx;
    PeerStats rx;           /* snapshot channel stats (incl. hello on ch0 not counted here) */
    uint64_t hello_rx;
    uint64_t echo_mismatch;
} Client;

typedef struct {
    ENetHost *host;
    enet_uint16 port;
    uint64_t connects;
    uint64_t disconnects;
    uint64_t tx_snapshots;
    PeerStats peers[MAX_CLIENTS + 1]; /* indexed by client id */
} Host;

static int g_failed = 0;

#define LOGP(fmt, ...) \
    printf(fmt "\n", ##__VA_ARGS__)

/* ----------------------------------------------------------------------- */

static int peer_id_for(const ENetAddress *a) {
    if (!a) return 0;
    int id = (int)a->port - CLIENT_BASE_PORT + 1;
    if (id < 1 || id > MAX_CLIENTS) return 0;
    return id;
}

static void host_service(Host *sv) {
    ENetEvent ev;
    while (enet_host_service(sv->host, &ev, 0) > 0) {
        switch (ev.type) {
            case ENET_EVENT_TYPE_CONNECT: {
                int id = peer_id_for(&ev.peer->address);
                char addr[24];
                peer_addr_str(ev.peer, addr, sizeof(addr));
                if (id >= 1 && id <= MAX_CLIENTS) {
                    ev.peer->data = (void *)(intptr_t)id;
                    sv->connects++;
                    LOGP("[SERVER] CONNECTED client %d from %s (channels ok) -> queued HELLO",
                         id, addr);
                    /* Reliable handshake on the reliable channel. */
                    GamePacket hello = {
                        .version = WIRE_VERSION, .kind = PKT_HELLO,
                        .seq = 0, .client_id = (uint8_t)id,
                        .x = 0, .y = 0, .hp = 0, .weapon = 0
                    };
                    uint8_t buf[WIRE_SIZE];
                    gpk_encode(buf, &hello);
                    ENetPacket *pk = enet_packet_create(buf, WIRE_SIZE,
                                                        ENET_PACKET_FLAG_RELIABLE);
                    enet_peer_send(ev.peer, CH_CTRL, pk);
                    enet_host_flush(sv->host);
                } else {
                    LOGP("[SERVER] CONNECT from unknown id %d (%s)', disconnecting", id, addr);
                    enet_peer_disconnect(ev.peer, 0);
                }
                break;
            }
            case ENET_EVENT_TYPE_DISCONNECT:
                sv->disconnects++;
                LOGP("[SERVER] DISCONNECT peer id=%d", (int)(intptr_t)ev.peer->data);
                break;
            case ENET_EVENT_TYPE_RECEIVE: {
                int id = (int)(intptr_t)ev.peer->data;
                GamePacket p;
                PeerStats *st = NULL;
                if (id >= 1 && id <= MAX_CLIENTS) st = &sv->peers[id];
                if (ev.packet->dataLength != WIRE_SIZE ||
                    gpk_decode(&p, ev.packet->data) != 0) {
                    sv->peers[0].decode_errors++;
                    LOGP("[SERVER] BAD PACKET from id=%d (len=%zu)", id, ev.packet->dataLength);
                    break;
                }
                if (p.kind == PKT_INPUT) {
                    if (st) peer_stats_observe(st, p.seq);
                    LOGP("[SERVER] RX INPUT id=%d seq=%u pos=(%.1f,%.1f) hp=%u weapon=%u "
                         "-> echo SNAPSHOT",
                         id, p.seq, p.x, p.y, p.hp, p.weapon);
                    GamePacket snap = {
                        .version = WIRE_VERSION, .kind = PKT_SNAPSHOT,
                        .seq = p.seq, .client_id = (uint8_t)id,
                        .x = p.x, .y = p.y, .hp = p.hp, .weapon = p.weapon
                    };
                    uint8_t buf[WIRE_SIZE];
                    gpk_encode(buf, &snap);
                    ENetPacket *pk = enet_packet_create(buf, WIRE_SIZE, 0 /* unreliable, sequenced */);
                    enet_peer_send(ev.peer, CH_SNAP, pk);
                    enet_host_flush(sv->host);
                    sv->tx_snapshots++;
                } else {
                    LOGP("[SERVER] unexpected kind=%u on ch1 from id=%d", p.kind, id);
                }
                break;
            }
            default:
                break;
        }
    }
}

static void client_service(Client *c) {
    ENetEvent ev;
    while (enet_host_service(c->host, &ev, 0) > 0) {
        switch (ev.type) {
            case ENET_EVENT_TYPE_CONNECT:
                c->connected = true;
                LOGP("[CLIENT-%d] connected to server (local port %u)", c->id, c->bind_port);
                break;
            case ENET_EVENT_TYPE_DISCONNECT:
                c->connected = false;
                LOGP("[CLIENT-%d] server disconnected us", c->id);
                break;
            case ENET_EVENT_TYPE_RECEIVE: {
                GamePacket p;
                if (ev.packet->dataLength != WIRE_SIZE || gpk_decode(&p, ev.packet->data) != 0) {
                    c->rx.decode_errors++;
                    LOGP("[CLIENT-%d] BAD PACKET (len=%zu)", c->id, ev.packet->dataLength);
                    break;
                }
                if (p.kind == PKT_HELLO) {
                    c->hello_rx++;
                    c->hello_ok = (p.client_id == (uint8_t)c->id);
                    LOGP("[CLIENT-%d] HELLO from server (assigned id=%u) %s",
                         c->id, p.client_id, c->hello_ok ? "-> verified" : "-> MISMATCH");
                } else if (p.kind == PKT_SNAPSHOT) {
                    peer_stats_observe(&c->rx, p.seq);
                    bool ok = (p.client_id == (uint8_t)c->id) && gpk_echo_matches(&p);
                    if (!ok) c->echo_mismatch++;
                    LOGP("[CLIENT-%d] RX SNAPSHOT echo seq=%u pos=(%.1f,%.1f) hp=%u weapon=%u "
                         "-> %s",
                         c->id, p.seq, p.x, p.y, p.hp, p.weapon, ok ? "verified" : "MISMATCH");
                }
                break;
            }
            default:
                break;
        }
    }
}

int main(int argc, char *argv[]) {
    const char *server_ip = argc > 1 ? argv[1] : "127.0.0.1";
    int server_port = argc > 2 ? atoi(argv[2]) : 50910;
    double seconds = argc > 3 ? atof(argv[3]) : 1.5;
    int nclients = argc > 4 ? atoi(argv[4]) : 3;
    if (nclients < 1) nclients = 1;
    if (nclients > MAX_CLIENTS) nclients = MAX_CLIENTS;
    if (server_port <= 0) server_port = 50910;

    if (enet_initialize() != 0) {
        LOGP("FATAL: enet_initialize failed");
        return 1;
    }

    printf("==============================================================\n");
    printf("  ENet loopback spike: 1 server + %d clients on distinct ports\n", nclients);
    printf("==============================================================\n");

    /* --- server host ---------------------------------------------------- */
    Host sv = {0};
    {
        ENetAddress addr;
        addr.host = ENET_HOST_ANY;
        addr.port = (enet_uint16)server_port;
        sv.host = enet_host_create(&addr, 8, 2, 0, 0);
        if (!sv.host) {
            LOGP("FATAL: server enet_host_create failed (port %d)", server_port);
            enet_deinitialize();
            return 1;
        }
        sv.port = (enet_uint16)server_port;
        LOGP("[SERVER] listening on %s:%d (channels: 0=reliable ctrl, 1=unreliable snap)",
             server_ip, server_port);
    }

    /* --- client hosts, each bound to its own local UDP port ------------ */
    Client clients[MAX_CLIENTS];
    memset(clients, 0, sizeof(clients));
    for (int i = 0; i < nclients; i++) {
        Client *c = &clients[i];
        c->id = i + 1;
        c->bind_port = (enet_uint16)(CLIENT_BASE_PORT + i);

        ENetAddress bind;
        bind.host = ENET_HOST_ANY;
        bind.port = c->bind_port;
        c->host = enet_host_create(&bind, 1, 2, 0, 0);
        if (!c->host) {
            LOGP("FATAL: client %d enet_host_create failed (port %u)", c->id, c->bind_port);
            return 1;
        }

        ENetAddress saddr;
        if (enet_address_set_host(&saddr, server_ip)) {
            LOGP("FATAL: bad server ip '%s'", server_ip);
            return 1;
        }
        saddr.port = (enet_uint16)server_port;
        c->server = enet_host_connect(c->host, &saddr, 2, (enet_uint32)c->id);
        if (!c->server) {
            LOGP("FATAL: client %d connect failed", c->id);
            return 1;
        }
        LOGP("[CLIENT-%d] created on LOCAL port %u -> connecting to %s:%d (id=%d)",
             c->id, c->bind_port, server_ip, server_port, c->id);
    }

    /* --- exchange phase -------------------------------------------------- */
    double start = now_ms();
    double deadline = start + seconds * 1000.0;
    uint64_t tick = 0;
    while (now_ms() < deadline) {
        host_service(&sv);
        for (int i = 0; i < nclients; i++) client_service(&clients[i]);

        /* Each tick every connected client fires one input packet. */
        for (int i = 0; i < nclients; i++) {
            Client *c = &clients[i];
            if (!c->connected || !c->server) continue;
            GamePacket p;
            gpk_fill_input(&p, (uint8_t)c->id, (uint8_t)(tick & 0xFF));
            uint8_t buf[WIRE_SIZE];
            gpk_encode(buf, &p);
            ENetPacket *pk = enet_packet_create(buf, WIRE_SIZE, 0 /* unreliable, sequenced */);
            if (enet_peer_send(c->server, CH_SNAP, pk) == 0) {
                c->tx++;
                LOGP("[CLIENT-%d] TX INPUT seq=%u pos=(%.1f,%.1f) hp=%u weapon=%u",
                     c->id, (uint8_t)(tick & 0xFF), p.x, p.y, p.hp, p.weapon);
            }
            enet_host_flush(c->host);
        }
        tick++;
        /* ~1kHz pacing; keeps the loop from pegging a core. */
        {
            struct timespec ts = {0, 1000000L};
            nanosleep(&ts, NULL);
        }
    }
    printf("\n----------------------- summary -----------------------------\n");

    bool pass = true;

    /* Server-side tallies. */
    LOGP("[SERVER] connects=%llu disconnects=%llu snapshots_tx=%llu",
         (unsigned long long)sv.connects, (unsigned long long)sv.disconnects,
         (unsigned long long)sv.tx_snapshots);
    if (sv.connects != (uint64_t)nclients) {
        LOGP("[SERVER] FAIL: expected %d connects, saw %llu", nclients,
             (unsigned long long)sv.connects);
        pass = false;
    }
    if (sv.peers[0].decode_errors > 0) pass = false;

    /* Per-client tallies. */
    for (int i = 0; i < nclients; i++) {
        Client *c = &clients[i];
        PeerStats *st = &sv.peers[c->id];
        LOGP("[CLIENT-%d] local_port=%u connected=%d hello_ok=%d tx=%llu "
             "snap_rx=%llu lost=%llu decode_err=%llu echo_mismatch=%llu",
             c->id, c->bind_port, c->connected, c->hello_ok,
             (unsigned long long)c->tx, (unsigned long long)c->rx.rx,
             (unsigned long long)c->rx.lost,
             (unsigned long long)c->rx.decode_errors,
             (unsigned long long)c->echo_mismatch);
        LOGP("[SERVER]   peer stats for id=%d: rx=%llu lost=%llu decode_err=%llu",
             c->id, (unsigned long long)st->rx, (unsigned long long)st->lost,
             (unsigned long long)st->decode_errors);

        if (!c->connected || !c->hello_ok) pass = false;
        if (c->rx.rx == 0) {
            LOGP("[CLIENT-%d] FAIL: no snapshots returned", c->id);
            pass = false;
        }
        if (c->rx.decode_errors || c->echo_mismatch) {
            pass = false;
        }
        if (c->rx.lost > 0) {
            /* On loopback we expect no loss; flag if the sequenced channel slipped. */
            LOGP("[CLIENT-%d] note: %llu gap(s) seen on channel 1 (loopback expected 0)",
                 c->id, (unsigned long long)c->rx.lost);
        }
    }

    /* --- clean disconnect ------------------------------------------------- */
    for (int i = 0; i < nclients; i++) {
        if (clients[i].server && clients[i].connected) {
            enet_peer_disconnect(clients[i].server, 0);
        }
    }
    double dis_deadline = now_ms() + 500.0;
    while (now_ms() < dis_deadline) {
        host_service(&sv);
        for (int i = 0; i < nclients; i++) client_service(&clients[i]);
        nanosleep(&(struct timespec){0, 1000000L}, NULL);
    }
    for (int i = 0; i < nclients; i++) {
        if (clients[i].host) enet_host_destroy(clients[i].host);
    }
    enet_host_destroy(sv.host);
    enet_deinitialize();

    printf("--------------------------------------------------------------\n");
    LOGP(pass ? "RESULT: PASS  (all %d clients connected, codec round-trip verified, no loss)"
              : "RESULT: FAIL  (see logs above)",
         nclients);
    printf("--------------------------------------------------------------\n");
    return pass ? 0 : 1;
}