#include "trevor/runtime.h"
#include "entrance/runtime.h"
#include "nvs_flash.h"

void app_main(void) {
    esp_err_t nvs=nvs_flash_init();
    if(nvs==ESP_ERR_NVS_NO_FREE_PAGES || nvs==ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs=nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs);
    ESP_ERROR_CHECK(tr_runtime_start());
    ESP_ERROR_CHECK(entrance_runtime_start());
}
