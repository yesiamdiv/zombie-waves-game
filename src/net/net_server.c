#include "net_server.h"
#include "net_codec.h"
#include "core/log.h"
#include <string.h>

/* Idempotent global ENet init (the spike did this at startup too). */
int net_init(void) {
    static bool init = false;
    if (init) return 0;
    if (enet_initialize() != 0) return -1;
    init = true;
    return 0;
}

static void send_ctrl(ENetPeer *peer, const uint8_t *buf, int len) {
    ENetPacket *pk = enet_packet_create(buf, (size_t)len, ENET_PACKET_FLAG_RELIABLE);
    if (!pk) return;
    if (enet_peer_send(peer, NET_CH_CTRL, pk) != 0) {
        enet_packet_destroy(pk);
    }
}

static int peer_slot(const ENetPeer *p) {
    return (int)(intptr_t)p->data;
}

static int next_free_slot(NetServer *s) {
    for (int i = 1; i < NET_MAX_PLAYERS; i++) {
        if (!s->slot_used[i]) return i;
    }
    return -1;
}

/* Send a coded REJECT and then end the connection. enet_peer_disconnect()
 * resets the peer queues (peer.c) - which on loopback races the incoming
 * reliable REJECT and can drop it before the client dispatches it. Using
 * disconnect_later keeps the transport up until the REJECT is ACKed, so the
 * client always sees the coded reason before the disconnect event. */
static void reject_and_drop(NetServer *s, ENetPeer *peer, uint8_t reason,
                            uint8_t flags) {
    NetHeader h = {NET_WIRE_VERSION, NET_PKT_REJECT, 0, 0, flags};
    /* N2: name the host's own number for the concern being refused, so the
     * player's menu can print both sides instead of "version mismatch". */
    uint32_t host_version;
    switch (reason) {
        case NET_REJECT_WORLD: host_version = s->world_gen; break;
        case NET_REJECT_ASSET: host_version = NET_ASSET_VERSION; break;
        default:               host_version = NET_WIRE_VERSION; break;
    }
    uint8_t buf[NET_HDR_SIZE + 1 + 4];
    int len = net_encode_reject(buf, (int)sizeof(buf), &h, reason, host_version);
    if (len > 0) send_ctrl(peer, buf, len);
    enet_peer_disconnect_later(peer, 0);
    s->rejects++;
    /* N2: the HOST is refused-peer feedback too, and it used to be silent for
     * every reason without its own LOG_WARN (e.g. a full server). Logged here
     * rather than at each call site so no refusal path can be forgotten again.
     * Console-level only: the host has no menu on screen while hosting. */
    LOG_WARN("NET: refused peer port=%u reason=%s host=%u",
             (unsigned)peer->address.port, net_reject_reason_name(reason),
             (unsigned)host_version);
}

int net_server_host(NetServer *s, uint16_t port, const char *host_name,
                    uint32_t seed, uint32_t world_gen, uint8_t map_index) {
    if (!s) return -1;
    memset(s, 0, sizeof(*s));
    s->map_index = map_index;
    if (net_init() != 0) {
        LOG_ERROR("net_server: enet_initialize failed");
        return -1;
    }

    ENetAddress addr;
    addr.host = ENET_HOST_ANY;
    addr.port = port;
    s->host = enet_host_create(&addr, NET_MAX_PLAYERS, 2, 0, 0);
    if (!s->host) {
        LOG_ERROR("net_server: enet_host_create failed on port %u", port);
        return -1;
    }

    s->port = port;
    s->seed = seed;
    s->world_gen = world_gen;
    snprintf(s->host_name, sizeof(s->host_name), "%s",
             host_name ? host_name : "Host");
    s->slot_used[0] = true;
    snprintf(s->slot_names[0], sizeof(s->slot_names[0]), "%s", s->host_name);
    s->state = NET_SERVER_HOSTING;
    snprintf(s->status, sizeof(s->status), "hosting on :%u", (unsigned)port);
    LOG_INFO("NET: hosting on port %u (seed=%u world_gen=%u) as '%s'",
             (unsigned)port, seed, world_gen, s->host_name);
    return 0;
}

void net_server_shutdown(NetServer *s) {
    if (!s) return;
    if (s->host) {
        enet_host_destroy(s->host);
        s->host = NULL;
    }
    memset(s->slot_used, 0, sizeof(s->slot_used));
    memset(s->slot_peers, 0, sizeof(s->slot_peers));
    s->state = NET_SERVER_OFFLINE;
}

