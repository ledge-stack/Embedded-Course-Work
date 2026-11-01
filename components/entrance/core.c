#include "entrance/core.h"
#include <string.h>

static const system_user demo_users[] = {{.uid_size = 4,
                                          .uid = {0xDE, 0xAD, 0xBE, 0xEF},
                                          .display_name = "Demo User 1",
                                          .enabled = true},
                                         {.uid_size = 4,
                                          .uid = {0x11, 0x22, 0x33, 0x44},
                                          .display_name = "Demo User 2",
                                          .enabled = true}};

void entrance_state_init(entrance_state *state) {
    if (!state)
        return;
    memset(state, 0, sizeof(*state));
    memcpy(state->users, demo_users, sizeof(demo_users));
    state->schedule = (system_access_schedule){.start_minute = 8u * 60u, .end_minute = 18u * 60u};
}

bool entrance_schedule_valid(system_access_schedule s) {
    /* A day has 1,440 minutes. Equal start and end times are ambiguous. */
    return s.start_minute < 1440u && s.end_minute < 1440u && s.start_minute != s.end_minute;
}

bool entrance_is_open(system_access_schedule s, unsigned minute) {
    if (!entrance_schedule_valid(s) || minute >= 1440u)
        return false;
    /* The second case supports overnight hours, such as 22:00 to 06:00. */
    if (s.start_minute < s.end_minute)
        return minute >= s.start_minute && minute < s.end_minute;
    return minute >= s.start_minute || minute < s.end_minute;
}

size_t entrance_inside_count(const entrance_state *state) {
    if (!state)
        return 0;
    size_t n = 0;
    for (size_t i = 0; i < SYSTEM_MAX_USERS; i++)
        n += state->inside[i] ? 1u : 0u;
    return n;
}

bool entrance_is_inside(const entrance_state *state, uint32_t id) {
    return state && id < SYSTEM_MAX_USERS && state->inside[id];
}

system_access_result entrance_apply_event(entrance_state *state, uint32_t event_id,
                                          system_access_point point, const uint8_t *uid,
                                          size_t uid_size, bool clock_valid, unsigned local_minute,
                                          uint32_t *user_id) {
    if (user_id)
        *user_id = UINT32_MAX;
    if (!state || !event_id || !uid || !uid_size || uid_size > SYSTEM_UID_MAX_BYTES ||
        (point != SYSTEM_ENTRY && point != SYSTEM_EXIT))
        return SYSTEM_ACCESS_UNKNOWN_USER;
    /* Ignore duplicate or old events before changing occupancy. */
    if (state->event_id) {
        uint32_t delta = event_id - state->event_id;
        if (!delta || delta >= 0x80000000u)
            return SYSTEM_ACCESS_STALE_EVENT;
    }
    state->event_id = event_id;
    uint32_t found = UINT32_MAX;
    for (uint32_t i = 0; i < SYSTEM_MAX_USERS; i++) {
        const system_user *u = &state->users[i];
        if (u->enabled && u->uid_size == uid_size && memcmp(u->uid, uid, uid_size) == 0) {
            found = i;
            break;
        }
    }
    /* Check identity and current inside status before applying a scan. */
    system_access_result result;
    if (found == UINT32_MAX)
        result = SYSTEM_ACCESS_UNKNOWN_USER;
    else if (point == SYSTEM_ENTRY && state->inside[found])
        result = SYSTEM_ACCESS_ALREADY_INSIDE;
    else if (point == SYSTEM_EXIT && !state->inside[found])
        result = SYSTEM_ACCESS_NOT_INSIDE;
    else if (point == SYSTEM_ENTRY && !clock_valid)
        result = SYSTEM_ACCESS_CLOCK_UNAVAILABLE;
    else if (point == SYSTEM_ENTRY && !entrance_is_open(state->schedule, local_minute))
        result = SYSTEM_ACCESS_SCHEDULE_CLOSED;
    else {
        state->inside[found] = (point == SYSTEM_ENTRY);
        result = SYSTEM_ACCESS_GRANTED;
    }
    if (user_id)
        *user_id = found;
    return result;
}

system_access_result entrance_apply_scan(entrance_state *state, system_access_point point,
                                         const uint8_t *uid, size_t uid_size, bool clock_valid,
                                         unsigned local_minute, uint32_t *user_id,
                                         uint32_t *event_id) {
    if (event_id)
        *event_id = 0;
    if (!state)
        return SYSTEM_ACCESS_UNKNOWN_USER;
    uint32_t next = state->event_id + 1u;
    if (!next)
        next = 1;
    if (event_id)
        *event_id = next;
    return entrance_apply_event(state, next, point, uid, uid_size, clock_valid, local_minute,
                                user_id);
}
