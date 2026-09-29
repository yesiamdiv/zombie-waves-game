#include "net_client.h"
#include "net_codec.h"
#include "events/event_bus.h"
#include "core/log.h"
#include <ctype.h>
#include <string.h>

int net_parse_host_port(const char *addr, char *ip, int ip_cap, uint16_t *port) {
    if (!addr || !*addr) return -1;
    const char *colon = strrchr(addr, ':');
    if (colon) {
        size_t ip_len = (size_t)(colon - addr);
        if (ip_len == 0 || ip_len >= (size_t)ip_cap) return -1;
        memcpy(ip, addr, ip_len);
        ip[ip_len] = '\0';
        long p = strtol(colon + 1, NULL, 10);
        if (p <= 0 || p > 65535) return -1;
        *port = (uint16_t)p;
    } else {
        if ((int)strlen(addr) >= ip_cap) return -1;
        snprintf(ip, ip_cap, "%s", addr);
        *port = NET_DEFAULT_PORT;
    }
    return 0;
}

static void send_ctrl(ENetPeer *peer, const uint8_t *buf, int len) {
    ENetPacket *pk = enet_packet_create(buf, (size_t)len, ENET_PACKET_FLAG_RELIABLE);
    if (!pk) return;
    if (enet_peer_send(peer, NET_CH_CTRL, pk) != 0) {
        enet_packet_destroy(pk);
    }
}

int net_client_init(NetClient *c) {
    if (!c) return -1;
    memset(c, 0, sizeof(*c));
    c->state = NET_CLIENT_OFFLINE;
    c->slot = 255;
    c->world_gen = 0;
    snprintf(c->status, sizeof(c->status), "offline");
    if (net_init() != 0) {
        LOG_ERROR("net_client: enet_initialize failed");
        return -1;
    }
    return 0;
}

static void client_set_offline(NetClient *c) {
    if (c->host) {
        enet_host_destroy(c->host);
        c->host = NULL;
    }
    c->server = NULL;
    c->state = NET_CLIENT_OFFLINE;
}

/* Like set_offline but preserve NET_CLIENT_REJECTED so the UI can explain
 * WHY we were refused after the transport finishes disconnecting us. */
static void client_keep_rejected(NetClient *c) {
    if (c->host) {
        enet_host_destroy(c->host);
        c->host = NULL;
    }
    c->server = NULL;
}

void net_client_shutdown(NetClient *c) {
    if (!c) return;
    if (c->server && c->state != NET_CLIENT_OFFLINE) {
        enet_peer_disconnect_now(c->server, 0);
    }
    client_set_offline(c);
}

int net_client_connect(NetClient *c, const char *ip, uint16_t port,
                       const char *name) {
    if (!c) return -1;
    if (c->state != NET_CLIENT_OFFLINE) return -1;

    ENetHost *host = enet_host_create(NULL, 1, 2, 0, 0);
    if (!host) {
        LOG_ERROR("net_client: enet_host_create failed");
        return -1;
    }

    ENetAddress addr;
    if (enet_address_set_host(&addr, ip) != 0) {
        LOG_ERROR("net_client: bad host address '%s'", ip);
        enet_host_destroy(host);
        return -1;
    }
    addr.port = port;

    ENetPeer *server = enet_host_connect(host, &addr, 2, 0);
    if (!server) {
        LOG_ERROR("net_client: enet_host_connect to %s:%u failed", ip,
                  (unsigned)port);
        enet_host_destroy(host);
        return -1;
    }

    c->host = host;
    c->server = server;
    c->state = NET_CLIENT_CONNECTING;
    snprintf(c->name, sizeof(c->name), "%s", name ? name : "Player");
    snprintf(c->host_addr, sizeof(c->host_addr), "%s:%u", ip, (unsigned)port);
    snprintf(c->status, sizeof(c->status), "connecting to %s...", c->host_addr);
    /* No JOIN yet - wait for the transport CONNECT event first. */
    c->seq = 0;
    LOG_INFO("NET: client connecting to %s as '%s'", c->host_addr, c->name);
    return 0;
}

int net_client_rtt_ms(const NetClient *c) {
    if (!c || !c->server || c->state == NET_CLIENT_CONNECTING) return -1;
    return (int)c->server->roundTripTime;
}

