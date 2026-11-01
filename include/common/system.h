#ifndef COMMON_SYSTEM_H
#define COMMON_SYSTEM_H

#include <stdbool.h>
#include <stdint.h>

#define SYSTEM_PROTOCOL_VERSION 1u
#define SYSTEM_MAX_USERS 32u
#define SYSTEM_UID_MAX_BYTES 10u
#define SYSTEM_NAME_MAX_BYTES 24u

typedef struct {
    uint8_t uid_size;
    uint8_t uid[SYSTEM_UID_MAX_BYTES];
    char display_name[SYSTEM_NAME_MAX_BYTES];
    bool enabled;
} system_user;

typedef enum { SYSTEM_ENTRY = 1, SYSTEM_EXIT = 2 } system_access_point;
typedef enum {
    SYSTEM_ACCESS_GRANTED = 1,
    SYSTEM_ACCESS_UNKNOWN_USER = 2,
    SYSTEM_ACCESS_ALREADY_INSIDE = 3,
    SYSTEM_ACCESS_NOT_INSIDE = 4,
    SYSTEM_ACCESS_SCHEDULE_CLOSED = 5,
    SYSTEM_ACCESS_CLOCK_UNAVAILABLE = 6,
    SYSTEM_ACCESS_STALE_EVENT = 7
} system_access_result;

typedef struct {
    uint32_t event_id;
    uint32_t user_id;
    uint64_t utc_epoch_seconds;
    uint64_t monotonic_ms;
    system_access_point point;
    system_access_result result;
    uint8_t uid_size;
    uint8_t uid[SYSTEM_UID_MAX_BYTES];
} system_access_event;

typedef struct {
    uint16_t start_minute, end_minute;
} system_access_schedule;

typedef struct {
    uint8_t version;
    uint8_t kind;
    uint16_t payload_size;
    uint32_t sequence;
    uint8_t payload[48];
} system_terminal_message;

#endif
