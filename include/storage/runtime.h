#ifndef STORAGE_RUNTIME_H
#define STORAGE_RUNTIME_H

#include <stdbool.h>
#include "esp_err.h"
#include "trevor/runtime.h"
#include <stdio.h>
#include "common/system.h"

esp_err_t storage_runtime_start(void);
/* Shared by SD and MFRC522 before either device is attached to SPI2. */
esp_err_t storage_runtime_init_shared_spi(void);
bool tr_storage_append(const tr_storage_event *event);
bool storage_runtime_available(void);
void storage_runtime_print_events(FILE *output);
esp_err_t storage_runtime_load_users(system_user users[SYSTEM_MAX_USERS]);
/* ESP_OK means committed to authoritative NVS; SD mirror failures are warnings. */
esp_err_t storage_runtime_save_users(const system_user users[SYSTEM_MAX_USERS]);

#endif
