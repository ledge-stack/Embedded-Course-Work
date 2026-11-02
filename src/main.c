#include "trevor/runtime.h"
#include "entrance/runtime.h"
#include "entrance/core.h"
#include "nvs_flash.h"
#include "storage/runtime.h"
#include "actuator/runtime.h"
#include "cli/runtime.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <string.h>

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

static bool is_old_demo_roster(const system_user users[SYSTEM_MAX_USERS]) {
    const uint8_t blue_uid[] = {0xDE, 0xAD, 0xBE, 0xEF};
    const uint8_t green_uid[] = {0x11, 0x22, 0x33, 0x44};
    unsigned old_blue = 0, old_green = 0;
    for (size_t i = 0; i < SYSTEM_MAX_USERS; i++) {
        if (!users[i].enabled)
            continue;
        if (!memchr(users[i].display_name, '\0', sizeof(users[i].display_name)))
            return false;
        if (users[i].uid_size == sizeof(blue_uid) &&
            memcmp(users[i].uid, blue_uid, sizeof(blue_uid)) == 0 &&
            strcmp(users[i].display_name, "Demo User 1") == 0) {
            old_blue++;
        } else if (users[i].uid_size == sizeof(green_uid) &&
                   memcmp(users[i].uid, green_uid, sizeof(green_uid)) == 0 &&
                   strcmp(users[i].display_name, "Demo User 2") == 0) {
            old_green++;
        } else {
            return false;
        }
    }
    return old_blue == 1 && old_green == 1;
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
        bool migrated_demo_roster = is_old_demo_roster(saved_users);
        if (migrated_demo_roster)
            entrance_default_users(saved_users);
        esp_err_t user_error = entrance_runtime_set_users(saved_users);
        if (user_error != ESP_OK)
            ESP_LOGW(TAG, "saved roster invalid; using demo users: %s",
                     esp_err_to_name(user_error));
        else if (migrated_demo_roster && storage_runtime_save_users(saved_users) != ESP_OK)
            ESP_LOGW(TAG, "updated demo names are active, but roster migration backup failed");
    }
    BaseType_t task_created =
        xTaskCreate(occupancy_bridge_task, "occupancy_bridge", 3072, NULL, 3, NULL);
    ESP_ERROR_CHECK(task_created == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(cli_runtime_start());
#endif
}
