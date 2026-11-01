#include "entrance/core.h"
#include <assert.h>
#include <stdio.h>

static const uint8_t demo_uid[] = {0xDE, 0xAD, 0xBE, 0xEF};

int main(void) {
    entrance_state state;
    uint32_t user_id;
    uint32_t event_id;

    entrance_state_init(&state);
    assert(entrance_inside_count(&state) == 0);

    assert(entrance_apply_scan(&state, SYSTEM_ENTRY, demo_uid, sizeof(demo_uid), true, 9u * 60u,
                               &user_id, &event_id) == SYSTEM_ACCESS_GRANTED);
    assert(user_id == 0 && event_id == 1);
    assert(entrance_inside_count(&state) == 1);

    assert(entrance_apply_scan(&state, SYSTEM_EXIT, demo_uid, sizeof(demo_uid), false, 9u * 60u,
                               &user_id, &event_id) == SYSTEM_ACCESS_CLOCK_UNAVAILABLE);
    assert(entrance_inside_count(&state) == 1);

    assert(entrance_apply_scan(&state, SYSTEM_ENTRY, demo_uid, sizeof(demo_uid), true, 9u * 60u,
                               &user_id, &event_id) == SYSTEM_ACCESS_ALREADY_INSIDE);
    assert(entrance_inside_count(&state) == 1);

    /* The schedule applies to the exit reader as well as the entry reader. */
    assert(entrance_apply_scan(&state, SYSTEM_EXIT, demo_uid, sizeof(demo_uid), true, 20u * 60u,
                               &user_id, &event_id) == SYSTEM_ACCESS_SCHEDULE_CLOSED);
    assert(entrance_inside_count(&state) == 1);

    assert(entrance_apply_scan(&state, SYSTEM_EXIT, demo_uid, sizeof(demo_uid), true, 17u * 60u,
                               &user_id, &event_id) == SYSTEM_ACCESS_GRANTED);
    assert(entrance_inside_count(&state) == 0);

    assert(entrance_apply_scan(&state, SYSTEM_EXIT, demo_uid, sizeof(demo_uid), true, 20u * 60u,
                               &user_id, &event_id) == SYSTEM_ACCESS_NOT_INSIDE);
    assert(entrance_inside_count(&state) == 0);

    assert(entrance_apply_scan(&state, SYSTEM_ENTRY, demo_uid, sizeof(demo_uid), true,
                               7u * 60u + 59u, &user_id,
                               &event_id) == SYSTEM_ACCESS_SCHEDULE_CLOSED);
    assert(entrance_inside_count(&state) == 0);

    assert(entrance_apply_scan(&state, SYSTEM_ENTRY, demo_uid, sizeof(demo_uid), false, 9u * 60u,
                               &user_id, &event_id) == SYSTEM_ACCESS_CLOCK_UNAVAILABLE);
    assert(entrance_inside_count(&state) == 0);

    puts("PASS entrance access, anti-passback, both-door schedule and clock rules");
    return 0;
}
