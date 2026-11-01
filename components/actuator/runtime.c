#include "actuator/runtime.h"
#include "driver/gpio.h"
#include "esp_timer.h"

/* GPIO12 is shared with the ESP32 boot strap; keep its external pull-down. */
#define STATUS_LED_GPIO 12
#define FEEDBACK_TIME_US 500000

static int64_t led_off_at;

esp_err_t actuator_runtime_start(void) {
    gpio_config_t config = {.pin_bit_mask = 1ULL << STATUS_LED_GPIO,
                            .mode = GPIO_MODE_OUTPUT,
                            .pull_down_en = GPIO_PULLDOWN_ENABLE,
                            .intr_type = GPIO_INTR_DISABLE};
    esp_err_t error = gpio_config(&config);
    if (error == ESP_OK)
        gpio_set_level(STATUS_LED_GPIO, 0);
    return error;
}

void actuator_runtime_signal(bool granted) {
    gpio_set_level(STATUS_LED_GPIO, 1);
    led_off_at = esp_timer_get_time() + (granted ? FEEDBACK_TIME_US : FEEDBACK_TIME_US * 2);
}

/* Called by the entrance poll task, so the pulse expires without another task. */
void actuator_runtime_update(void) {
    if (led_off_at && esp_timer_get_time() >= led_off_at) {
        gpio_set_level(STATUS_LED_GPIO, 0);
        led_off_at = 0;
    }
}
