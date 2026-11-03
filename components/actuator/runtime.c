#include "actuator/runtime.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

/* GPIO12 keeps its external pulldown. GPIO32 replaces the old link switch. */
static const int door_pin[2] = {32, 12};
static int64_t close_at[2];
static SemaphoreHandle_t door_mutex;

static void door_timer_task(void *arg) {
    (void)arg;
    while (true) {
        xSemaphoreTake(door_mutex, portMAX_DELAY);
        int64_t now = esp_timer_get_time();
        for (unsigned i = 0; i < 2; i++) {
            if (close_at[i] && now >= close_at[i]) {
                gpio_set_level(door_pin[i], 0);
                close_at[i] = 0;
            }
        }
        xSemaphoreGive(door_mutex);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

esp_err_t actuator_runtime_start(void) {
    if (door_mutex) return ESP_ERR_INVALID_STATE;
    door_mutex = xSemaphoreCreateMutex();
    if (!door_mutex) return ESP_ERR_NO_MEM;
    gpio_config_t pins = {.pin_bit_mask = (1ULL << 32) | (1ULL << 12),
                          .mode = GPIO_MODE_OUTPUT, .pull_down_en = GPIO_PULLDOWN_ENABLE,
                          .intr_type = GPIO_INTR_DISABLE};
    esp_err_t error = gpio_config(&pins);
    if (error != ESP_OK) return error;
    actuator_runtime_close();
    return xTaskCreate(door_timer_task, "door_timer", 2048, NULL, 2, NULL) == pdPASS
               ? ESP_OK : ESP_ERR_NO_MEM;
}

void actuator_runtime_signal(system_access_point point, bool granted) {
    if (!door_mutex || !granted || (point != SYSTEM_ENTRY && point != SYSTEM_EXIT)) return;
    unsigned i = point == SYSTEM_ENTRY ? 0 : 1;
    xSemaphoreTake(door_mutex, portMAX_DELAY);
    gpio_set_level(door_pin[i], 1);
    close_at[i] = esp_timer_get_time() + 3000000;
    xSemaphoreGive(door_mutex);
}

void actuator_runtime_close(void) {
    if (!door_mutex) return;
    xSemaphoreTake(door_mutex, portMAX_DELAY);
    for (unsigned i = 0; i < 2; i++) {
        gpio_set_level(door_pin[i], 0);
        close_at[i] = 0;
    }
    xSemaphoreGive(door_mutex);
}
