#include "entrance/display.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <string.h>

#define LCD_SDA 21
#define LCD_SCL 22
#define LCD_ADDRESS 0x27

/* The LCD is a 4-row display connected through an I2C expander. */
static i2c_master_bus_handle_t bus;
static i2c_master_dev_handle_t lcd;
static SemaphoreHandle_t display_mutex;
static const uint8_t row_address[4] = {0x80, 0xc0, 0x94, 0xd4};

static esp_err_t expander_write(uint8_t value) {
    return i2c_master_transmit(lcd, &value, 1, 100);
}
static esp_err_t write_nibble(uint8_t nibble, bool data) {
    uint8_t base = (uint8_t)((nibble & 0xf0) | 0x08 | (data ? 0x01 : 0));
    if (expander_write(base | 0x04) != ESP_OK)
        return ESP_FAIL;
    if (expander_write(base) != ESP_OK)
        return ESP_FAIL;
    return ESP_OK;
}
static esp_err_t write_byte(uint8_t value, bool data) {
    if (write_nibble(value & 0xf0, data) != ESP_OK)
        return ESP_FAIL;
    return write_nibble((uint8_t)(value << 4), data);
}
static void command(uint8_t value) {
    (void)write_byte(value, false);
    if (value == 0x01 || value == 0x02)
        vTaskDelay(pdMS_TO_TICKS(2));
}

esp_err_t entrance_display_init(void) {
    i2c_master_bus_config_t bus_config = {.i2c_port = I2C_NUM_0,
                                          .sda_io_num = LCD_SDA,
                                          .scl_io_num = LCD_SCL,
                                          .clk_source = I2C_CLK_SRC_DEFAULT,
                                          .glitch_ignore_cnt = 7,
                                          .flags.enable_internal_pullup = true};
    esp_err_t e = i2c_new_master_bus(&bus_config, &bus);
    if (e != ESP_OK)
        return e;
    i2c_device_config_t device_config = {.dev_addr_length = I2C_ADDR_BIT_LEN_7,
                                         .device_address = LCD_ADDRESS,
                                         .scl_speed_hz = 100000};
    e = i2c_master_bus_add_device(bus, &device_config, &lcd);
    if (e != ESP_OK)
        return e;
    display_mutex = xSemaphoreCreateMutex();
    if (!display_mutex)
        return ESP_ERR_NO_MEM;
    vTaskDelay(pdMS_TO_TICKS(50));
    if (write_nibble(0x30, false) != ESP_OK)
        return ESP_FAIL;
    vTaskDelay(pdMS_TO_TICKS(5));
    if (write_nibble(0x30, false) != ESP_OK)
        return ESP_FAIL;
    vTaskDelay(pdMS_TO_TICKS(1));
    if (write_nibble(0x30, false) != ESP_OK)
        return ESP_FAIL;
    if (write_nibble(0x20, false) != ESP_OK)
        return ESP_FAIL;
    command(0x28);
    command(0x0c);
    command(0x06);
    command(0x01);
    return ESP_OK;
}

void entrance_display_write(const char rows[4][21]) {
    if (!lcd || !display_mutex || !rows)
        return;
    xSemaphoreTake(display_mutex, portMAX_DELAY);
    for (unsigned row = 0; row < 4; row++) {
        command(row_address[row]);
        for (unsigned col = 0; col < 20; col++) {
            char c = rows[row][col];
            (void)write_byte(c ? (uint8_t)c : (uint8_t)' ', true);
        }
    }
    xSemaphoreGive(display_mutex);
}
