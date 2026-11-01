#ifndef ACTUATOR_RUNTIME_H
#define ACTUATOR_RUNTIME_H

#include <stdbool.h>
#include "esp_err.h"

/* Simple simulation feedback: the status LED turns on for a granted scan. */
esp_err_t actuator_runtime_start(void);
void actuator_runtime_signal(bool granted);
void actuator_runtime_update(void);

#endif