int net_server_take_shop_request(NetServer *s, int slot, uint8_t *item_out) {
    if (!s || slot < 1 || slot >= NET_MAX_PLAYERS) return -1;
    if (!s->shop_valid[slot]) return -1;
    s->shop_valid[slot] = false;
    if (item_out) *item_out = s->shop_item[slot];
    return 0;
}

void net_server_send_shop_result(NetServer *s, int slot, uint8_t item, uint8_t result) {
    if (!s || slot < 1 || slot >= NET_MAX_PLAYERS) return;
    if (!s->slot_used[slot] || !s->slot_peers[slot]) return;
    uint8_t buf[NET_HDR_SIZE + NET_SHOP_RESULT_BYTES];
    NetHeader h = {NET_WIRE_VERSION, NET_PKT_SHOP_RESULT, 0, (uint8_t)slot, 0};
    int len = net_encode_shop_result(buf, (int)sizeof(buf), &h, item, result);
    if (len > 0) send_ctrl(s->slot_peers[slot], buf, len);
}

int net_server_player_count(const NetServer *s) {
    if (!s) return 0;
    int n = 0;
    for (int i = 0; i < NET_MAX_PLAYERS; i++) if (s->slot_used[i]) n++;
    return n;
}

int net_server_build_player_list(const NetServer *s, NetPlayerInfo *out, int max) {
    if (!s || !out || max < 0) return 0;
    int n = 0;
    for (int i = 0; i < NET_MAX_PLAYERS && n < max; i++) {
        if (!s->slot_used[i]) continue;
        out[n].slot = (uint8_t)i;
        out[n].color = (uint8_t)i;
        snprintf(out[n].name, sizeof(out[n].name), "%s", s->slot_names[i]);
        n++;
    }
    return n;
}

bool net_server_get_input(const NetServer *s, int slot, NetInput *out) {
    if (!s || slot < 1 || slot >= NET_MAX_PLAYERS) return false;
    if (!s->input_valid[slot]) return false;
    if (out) *out = s->inputs[slot];
    return true;
}

void net_server_broadcast_snapshot(NetServer *s, const NetSnapshot *snap) {
    if (!s || s->state != NET_SERVER_HOSTING || !snap) return;
    uint8_t buf[NET_SNAP_MAX_BYTES];
    NetHeader h = {NET_WIRE_VERSION, NET_PKT_SNAPSHOT, (uint16_t)s->snaps_sent, 0, 0};
    int len = net_encode_snapshot(buf, (int)sizeof(buf), &h, snap);
    if (len <= 0) return;
    for (int i = 1; i < NET_MAX_PLAYERS; i++) {
        if (s->slot_peers[i]) {
            ENetPacket *pk = enet_packet_create(buf, (size_t)len, 0);
            if (pk && enet_peer_send(s->slot_peers[i], NET_CH_SNAP, pk) != 0) {
                enet_packet_destroy(pk);
            }
        }
    }
    s->snaps_sent++;
}

void net_server_broadcast_events(NetServer *s, const NetRelayedEvent *events,
                                 int count) {
    if (!s || s->state != NET_SERVER_HOSTING || !events || count <= 0) return;
    uint8_t buf[NET_EVENTS_MAX_BYTES];
    NetHeader h = {NET_WIRE_VERSION, NET_PKT_EVENTS, (uint16_t)s->events_sent,
                   0, 0};
    int len = net_encode_events(buf, (int)sizeof(buf), &h, events, count);
    if (len <= 0) return;
    for (int i = 1; i < NET_MAX_PLAYERS; i++) {
        if (s->slot_peers[i]) {
            ENetPacket *pk = enet_packet_create(buf, (size_t)len,
                                                ENET_PACKET_FLAG_RELIABLE);
            if (pk && enet_peer_send(s->slot_peers[i], NET_CH_CTRL, pk) != 0) {
                enet_packet_destroy(pk);
            }
        }
    }
    s->events_sent++;
}

static void broadcast_player_list(NetServer *s) {
    NetPlayerInfo roster[NET_MAX_PLAYERS];
    int n = net_server_build_player_list(s, roster, NET_MAX_PLAYERS);
    uint8_t buf[NET_HDR_SIZE + 1 + NET_MAX_PLAYERS * (2 + NET_NAME_MAX)];
    int len = net_encode_player_list(buf, (int)sizeof(buf), roster, n);
    if (len < 0) return;
    for (int i = 1; i < NET_MAX_PLAYERS; i++) {
        if (s->slot_peers[i]) send_ctrl(s->slot_peers[i], buf, len);
    }
    LOG_INFO("NET: broadcast player list (%d players)", n);
    for (int i = 0; i < n; i++) {
        LOG_INFO("NET:   slot %u = '%s'%s", roster[i].slot, roster[i].name,
                 s->slot_used[roster[i].slot] && s->slot_peers[roster[i].slot]
                     ? " (remote)" : " (local)");
    }
}

