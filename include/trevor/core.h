#ifndef TREVOR_CORE_H
#define TREVOR_CORE_H
#include <stdbool.h>
#include <stdint.h>

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
    uint64_t last_update_ms;
    uint64_t light_on_ms;
    uint64_t empty_suppressed_ms;
    bool started;
    bool light_was_on;
    bool light_was_suppressed;
} tr_usage;
void tr_usage_sample(tr_usage *usage, uint64_t now_ms, bool light_on, bool light_suppressed);

typedef struct {
    uint8_t room;
    bool occupied, light;
    uint32_t off_delay_ms;
    uint64_t uptime_ms;
} tr_room_state;
#endif
