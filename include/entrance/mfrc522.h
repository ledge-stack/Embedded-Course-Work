#ifndef ENTRANCE_MFRC522_H
#define ENTRANCE_MFRC522_H

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

esp_err_t mfrc522_init(void);
/* Reads one ISO/IEC 14443A UID. Returns false when no new card is present. */
bool mfrc522_poll(uint8_t reader, uint8_t *uid, uint8_t *uid_size);

#endif
