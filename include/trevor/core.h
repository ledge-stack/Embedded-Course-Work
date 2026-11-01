#ifndef TREVOR_CORE_H
#define TREVOR_CORE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TR_FRAME_SIZE 28
#define TR_RETRY_MS 500u
#define TR_MAX_ATTEMPTS 5u
#define TR_HEARTBEAT_MS 2000u
#define TR_STALE_MS 6000u
#define TR_STATE 1
#define TR_ACK 2

typedef struct {
    uint8_t id;
    bool occupied, light;
    uint32_t off_delay_ms;
    uint64_t last_motion_ms;
} tr_room;
void tr_room_init(tr_room *room, uint8_t id, uint32_t delay_ms);
void tr_room_set_delay(tr_room *room, uint32_t delay_ms);
bool tr_room_sample(tr_room *room, bool motion, uint64_t now_ms);

typedef struct {
    uint8_t kind, room;
    bool occupied, light;
    uint32_t session, sequence;
    uint64_t uptime_ms;
    uint32_t off_delay_ms;
} tr_message;
bool tr_encode(const tr_message *message, uint8_t frame[TR_FRAME_SIZE]);
bool tr_decode(const uint8_t frame[TR_FRAME_SIZE], tr_message *message);
typedef struct {
    uint8_t bytes[TR_FRAME_SIZE];
    size_t used;
} tr_parser;
bool tr_parser_push(tr_parser *parser, uint8_t byte, tr_message *message);

typedef struct {
    tr_message pending;
    bool waiting, sent;
    uint64_t last_send_ms;
    uint32_t retries, timeouts;
    unsigned attempts;
} tr_sender;
bool tr_sender_offer(tr_sender *sender, const tr_message *message);
bool tr_sender_poll(tr_sender *sender, uint64_t now_ms, tr_message *message);
bool tr_sender_ack(tr_sender *sender, const tr_message *message);

enum tr_result { TR_INVALID, TR_NEW, TR_DUPLICATE, TR_STALE };
typedef struct {
    bool valid;
    tr_message state;
    uint64_t last_seen_ms;
} tr_receiver;
enum tr_result tr_receiver_accept(tr_receiver *receiver, const tr_message *message,
                                  uint64_t now_ms);
bool tr_receiver_online(const tr_receiver *receiver, uint64_t now_ms);
#endif
