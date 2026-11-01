#ifndef TREVOR_HOST_SHIM_H
#define TREVOR_HOST_SHIM_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_ARG 0x102
#define ESP_ERR_INVALID_STATE 0x103
#define ESP_ERR_NO_MEM 0x101
#define ESP_ERR_NOT_SUPPORTED 0x106
#define ESP_ERR_NVS_NOT_FOUND 0x1102
#define ESP_ERROR_CHECK(x)                                                                         \
    do {                                                                                           \
        if ((x) != ESP_OK)                                                                         \
            abort();                                                                               \
    } while (0)
#define ESP_LOGI(tag, ...)                                                                         \
    do {                                                                                           \
        (void)(tag);                                                                               \
    } while (0)
#define ESP_LOGW(tag, ...)                                                                         \
    do {                                                                                           \
        (void)(tag);                                                                               \
    } while (0)
#define ESP_LOGE(tag, ...)                                                                         \
    do {                                                                                           \
        (void)(tag);                                                                               \
    } while (0)
typedef unsigned TickType_t;
typedef int BaseType_t;
typedef void *QueueHandle_t;
typedef void *SemaphoreHandle_t;
#define pdTRUE 1
#define pdPASS 1
#define pdMS_TO_TICKS(ms) (ms)
#define portMAX_DELAY INFINITE
QueueHandle_t xQueueCreate(unsigned length, unsigned size);
int xQueueSend(QueueHandle_t q, const void *data, unsigned timeout);
int xQueueReceive(QueueHandle_t q, void *data, unsigned timeout);
SemaphoreHandle_t xSemaphoreCreateMutex(void);
int xSemaphoreTake(SemaphoreHandle_t m, unsigned timeout);
int xSemaphoreGive(SemaphoreHandle_t m);
int xTaskCreate(void (*fn)(void *), const char *name, unsigned stack, void *arg, unsigned priority,
                void *handle);
void vTaskDelay(unsigned ticks);
int64_t esp_timer_get_time(void);
typedef struct {
    uint64_t pin_bit_mask;
    int mode, pull_up_en, pull_down_en, intr_type;
} gpio_config_t;
#define GPIO_MODE_INPUT 1
#define GPIO_MODE_OUTPUT 2
#define GPIO_PULLUP_ENABLE 1
#define GPIO_PULLUP_DISABLE 0
#define GPIO_PULLDOWN_ENABLE 1
#define GPIO_PULLDOWN_DISABLE 0
#define GPIO_INTR_DISABLE 0
int gpio_config(const gpio_config_t *c);
int gpio_get_level(int pin);
int gpio_set_level(int pin, int level);
void stub_gpio_input(int pin, int value);
typedef int uart_port_t;
typedef struct {
    int baud_rate, data_bits, parity, stop_bits, flow_ctrl, source_clk;
} uart_config_t;
#define UART_NUM_1 1
#define UART_NUM_2 2
#define UART_DATA_8_BITS 8
#define UART_PARITY_DISABLE 0
#define UART_STOP_BITS_1 1
#define UART_HW_FLOWCTRL_DISABLE 0
#define UART_SCLK_DEFAULT 0
#define UART_PIN_NO_CHANGE -1
int uart_param_config(int port, const uart_config_t *c);
int uart_set_pin(int port, int tx, int rx, int rts, int cts);
int uart_driver_install(int port, int rx, int tx, int q, void *h, int flags);
int uart_read_bytes(int port, void *bytes, unsigned size, unsigned ticks);
int uart_write_bytes(int port, const void *bytes, size_t size);
typedef unsigned nvs_handle_t;
#define NVS_READWRITE 1
int nvs_flash_init(void);
int nvs_open(const char *name, int mode, nvs_handle_t *h);
int nvs_get_u32(nvs_handle_t h, const char *key, uint32_t *v);
int nvs_set_u32(nvs_handle_t h, const char *key, uint32_t v);
int nvs_commit(nvs_handle_t h);
void nvs_close(nvs_handle_t h);
#endif
