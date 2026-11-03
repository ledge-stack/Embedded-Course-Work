#ifndef ACTUATOR_RUNTIME_H
#define ACTUATOR_RUNTIME_H

#include <stdbool.h>
#include "esp_err.h"
#include "common/system.h"

/* Separate simulated entry/exit doors. Denied scans never open an output. */
esp_err_t actuator_runtime_start(void);
void actuator_runtime_signal(system_access_point point, bool granted);
void actuator_runtime_close(void);

#endif
