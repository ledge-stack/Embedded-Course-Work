#ifndef ENTRANCE_DISPLAY_H
#define ENTRANCE_DISPLAY_H

#include "esp_err.h"

esp_err_t entrance_display_init(void);
void entrance_display_write(const char rows[4][21]);

#endif
