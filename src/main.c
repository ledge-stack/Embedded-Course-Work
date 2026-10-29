#include "trevor/runtime.h"
#include "entrance/runtime.h"
#include "nvs_flash.h"

void app_main(void) {
    /* NVS is shared by all modules. Fail safely instead of erasing team data. */
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(tr_runtime_start());
#if TR_ROLE == 0
    /* One-chip Wokwi presentation build: logical rooms plus Leon's entrance. */
    ESP_ERROR_CHECK(entrance_runtime_start());
#endif
}