int net_client_send_input(NetClient *c, const NetInput *in) {
    if (!c || !c->server || c->state != NET_CLIENT_CONNECTED) return -1;
    NetHeader h = {NET_WIRE_VERSION, NET_PKT_INPUT, c->seq++, c->slot, 0};
    uint8_t buf[NET_HDR_SIZE + 3 + 8];
    int len = net_encode_input(buf, (int)sizeof(buf), &h, in);
    if (len <= 0) return -1;
    ENetPacket *pk = enet_packet_create(buf, (size_t)len, 0);
    if (!pk) return -1;
    return enet_peer_send(c->server, NET_CH_SNAP, pk);
}

void net_client_update(NetClient *c) {
    if (!c || !c->host || c->state == NET_CLIENT_OFFLINE) return;

    ENetEvent ev;
    /* Re-check the host each iteration: the DISCONNECT handler tears it down
     * mid-loop, and the transport must not be serviced with a NULL host. */
    while (c->host && c->state != NET_CLIENT_OFFLINE &&
           enet_host_service(c->host, &ev, 0) > 0) {
        switch (ev.type) {
            case ENET_EVENT_TYPE_CONNECT: {
                c->state = NET_CLIENT_CONNECTING;
                uint8_t ver = c->wire_version_override
                                  ? c->wire_version_override
                                  : NET_WIRE_VERSION;
                NetHeader h = {ver, NET_PKT_JOIN, c->seq++, 0, 0};
                uint8_t buf[NET_HDR_SIZE + 1 + NET_NAME_MAX];
                int len = net_encode_join(buf, (int)sizeof(buf), &h, c->name);
                if (len > 0) {
                    send_ctrl(c->server, buf, len);
                    LOG_INFO("NET: JOIN sent (wire=%u name='%s')", ver, c->name);
                }
                snprintf(c->status, sizeof(c->status), "joined %s",
                         c->host_addr);
                break;
            }

            case ENET_EVENT_TYPE_RECEIVE: {
                if (ev.channelID == NET_CH_SNAP) {
                    /* Channel 1 game traffic: snapshots from the host. */
                    NetHeader h;
                    if ((int)ev.packet->dataLength < NET_HDR_SIZE ||
                        net_hdr_decode(&h, ev.packet->data) != 0) {
                        LOG_WARN("NET: undecodable ch1 packet from host");
                        enet_packet_destroy(ev.packet);
                        break;
                    }
                    if (h.kind == NET_PKT_SNAPSHOT) {
                        NetSnapshot snap;
                        if (net_decode_snapshot(ev.packet->data,
                                                (int)ev.packet->dataLength, &h,
                                                &snap,
                                                NET_SNAP_MAX_ENTITIES) == 0) {
                            c->snap = snap;
                            c->snap_valid = true;
                            c->snap_seq = h.seq;
                            LOG_DEBUG("NET: snapshot seq=%u count=%d sim=%.2f",
                                      h.seq, snap.count, snap.sim_time);
                        } else {
                            LOG_WARN("NET: malformed snapshot from host");
                        }
                    }
                    enet_packet_destroy(ev.packet);
                    break;
                }
                if (ev.channelID != NET_CH_CTRL) {
                    enet_packet_destroy(ev.packet);
                    break;
                }
                NetHeader h;
                if ((int)ev.packet->dataLength < NET_HDR_SIZE ||
                    net_hdr_decode(&h, ev.packet->data) != 0) {
                    LOG_WARN("NET: undecodable packet from host");
                    enet_packet_destroy(ev.packet);
                    break;
                }

                if (h.kind == NET_PKT_HELLO) {
                    uint8_t slot;
                    uint32_t seed, world;
                    char host_name[NET_NAME_CAP];
                    if (net_decode_hello(ev.packet->data,
                                         (int)ev.packet->dataLength, &h, &slot,
                                         &seed, &world, host_name,
                                         NET_NAME_CAP) != 0) {
                        LOG_WARN("NET: malformed HELLO from host");
                        enet_packet_destroy(ev.packet);
                        break;
                    }
                    if (world != NET_WORLD_GEN_VERSION) {
                        c->flags = NET_FLAG_WORLD_MISMATCH;
                        c->reject_reason = NET_REJECT_WORLD;
                        c->state = NET_CLIENT_REJECTED;
                        LOG_ERROR("NET: world-gen mismatch (host=%u client=%u)",
                                  world, NET_WORLD_GEN_VERSION);
                        enet_peer_disconnect(c->server, 0);
                    } else {
                        c->slot = slot;
                        c->seed = seed;
                        c->world_gen = world;
                        snprintf(c->host_name, sizeof(c->host_name), "%s",
                                 host_name);
                        c->state = NET_CLIENT_CONNECTED;
                        LOG_INFO("NET: hello from '%s' -> slot %u (seed=%u world_gen=%u)",
                                 host_name, slot, seed, world);
                        snprintf(c->status, sizeof(c->status),
                                 "in lobby of '%s'", host_name);
                    }
                } else if (h.kind == NET_PKT_REJECT) {
                    uint8_t reason;
                    if (net_decode_reject(ev.packet->data,
                                          (int)ev.packet->dataLength, &h,
                                          &reason) != 0) {
                        reason = NET_REJECT_OTHER;
                    }
                    c->reject_reason = reason;
                    c->flags = h.flags;
                    c->state = NET_CLIENT_REJECTED;
                    LOG_WARN("NET: rejected by host: %s",
                             net_reject_reason_name(reason));
                    enet_peer_disconnect(c->server, 0);
                } else if (h.kind == NET_PKT_PLAYER_LIST) {
                    NetPlayerInfo roster[NET_MAX_PLAYERS];
                    int n = 0;
                    if (net_decode_player_list(ev.packet->data,
                                               (int)ev.packet->dataLength, &h,
                                               roster, NET_MAX_PLAYERS, &n) == 0) {
                        memset(c->roster, 0, sizeof(c->roster));
                        memcpy(c->roster, roster, (size_t)n * sizeof(*roster));
                        c->roster_count = n;
                        LOG_INFO("NET: player list (%d in lobby)", n);
                        for (int i = 0; i < n; i++) {
                            LOG_INFO("NET:   [%u] '%s' (%s)", c->roster[i].slot,
                                     c->roster[i].name,
                                     c->roster[i].slot == c->slot ? "you" : "");
                        }
                    }
                } else if (h.kind == NET_PKT_EVENTS) {
                    NetRelayedEvent events[NET_EVENTS_MAX_BATCH];
                    int n = 0;
                    if (net_decode_events(ev.packet->data,
                                          (int)ev.packet->dataLength, &h,
                                          events, NET_EVENTS_MAX_BATCH, &n) == 0) {
                        for (int i = 0; i < n; i++) {
                            const NetRelayedEvent *e = &events[i];
                            /* R13-C3 fix: the client is a render-only mirror, so
                             * without this it would never write any gameplay event
                             * to its own log and a playtest run could not evidence
                             * wave/kill progression. Re-emit the host's authoritative
                             * events into the local bus verbatim (same type/kind/
                             * entity/payload) so the client log is a faithful
                             * record of the session. */
                            event_emit(g_events, (GameEventType)e->type, e->id,
                                       (EventKind)e->kind, e->x, e->y, e->a, e->b,
                                       0, 0);
                            if ((e->type == GE_ENTITY_DEATH ||
                                 e->type == GE_KILL) && e->id != 0 &&
                                c->dead_count < NET_EVENTS_MAX_BATCH) {
                                c->dead_ids[c->dead_count++] = e->id;
                            } else if (e->type == GE_WAVE_START) {
                                c->pending_wave = (int)e->a;
                                c->pending_wave_count = (int)e->b;
                                c->has_pending_wave = true;
                                LOG_INFO("NET: wave %d starting - %d zombies",
                                         c->pending_wave,
                                         c->pending_wave_count);
                            } else if (e->type == GE_HOST_PAUSE) {
                                c->host_paused = (e->a != 0.0f);
                                LOG_INFO("NET: host %s the session",
                                         c->host_paused ? "PAUSED" : "resumed");
                            }
                        }
                    } else {
                        LOG_WARN("NET: malformed events batch from host");
                    }
                } else if (h.kind == NET_PKT_LEAVE) {
                    uint8_t reason;
                    net_decode_leave(ev.packet->data,
                                     (int)ev.packet->dataLength, &h, &reason);
                    c->server_stopped = true;
                    LOG_INFO("NET: host left the session (%s)",
                             net_leave_reason_name(reason));
                } else {
                    LOG_WARN("NET: unexpected kind=%s from host",
                             net_pkt_kind_name(h.kind));
                }
                enet_packet_destroy(ev.packet);
                break;
            }

            case ENET_EVENT_TYPE_DISCONNECT: {
                if (c->state == NET_CLIENT_REJECTED) {
                    LOG_WARN("NET: host refused connection: %s",
                             net_reject_reason_name(c->reject_reason));
                    client_keep_rejected(c);
                } else if (c->state == NET_CLIENT_CONNECTED) {
                    c->server_stopped = true;
                    LOG_WARN("NET: connection to host lost");
                    client_set_offline(c);
                } else {
                    LOG_WARN("NET: connection attempt to host failed");
                    client_set_offline(c);
                }
                break;  /* host was torn down; don't re-enter enet_host_service */
            }

            default:
                break;
        }
    }
}