#ifndef ENTRANCE_RUNTIME_H
#define ENTRANCE_RUNTIME_H

#include "esp_err.h"
#include "common/system.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

esp_err_t entrance_runtime_start(void);
bool entrance_runtime_snapshot(system_user *users, bool *inside,
    size_t capacity, size_t *user_count, size_t *inside_count,
    system_access_schedule *schedule);
/* Called by Edward's authenticated CLI after validating admin credentials. */
esp_err_t entrance_runtime_set_schedule(system_access_schedule schedule);

#endif
