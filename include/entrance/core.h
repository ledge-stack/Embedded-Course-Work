#ifndef ENTRANCE_CORE_H
#define ENTRANCE_CORE_H

#include "common/system.h"
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    system_user users[SYSTEM_MAX_USERS];
    bool inside[SYSTEM_MAX_USERS];
    uint32_t event_id;
    system_access_schedule schedule;
} entrance_state;

/* Fill a 32-slot roster with the four Wokwi member cards and disable unused slots. */
void entrance_default_users(system_user users[SYSTEM_MAX_USERS]);
void entrance_state_init(entrance_state *state);
bool entrance_schedule_valid(system_access_schedule schedule);
bool entrance_is_open(system_access_schedule schedule, unsigned local_minute);
size_t entrance_inside_count(const entrance_state *state);
bool entrance_is_inside(const entrance_state *state, uint32_t user_id);
/* Apply an event with its source sequence. Duplicate and older event IDs are
 * ignored; IDs are nonzero and use 32-bit serial-number ordering. */
system_access_result entrance_apply_event(entrance_state *state, uint32_t event_id,
                                          system_access_point point, const uint8_t *uid,
                                          size_t uid_size, bool clock_valid, unsigned local_minute,
                                          uint32_t *user_id);
system_access_result entrance_apply_scan(entrance_state *state, system_access_point point,
                                         const uint8_t *uid, size_t uid_size, bool clock_valid,
                                         unsigned local_minute, uint32_t *user_id,
                                         uint32_t *event_id);

#endif
