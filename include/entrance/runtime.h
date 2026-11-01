#ifndef ENTRANCE_RUNTIME_H
#define ENTRANCE_RUNTIME_H

#include "esp_err.h"
#include "common/system.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    bool clock_valid;
    bool is_open;
    unsigned local_minute;
    size_t inside_count;
    system_access_schedule schedule;
    bool has_last_event;
    bool occupancy_uncertain;
    char last_person[SYSTEM_NAME_MAX_BYTES];
    char last_action[8];
    char last_time[6];
} entrance_status;

esp_err_t entrance_runtime_start(void);
/* Edward loads registered users and passes the fixed 32-slot roster here.
 * User IDs are stable array indexes; an occupied user's UID cannot be removed. */
esp_err_t entrance_runtime_set_users(const system_user users[SYSTEM_MAX_USERS]);
bool entrance_runtime_snapshot(system_user *users, bool *inside, size_t capacity,
                               size_t *user_count, size_t *inside_count,
                               system_access_schedule *schedule);
/* Schedule writes must be made only after caller-side administrator authentication. */
esp_err_t entrance_runtime_set_schedule(system_access_schedule schedule);
bool entrance_runtime_status(entrance_status *out);
/* Print a read-only roll-call. after_hours adds an alert heading. */
void entrance_runtime_print_roll_call(bool after_hours);
/* Admin confirms the building is physically empty after a restart/tailgate alert. */
esp_err_t entrance_runtime_confirm_empty(void);
/* Called by Grace's labelled Wokwi test button; never changes the inside list. */
void entrance_runtime_note_simulated_tailgate(void);

#endif
