/* ============================================================================
 * net_test - loopback handshake integration test for NetServer/NetClient
 *
 * Runs the REAL networking skeleton in a single process over 127.0.0.1:
 *   - host starts on a fixed port,
 *   - N clients connect, JOIN, and receive a HELLO (assigned slot + seed),
 *   - the host broadcasts PLAYER_LIST and every client converges on the roster,
 *   - a version-mismatch client is refused with a coded REJECT,
 *   - a client that disconnects is removed from the host roster.
 *
 * Mirrors P1's exit criteria ("two processes join over loopback; names/ping
 * shown; version mismatch handled") as a deterministic ctest.
 *
 * Returns 0 on PASS.
 * ========================================================================== */

#include <enet/enet.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "net/net.h"
#include "net/net_server.h"
#include "net/net_client.h"
#include "net/net_mirror.h"
#include "ecs/ecs.h"
#include "players.h"
#include "weapons/weapons.h"
#include "net/net_mirror.h"
#include "events/event_bus.h"
#include "core/log.h"

#define TEST_PORT 51240
#define TEST_SEED 2026u

static int g_failed = 0;
#define CHECK(expr)                                                      \
    do {                                                                 \
        if (expr) {                                                      \
            LOG_INFO("PASS: %s", #expr);                                 \
        } else {                                                         \
            g_failed++;                                                  \
            LOG_ERROR("FAIL: %s (%s:%d)", #expr, __FILE__, __LINE__);    \
        }                                                                \
    } while (0)

static double now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1e6;
}

static void service(NetServer *server, NetClient *clients, int nclients) {
    net_server_update(server);
    for (int i = 0; i < nclients; i++) net_client_update(&clients[i]);
    nanosleep(&(struct timespec){0, 500000L}, NULL); /* pace the handshake */
}

static bool all_connected(NetClient *clients, int nclients) {
    for (int i = 0; i < nclients; i++) {
        if (clients[i].state != NET_CLIENT_CONNECTED) return false;
    }
    return true;
}

int main(void) {
    log_init(NULL, LOG_INFO, LOG_TRACE);

    /* ---------------------------------------------------------- server --- */
    NetServer server;
    if (net_server_host(&server, TEST_PORT, "Host-1", TEST_SEED,
                        NET_WORLD_GEN_VERSION, 0) != 0) {
        LOG_ERROR("FAIL: could not start host on :%d", TEST_PORT);
        log_shutdown();
        return 1;
    }
    CHECK(net_server_player_count(&server) == 1); /* host local slot 0 */

    /* ------------------------------------------- Alice + Bob join first --- */
    NetClient c[3];
    for (int i = 0; i < 3; i++) {
        if (net_client_init(&c[i]) != 0) g_failed++;
    }
    char names[3][NET_NAME_CAP] = {"Alice", "Bob", "Cara"};
    if (net_client_connect(&c[0], "127.0.0.1", TEST_PORT, names[0]) != 0 ||
        net_client_connect(&c[1], "127.0.0.1", TEST_PORT, names[1]) != 0) {
        g_failed++;
        LOG_ERROR("FAIL: initial connects failed");
    }
    double deadline = now_ms() + 2000.0;
    while (!all_connected(c, 2) && now_ms() < deadline) {
        service(&server, c, 2);
    }
    for (int i = 0; i < 2; i++) {
        CHECK(c[i].state == NET_CLIENT_CONNECTED);
        if (c[i].state == NET_CLIENT_CONNECTED) {
            CHECK(c[i].slot == (uint8_t)(i + 1));   /* join order: slots 1..2 */
            CHECK(c[i].seed == TEST_SEED);
            CHECK(c[i].world_gen == NET_WORLD_GEN_VERSION);
            CHECK(strcmp(c[i].host_name, "Host-1") == 0);
            CHECK(c[i].roster_count == 3);          /* host + Alice + Bob */
            int have_self = 0, have_host = 0;
            for (int r = 0; r < c[i].roster_count; r++) {
                if (c[i].roster[r].slot == c[i].slot) have_self++;
                if (c[i].roster[r].slot == 0) have_host++;
            }
            CHECK(have_self == 1 && have_host == 1);
        }
    }

    /* -------------------------- version mismatch (slot 3 still free) ------ */
    NetClient bad;
    net_client_init(&bad);
    if (net_client_connect(&bad, "127.0.0.1", TEST_PORT, "OldBuild") == 0) {
        bad.wire_version_override = NET_WIRE_VERSION - 1;
        for (int tries = 0; tries < 800 && bad.state == NET_CLIENT_CONNECTING;
             tries++) {
            service(&server, &bad, 1);
        }
        for (int tries = 0; tries < 400 &&
                          bad.state != NET_CLIENT_REJECTED &&
                          bad.state != NET_CLIENT_OFFLINE;
             tries++) {
            service(&server, &bad, 1);
        }
    }
    CHECK(server.rejects >= 1);
    CHECK(bad.state == NET_CLIENT_REJECTED);
    if (bad.state == NET_CLIENT_REJECTED) {
        CHECK(bad.reject_reason == NET_REJECT_VERSION);
        CHECK(bad.flags == NET_FLAG_VERSION_MISMATCH);
    }
    net_client_shutdown(&bad);
    CHECK(net_server_player_count(&server) == 3); /* bad never took a slot */

    /* --------------------- asset mismatch (N2): same protocol, other art ---
     * The case that used to be invisible: the session "works" and the two
     * windows simply look different. The host must refuse with its own reason
     * code, and the client must be able to name both numbers. */
    NetClient oldart;
    net_client_init(&oldart);
    if (net_client_connect(&oldart, "127.0.0.1", TEST_PORT, "OldArt") == 0) {
        oldart.asset_version_override = NET_ASSET_VERSION + 1;
        for (int tries = 0; tries < 800 && oldart.state == NET_CLIENT_CONNECTING;
             tries++) {
            service(&server, &oldart, 1);
        }
        for (int tries = 0; tries < 400 &&
                          oldart.state != NET_CLIENT_REJECTED &&
                          oldart.state != NET_CLIENT_OFFLINE;
             tries++) {
            service(&server, &oldart, 1);
        }
    }
    CHECK(oldart.state == NET_CLIENT_REJECTED);
    if (oldart.state == NET_CLIENT_REJECTED) {
        CHECK(oldart.reject_reason == NET_REJECT_ASSET);
        CHECK(oldart.flags == NET_FLAG_ASSET_MISMATCH);
        /* Both numbers must reach the menu, or the player is told nothing. */
        CHECK(oldart.reject_theirs == NET_ASSET_VERSION);
        CHECK(oldart.reject_ours == NET_ASSET_VERSION + 1);
    }
    net_client_shutdown(&oldart);
    CHECK(net_server_player_count(&server) == 3); /* never took a slot */

    /* ---------------------------------------------- Cara joins last ------ */
    if (net_client_connect(&c[2], "127.0.0.1", TEST_PORT, names[2]) != 0) {
        g_failed++;
        LOG_ERROR("FAIL: Cara connect failed");
    }
    deadline = now_ms() + 2000.0;
    while (!all_connected(c, 3) && now_ms() < deadline) {
        service(&server, c, 3);
    }
    for (int i = 0; i < 3; i++) {
        CHECK(c[i].state == NET_CLIENT_CONNECTED);
        if (c[i].state == NET_CLIENT_CONNECTED) {
            CHECK(c[i].slot == (uint8_t)(i + 1));   /* slots 1..3, join order */
            CHECK(c[i].roster_count == 4);          /* host + 3 clients */
        }
    }
    CHECK(net_server_player_count(&server) == 4);
    CHECK(server.joins == 3);

    NetPlayerInfo host_roster[NET_MAX_PLAYERS];
    int n = net_server_build_player_list(&server, host_roster, NET_MAX_PLAYERS);
    CHECK(n == 4);
    CHECK(strcmp(host_roster[0].name, "Host-1") == 0);
    CHECK(strcmp(host_roster[1].name, "Alice") == 0 ||
          strcmp(host_roster[2].name, "Alice") == 0 ||
          strcmp(host_roster[3].name, "Alice") == 0);

    /* ------------------------------ disconnect cleans up the roster ------- */
    int before = net_server_player_count(&server);
    net_client_shutdown(&c[2]);
    for (int i = 0; i < 2; i++) {
        service(&server, c, 2);
    }
    CHECK(net_server_player_count(&server) == before - 1);
    for (int i = 0; i < 2; i++) {
        CHECK(c[i].roster_count == 3); /* 'Cara' dropped out of everyone's list */
    }

    /* ------------------- channel 1 game traffic: INPUT up, latest-wins ----- */
    NetInput in = {NET_INPUT_MOVE_UP | NET_INPUT_MOVE_RIGHT,
                   NET_INPUT_BTN_SHOOT, 2, 123.5f, -4.25f};
    for (int i = 0; i < 2; i++) {
        if (c[i].state == NET_CLIENT_CONNECTED) {
            CHECK(net_client_send_input(&c[i], &in) == 0);
        }
    }
    for (int i = 0; i < 2; i++) {
        service(&server, c, 2);
    }
    CHECK(server.rx_inputs >= 2);
    NetInput got;
    CHECK(net_server_get_input(&server, 1, &got));
    CHECK(got.move_flags == in.move_flags);
    CHECK(got.buttons == in.buttons);
    CHECK(got.weapon == in.weapon);
    CHECK(got.aim_x == in.aim_x);
    CHECK(got.aim_y == in.aim_y);
    CHECK(net_server_get_input(&server, 2, &got));
    CHECK(got.move_flags == in.move_flags);
    /* Slot 0 (the local host) never has remote input. */
    CHECK(!net_server_get_input(&server, 0, &got));

    /* ------------------- channel 1 game traffic: SNAPSHOT down ------------- */
    NetSnapshot snap = {0};
    snap.sim_time = 6.25f;
    snap.wave_number = 3;
    snap.wave_active = 1;
    snap.slot_entities[0] = 101;
    snap.slot_entities[1] = 202;
    snap.count = 2;
    snap.entities[0] = (NetEntitySnap){101, NET_ENT_PLAYER, {10, 20}, {0, 0}, 100.0f, 0, 0};
    snap.entities[1] = (NetEntitySnap){303, NET_ENT_ZOMBIE, {300, 400}, {50, 0}, 25.0f, 0, 0};
    net_server_broadcast_snapshot(&server, &snap);
    for (int i = 0; i < 2; i++) {
        service(&server, c, 2);
    }
    CHECK(server.snaps_sent == 1);
    for (int i = 0; i < 2; i++) {
        if (c[i].state != NET_CLIENT_CONNECTED) continue;
        CHECK(c[i].snap_valid);
        CHECK(c[i].snap_seq == 0);
        CHECK(c[i].snap.count == 2);
        CHECK(c[i].snap.sim_time == 6.25f);
        CHECK(c[i].snap.wave_number == 3);
        CHECK(c[i].snap.slot_entities[1] == 202);
        CHECK(c[i].snap.entities[0].id == 101);
        CHECK(c[i].snap.entities[0].kind == NET_ENT_PLAYER);
        CHECK(c[i].snap.entities[0].pos.x == 10.0f);
        CHECK(c[i].snap.entities[1].id == 303);
        CHECK(c[i].snap.entities[1].kind == NET_ENT_ZOMBIE);
        CHECK(c[i].snap.entities[1].hp == 25.0f);
    }

    /* ------------------ channel 0 relay: EVENTS down (reliable) ------------ */
    NetRelayedEvent ev[2] = {
        {GE_ENTITY_DEATH, GEK_ZOMBIE, 303, 300.0f, 400.0f, 0.0f, 0.0f},
        {GE_WAVE_START, GEK_NONE, 0, 0.0f, 0.0f, 4.0f, 12.0f},
    };
    net_server_broadcast_events(&server, ev, 2);
    for (int i = 0; i < 2; i++) {
        service(&server, c, 2);
    }
    CHECK(server.events_sent == 1);
    for (int i = 0; i < 2; i++) {
        if (c[i].state != NET_CLIENT_CONNECTED) continue;
        CHECK(c[i].dead_count == 1);
        CHECK(c[i].dead_ids[0] == 303);
        CHECK(c[i].has_pending_wave);
        CHECK(c[i].pending_wave == 4 && c[i].pending_wave_count == 12);
        /* Draining removes the zombie from the client's mirror ahead of the
         * next snapshot: entity 303 must vanish while 101 survives. */
        NetMirror mm;
        net_mirror_reset(&mm);
        uint16_t dead[1] = {c[i].dead_ids[0]};
        net_mirror_push_removing(&mm, &c[i].snap, dead, c[i].dead_count);
        NetEntitySnap se;
        CHECK(!net_mirror_sample(&mm, 303, 0.5f, &se));
        CHECK(net_mirror_sample(&mm, 101, 0.5f, &se));
        c[i].dead_count = 0;
        c[i].has_pending_wave = false;
    }

    /* --------------- authoritative HUD state reaches the client (N3) ------
     * The regression this guards: the client HUD used to read HP and inventory
     * from the CLIENT'S OWN ECS, which a render-only client never simulates.
     * So the client displayed join-time values forever while the host moved on.
     * Drive a real host snapshot with distinctive numbers and require every
     * connected client to see exactly those, for its OWN slot. */
    {
        /* waves == NULL: net_snapshot_build tolerates it and this test only
         * cares about per-slot player state, not wave counters. */
        World ecs;
        Player hplayers[MAX_PLAYERS];
        NetSnapshot snap;

        ecs_init(&ecs);
        players_reset(hplayers, MAX_PLAYERS);
        CHECK(player_respawn(hplayers, &ecs, 0, "Host-1", &COLOR_RED,
                             vec2(10, 10)) == 0);
        for (int slot = 1; slot < NET_MAX_PLAYERS; slot++) {  /* 0 done above */
            if (!server.slot_used[slot]) continue;
            CHECK(player_respawn(hplayers, &ecs, slot, "P", &COLOR_BLUE,
                                 vec2(10.0f * (slot + 1), 10.0f)) == slot);
        }
        /* Distinguishable per slot so a client cannot pass by reading
         * somebody else's state. */
        for (int i = 0; i < MAX_PLAYERS; i++) {
            hplayers[i].inventory.points = 1000 + i;
            hplayers[i].inventory.grenades = 3 + i;
            hplayers[i].inventory.launcher_ammo = 7 + i;
            hplayers[i].beacon_pos = vec2(11.0f * (i + 1), 77.0f * (i + 1));
        }
        /* (Wave state is set after the build below: net_snapshot_build starts
         * from a zeroed struct, and it was handed waves == NULL.) */
        CHECK(net_snapshot_build(&ecs, hplayers, MAX_PLAYERS, 1.0f, NULL,
                                 &snap) >= 0);
        /* Wave state the client HUD needs (B26). Set after the build because
         * net_snapshot_build zeroes its output and got waves == NULL. */
        snap.wave_number = 5;
        snap.wave_active = 1;
        snap.zombies_alive = 9;
        snap.total_kills = 21;
        net_server_broadcast_snapshot(&server, &snap);

        /* Only clients still connected at this point in the test: earlier
         * sections deliberately disconnect one. */
        int live = 0;
        for (int i = 0; i < 3; i++) {
            if (c[i].state == NET_CLIENT_CONNECTED) live++;
        }
        CHECK(live >= 2);
        deadline = now_ms() + 2000.0;
        while (now_ms() < deadline) {
            net_server_update(&server);
            for (int i = 0; i < 3; i++) net_client_update(&c[i]);
            bool all_have = true;
            for (int i = 0; i < 3; i++) {
                if (c[i].state != NET_CLIENT_CONNECTED) continue;
                if (!c[i].snap_valid) all_have = false;
            }
            if (all_have) break;
            nanosleep(&(struct timespec){0, 500000L}, NULL);
        }
        for (int i = 0; i < 3; i++) {
            if (c[i].state != NET_CLIENT_CONNECTED) continue;
            int slot = c[i].slot;
            CHECK(c[i].snap_valid);
            if (!c[i].snap_valid) continue;
            CHECK(c[i].snap.player_states[slot].points == 1000 + slot);
            CHECK(c[i].snap.player_states[slot].grenades == 3 + slot);
            CHECK(c[i].snap.player_states[slot].launcher_ammo == 7 + slot);
            /* Wave panel (B26) and beacons (B27) must reach the client too. */
            CHECK(c[i].snap.wave_number == 5);
            CHECK(c[i].snap.wave_active == 1);
            CHECK(c[i].snap.zombies_alive == 9);
            CHECK(c[i].snap.total_kills == 21);
            CHECK(c[i].snap.player_states[slot].beacon_x ==
                  11.0f * (slot + 1));
            CHECK(c[i].snap.player_states[slot].beacon_y == 77.0f * (slot + 1));
            /* And through the mirror, which is what the HUD actually reads. */
            NetMirror m;
            net_mirror_reset(&m);
            net_mirror_push(&m, &c[i].snap);
            NetPlayerState hs;
            CHECK(net_mirror_player_state(&m, slot, &hs));
            CHECK(hs.points == 1000 + slot);
            CHECK(hs.weapon == WEAPON_PISTOL);
            CHECK(hs.beacon_y == 77.0f * (slot + 1));
        }
    }

    /* ------------------------------------------------------------ teardown */
    for (int i = 0; i < 3; i++) net_client_shutdown(&c[i]);
    net_server_shutdown(&server);

    printf("==============================================================\n");
    LOG_INFO(g_failed ? "net_test: FAIL" : "net_test: PASS");
    printf("==============================================================\n");
    log_shutdown();
    return g_failed ? 1 : 0;
}