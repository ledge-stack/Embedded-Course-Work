#include "trevor/runtime.h"
#include "entrance/runtime.h"
#include "nvs_flash.h"
#include "storage/runtime.h"
#include "actuator/runtime.h"
#include "cli/runtime.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#if TR_ROLE == 0
static const char *TAG = "coursework";
#endif

#if TR_ROLE == 0
/* Pass the entrance's people count to the room controller every 250 ms. */
static void occupancy_bridge_task(void *arg) {
    (void)arg;
    while (true) {
        entrance_status status;
        if (entrance_runtime_status(&status))
            (void)tr_runtime_set_building_inside_count(status.inside_count,
                                                       !status.occupancy_uncertain);
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}
#endif

void app_main(void) {
    /* NVS is shared by all modules. Fail safely instead of erasing team data. */
    ESP_ERROR_CHECK(nvs_flash_init());
#if TR_ROLE == 0
    ESP_ERROR_CHECK(storage_runtime_start());
#endif
    ESP_ERROR_CHECK(tr_runtime_start());
#if TR_ROLE == 0
    /* One-chip Wokwi presentation build: logical rooms plus Leon's entrance. */
    ESP_ERROR_CHECK(actuator_runtime_start());
    ESP_ERROR_CHECK(entrance_runtime_start());
    system_user saved_users[SYSTEM_MAX_USERS];
    if (storage_runtime_load_users(saved_users) == ESP_OK) {
        esp_err_t user_error = entrance_runtime_set_users(saved_users);
        if (user_error != ESP_OK)
            ESP_LOGW(TAG, "saved roster invalid; using demo users: %s",
                     esp_err_to_name(user_error));
    }
    BaseType_t task_created =
        xTaskCreate(occupancy_bridge_task, "occupancy_bridge", 3072, NULL, 3, NULL);
    ESP_ERROR_CHECK(task_created == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(cli_runtime_start());
#endif
}
