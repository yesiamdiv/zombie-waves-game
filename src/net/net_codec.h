#ifndef NET_CODEC_H
#define NET_CODEC_H

#include "net.h"

/* Encode the 7-byte header (creates the trailing checksum) / decode + verify
 * it. Return 0 on success, -1 on bad args or checksum mismatch. */
int net_hdr_encode(uint8_t out[NET_HDR_SIZE], const NetHeader *h);
int net_hdr_decode(NetHeader *h, const uint8_t in[NET_HDR_SIZE]);

/* Full-packet coders: encode writes header+body into `buf` and returns the
 * payload length (or -1 on bad args / overflow); decode returns 0 and fills
 * `h` + the body out-params, or -1 on malformed input. Header version is
 * always NET_WIRE_VERSION on encode; `seq`/`flags` for the sender to set. */

int net_encode_join(uint8_t *buf, int cap, const NetHeader *h, const char *name);
int net_decode_join(const uint8_t *buf, int len, NetHeader *h, char *name,
                    int name_cap);

int net_encode_hello(uint8_t *buf, int cap, const NetHeader *h, uint8_t slot,
                     uint32_t seed, uint32_t world_gen, const char *host_name);
int net_decode_hello(const uint8_t *buf, int len, NetHeader *h, uint8_t *slot,
                     uint32_t *seed, uint32_t *world_gen, char *host_name,
                     int name_cap);

int net_encode_reject(uint8_t *buf, int cap, const NetHeader *h, uint8_t reason);
int net_decode_reject(const uint8_t *buf, int len, NetHeader *h, uint8_t *reason);

int net_encode_player_list(uint8_t *buf, int cap, const NetPlayerInfo *players,
                           int count);
int net_decode_player_list(const uint8_t *buf, int len, NetHeader *h,
                           NetPlayerInfo *players, int count_max, int *count_out);

int net_encode_leave(uint8_t *buf, int cap, const NetHeader *h, uint8_t reason);
int net_decode_leave(const uint8_t *buf, int len, NetHeader *h, uint8_t *reason);

/* Channel 1 game traffic -------------------------------------------------- */

/* Input sample (client -> host; latest-wins, never re-acked). */
int net_encode_input(uint8_t *buf, int cap, const NetHeader *h,
                     const NetInput *in);
int net_decode_input(const uint8_t *buf, int len, NetHeader *h, NetInput *in);

/* World snapshot (host -> clients). `snap->count` entities are serialized. */
int net_encode_snapshot(uint8_t *buf, int cap, const NetHeader *h,
                        const NetSnapshot *snap);
int net_decode_snapshot(const uint8_t *buf, int len, NetHeader *h,
                        NetSnapshot *snap, int max_entities);

#endif