static void free_slot(NetServer *s, int slot) {
    if (slot < 1 || slot >= NET_MAX_PLAYERS) return;
    if (s->slot_used[slot]) {
        LOG_INFO("NET: slot %d ('%s') freed", slot, s->slot_names[slot]);
    }
    s->slot_used[slot] = false;
    s->slot_names[slot][0] = '\0';
    s->slot_peers[slot] = NULL;
}

void net_server_update(NetServer *s) {
    if (!s || s->state != NET_SERVER_HOSTING || !s->host) return;

    ENetEvent ev;
    while (enet_host_service(s->host, &ev, 0) > 0) {
        switch (ev.type) {
            case ENET_EVENT_TYPE_CONNECT: {
                ev.peer->data = (void *)(intptr_t)0;
                if (next_free_slot(s) < 0 && net_server_player_count(s) >= NET_MAX_PLAYERS) {
                    LOG_INFO("NET: incoming connection rejected: lobby full");
                    reject_and_drop(s, ev.peer, NET_REJECT_FULL, 0);
                } else {
                    LOG_INFO("NET: connection from %u.%u.%u.%u:%u (joining)",
                             (ev.peer->address.host >> 24) & 0xFF,
                             (ev.peer->address.host >> 16) & 0xFF,
                             (ev.peer->address.host >> 8) & 0xFF,
                             ev.peer->address.host & 0xFF,
                             (unsigned)ev.peer->address.port);
                }
                break;
            }

            case ENET_EVENT_TYPE_RECEIVE: {
                if (ev.channelID == NET_CH_SNAP) {
                    /* Game traffic (channel 1): the only client->host kind in
                     * P2 is INPUT - keep the latest per slot, latest-wins. */
                    NetHeader h;
                    NetInput in;
                    if ((int)ev.packet->dataLength < NET_HDR_SIZE ||
                        net_hdr_decode(&h, ev.packet->data) != 0 ||
                        h.kind != NET_PKT_INPUT ||
                        net_decode_input(ev.packet->data,
                                         (int)ev.packet->dataLength,
                                         &h, &in) != 0) {
                        s->bad_packets++;
                        enet_packet_destroy(ev.packet);
                        break;
                    }
                    int slot = peer_slot(ev.peer);
                    if (slot >= 1 && slot < NET_MAX_PLAYERS) {
                        s->inputs[slot] = in;
                        s->input_valid[slot] = true;
                        s->rx_inputs++;
                    } else {
                        s->bad_packets++;
                    }
                    enet_packet_destroy(ev.packet);
                    break;
                }
                if (ev.channelID != NET_CH_CTRL) {
                    s->bad_packets++;
                    enet_packet_destroy(ev.packet);
                    break;
                }
                NetHeader h;
                if ((int)ev.packet->dataLength < NET_HDR_SIZE ||
                    net_hdr_decode(&h, ev.packet->data) != 0) {
                    s->bad_packets++;
                    LOG_WARN("NET: undecodable packet from peer slot=%d",
                             peer_slot(ev.peer));
                    enet_packet_destroy(ev.packet);
                    break;
                }

                if (next_free_slot(s) < 0 && net_server_player_count(s) >= NET_MAX_PLAYERS &&
                    h.kind == NET_PKT_JOIN) {
                    /* Someone connected while full and is trying to join. */
                    LOG_INFO("NET: JOIN on full lobby rejected");
                    reject_and_drop(s, ev.peer, NET_REJECT_FULL, 0);
                    enet_packet_destroy(ev.packet);
                    break;
                }

                if (h.version != NET_WIRE_VERSION) {
                    /* Version mismatch: refuse with a coded reason. */
                    LOG_WARN("NET: version mismatch from peer (wire=%u expected=%u)",
                             h.version, NET_WIRE_VERSION);
                    reject_and_drop(s, ev.peer, NET_REJECT_VERSION,
                                    NET_FLAG_VERSION_MISMATCH);
                    enet_packet_destroy(ev.packet);
                    break;
                }

                if (h.kind == NET_PKT_SHOP_REQUEST) {
                    /* Co-op purchase (B30). The slot is resolved from the
                     * host's own peer->data record, never from the body's
                     * from_slot -- otherwise one client could spend another
                     * player's points. Placed after the version check so a
                     * pre-v8 peer cannot reach it at all. */
                    uint8_t item = NET_SHOP_NONE;
                    int rslot = peer_slot(ev.peer);
                    /* Decoders return the number of bytes consumed, so a
                     * failure is < 0 -- not != 0. */
                    int dr = net_decode_shop_request(ev.packet->data,
                                                     (int)ev.packet->dataLength,
                                                     &h, &item);
                    if (dr < 0 ||
                        rslot < 1 || rslot >= NET_MAX_PLAYERS || !s->slot_used[rslot]) {
                        s->bad_packets++;
                        LOG_WARN("NET: bad shop request from slot=%d (dec=%d)",
                                 rslot, dr);
                        enet_packet_destroy(ev.packet);
                        break;
                    }
                    s->shop_item[rslot] = item;
                    s->shop_valid[rslot] = true;
                    s->shop_requests++;
                    LOG_DEBUG("NET: shop request slot=%d item=%u", rslot, item);
                    enet_packet_destroy(ev.packet);
                    break;
                }

                if (h.kind == NET_PKT_JOIN) {
                    char name[NET_NAME_CAP];
                    uint32_t peer_assets = 0;
                    NetHeader jh;
                    if (net_decode_join(ev.packet->data, (int)ev.packet->dataLength,
                                        &jh, name, NET_NAME_CAP,
                                        &peer_assets) != 0) {
                        s->bad_packets++;
                        enet_packet_destroy(ev.packet);
                        break;
                    }
                    /* N2: refuse art drift before allocating a slot, so the
                     * peer gets a real reason and the host roster never briefly
                     * contains someone who is about to be dropped. */
                    if (peer_assets != NET_ASSET_VERSION) {
                        LOG_WARN("NET: asset mismatch from peer (assets=%u expected=%u)",
                                 peer_assets, NET_ASSET_VERSION);
                        reject_and_drop(s, ev.peer, NET_REJECT_ASSET,
                                        NET_FLAG_ASSET_MISMATCH);
                        enet_packet_destroy(ev.packet);
                        break;
                    }
                    int slot = next_free_slot(s);
                    if (slot < 0) {
                        reject_and_drop(s, ev.peer, NET_REJECT_FULL, 0);
                        enet_packet_destroy(ev.packet);
                        break;
                    }
                    s->slot_used[slot] = true;
                    snprintf(s->slot_names[slot], sizeof(s->slot_names[slot]),
                             "%s", name);
                    ev.peer->data = (void *)(intptr_t)slot;
                    s->slot_peers[slot] = ev.peer;
                    s->joins++;

                    NetHeader oh = {NET_WIRE_VERSION, NET_PKT_HELLO, 0, 0, 0};
                    uint8_t buf[NET_HDR_SIZE + 4 + 1 + 4 + 4 + 1 + 4 + NET_NAME_MAX];
                    int len = net_encode_hello(buf, (int)sizeof(buf), &oh,
                                               (uint8_t)slot, s->seed,
                                               s->world_gen, s->map_index,
                                               NET_ASSET_VERSION, s->host_name);
                    if (len > 0) send_ctrl(ev.peer, buf, len);
                    LOG_INFO("NET: '%s' joined -> slot %d (seed=%u world_gen=%u map=%u)",
                             name, slot, s->seed, s->world_gen,
                             (unsigned)s->map_index);
                    broadcast_player_list(s);
                } else if (h.kind == NET_PKT_LEAVE) {
                    LOG_INFO("NET: peer slot=%d is leaving", peer_slot(ev.peer));
                    free_slot(s, peer_slot(ev.peer));
                    broadcast_player_list(s);
                } else {
                    s->bad_packets++;
                    LOG_WARN("NET: unexpected kind=%s from slot=%d",
                             net_pkt_kind_name(h.kind), peer_slot(ev.peer));
                }
                enet_packet_destroy(ev.packet);
                break;
            }

            case ENET_EVENT_TYPE_DISCONNECT: {
                int slot = peer_slot(ev.peer);
                LOG_INFO("NET: peer disconnected (slot=%d)", slot);
                if (slot >= 1) {
                    free_slot(s, slot);
                    broadcast_player_list(s);
                }
                break;
            }

            default:
                break;
        }
    }
}