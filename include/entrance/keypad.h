#ifndef ENTRANCE_KEYPAD_H
#define ENTRANCE_KEYPAD_H

#include "esp_err.h"
#include <stdbool.h>

esp_err_t entrance_keypad_start(void);
bool entrance_keypad_admin_view(char rows[4][21]);

#endif
