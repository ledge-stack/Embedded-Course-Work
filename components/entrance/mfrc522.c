#include "entrance/mfrc522.h"
#include "storage/runtime.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>

#define PIN_RST 4
#define PIN_ENTRY_CS 5
#define PIN_EXIT_CS 13

/*
 * This is the low-level RFID driver. It talks to the MFRC522 registers over SPI,
 * asks for a card, selects it, and returns its UID. Access decisions live in core.c.
 */
enum {
    REG_COMMAND = 0x01,
    REG_COM_IEN = 0x02,
    REG_DIV_IEN = 0x03,
    REG_COM_IRQ = 0x04,
    REG_DIV_IRQ = 0x05,
    REG_ERROR = 0x06,
    REG_STATUS2 = 0x08,
    REG_FIFO_DATA = 0x09,
    REG_FIFO_LEVEL = 0x0A,
    REG_CONTROL = 0x0C,
    REG_BIT_FRAMING = 0x0D,
    REG_MODE = 0x11,
    REG_TX_CONTROL = 0x14,
    REG_TX_ASK = 0x15,
    REG_CRC_RESULT_H = 0x21,
    REG_CRC_RESULT_L = 0x22,
    REG_T_MODE = 0x2A,
    REG_T_PRESCALER = 0x2B,
    REG_T_RELOAD_H = 0x2C,
    REG_T_RELOAD_L = 0x2D,
    REG_VERSION = 0x37
};
enum { CMD_IDLE = 0x00, CMD_CALC_CRC = 0x03, CMD_TRANSCEIVE = 0x0C, CMD_SOFT_RESET = 0x0F };
enum {
    PICC_REQA = 0x26,
    PICC_ANTICOLL_CL1 = 0x93,
    PICC_ANTICOLL_CL2 = 0x95,
    PICC_ANTICOLL_CL3 = 0x97
};

static spi_device_handle_t devices[2];
static const char *TAG = "mfrc522";
static char last_poll_issue[2][128];
static TickType_t last_poll_issue_tick[2];

static void log_poll_issue(uint8_t reader, const char *issue) {
    TickType_t now = xTaskGetTickCount();
    bool changed = strcmp(last_poll_issue[reader], issue) != 0;
    bool old = (now - last_poll_issue_tick[reader]) >= pdMS_TO_TICKS(3000);
    if (changed || old) {
        ESP_LOGW(TAG, "reader %u: %s", reader, issue);
        snprintf(last_poll_issue[reader], sizeof(last_poll_issue[reader]), "%s", issue);
        last_poll_issue_tick[reader] = now;
    }
}

static esp_err_t reg_write(spi_device_handle_t dev, uint8_t reg, uint8_t value) {
    /* MFRC522 SPI writes send the register address followed by its value. */
    uint8_t tx[2] = {(uint8_t)((reg << 1) & 0x7e), value};
    spi_transaction_t t = {.length = 16, .tx_buffer = tx};
    return spi_device_transmit(dev, &t);
}
static esp_err_t reg_read(spi_device_handle_t dev, uint8_t reg, uint8_t *value) {
    uint8_t tx[2] = {(uint8_t)(((reg << 1) & 0x7e) | 0x80), 0}, rx[2] = {0};
    spi_transaction_t t = {.length = 16, .tx_buffer = tx, .rx_buffer = rx};
    esp_err_t e = spi_device_transmit(dev, &t);
    if (e == ESP_OK)
        *value = rx[1];
    return e;
}
static esp_err_t fifo_write(spi_device_handle_t dev, const uint8_t *data, size_t n) {
    for (size_t i = 0; i < n; i++) {
        esp_err_t e = reg_write(dev, REG_FIFO_DATA, data[i]);
        if (e != ESP_OK)
            return e;
    }
    return ESP_OK;
}
static esp_err_t fifo_read(spi_device_handle_t dev, uint8_t *data, size_t n) {
    if (n == 0)
        return ESP_OK;
    /* Keep CS low for the whole FIFO read, as required for a burst read. */
    uint8_t tx[11] = {0}, rx[11] = {0};
    uint8_t address = (uint8_t)(((REG_FIFO_DATA << 1) & 0x7e) | 0x80);
    tx[0] = address;
    for (size_t i = 1; i < n; i++)
        tx[i] = address;
    tx[n] = 0;
    spi_transaction_t t = {.length = (n + 1) * 8, .tx_buffer = tx, .rx_buffer = rx};
    esp_err_t e = spi_device_transmit(dev, &t);
    if (e != ESP_OK)
        return e;
    for (size_t i = 0; i < n; i++)
        data[i] = rx[i + 1];
    return ESP_OK;
}
static esp_err_t calculate_crc(spi_device_handle_t dev, const uint8_t *data, size_t n,
                               uint8_t out[2]) {
    uint8_t v = 0;
    if (reg_write(dev, REG_COMMAND, CMD_IDLE) != ESP_OK ||
        reg_write(dev, REG_DIV_IRQ, 0x04) != ESP_OK ||
        reg_write(dev, REG_FIFO_LEVEL, 0x80) != ESP_OK || fifo_write(dev, data, n) != ESP_OK ||
        reg_write(dev, REG_COMMAND, CMD_CALC_CRC) != ESP_OK)
        return ESP_FAIL;
    for (unsigned i = 0; i < 100; i++) {
        if (reg_read(dev, REG_DIV_IRQ, &v) != ESP_OK)
            return ESP_FAIL;
        if (v & 0x04)
            break;
        vTaskDelay(1);
    }
    if (!(v & 0x04))
        return ESP_ERR_TIMEOUT;
    if (reg_read(dev, REG_CRC_RESULT_L, &out[0]) != ESP_OK)
        return ESP_FAIL;
    return reg_read(dev, REG_CRC_RESULT_H, &out[1]);
}
static bool transceive(spi_device_handle_t dev, const uint8_t *tx, size_t tx_n, uint8_t tx_bits,
                       uint8_t *rx, size_t rx_cap, size_t *rx_n, uint8_t *rx_bits) {
    /* Send a command, wait briefly for the reader, then copy its reply. */
    uint8_t irq = 0, level = 0, error = 0, control = 0;
    if (reg_write(dev, REG_COMMAND, CMD_IDLE) != ESP_OK ||
        reg_write(dev, REG_COM_IRQ, 0x7f) != ESP_OK ||
        reg_write(dev, REG_FIFO_LEVEL, 0x80) != ESP_OK || fifo_write(dev, tx, tx_n) != ESP_OK ||
        reg_write(dev, REG_BIT_FRAMING, tx_bits & 7) != ESP_OK ||
        reg_write(dev, REG_COMMAND, CMD_TRANSCEIVE) != ESP_OK ||
        reg_write(dev, REG_BIT_FRAMING, (uint8_t)((tx_bits & 7) | 0x80)) != ESP_OK)
        return false;
    for (unsigned i = 0; i < 40; i++) {
        if (reg_read(dev, REG_COM_IRQ, &irq) != ESP_OK)
            return false;
        /* Idle can mean the command ended without receiving a card reply. */
        if (irq & 0x20)
            break;
        if (irq & 0x01)
            return false;
        vTaskDelay(1);
    }
    reg_write(dev, REG_BIT_FRAMING, 0);
    if (!(irq & 0x20) || reg_read(dev, REG_ERROR, &error) != ESP_OK || (error & 0x1b) ||
        reg_read(dev, REG_FIFO_LEVEL, &level) != ESP_OK || level > rx_cap ||
        fifo_read(dev, rx, level) != ESP_OK || reg_read(dev, REG_CONTROL, &control) != ESP_OK)
        return false;
    *rx_n = level;
    *rx_bits = control & 7;
    return true;
}
static bool select_level(spi_device_handle_t dev, uint8_t sel, uint8_t block[5], uint8_t *sak) {
    uint8_t buf[9] = {sel, 0x70, block[0], block[1], block[2], block[3], block[4], 0, 0};
    uint8_t crc[2], response[3], response_crc[2], bits = 0;
    size_t n = 0;
    if (calculate_crc(dev, buf, 7, crc) != ESP_OK)
        return false;
    buf[7] = crc[0];
    buf[8] = crc[1];
    if (!transceive(dev, buf, sizeof(buf), 0, response, sizeof(response), &n, &bits) || n != 3 ||
        bits != 0)
        return false;
    if (calculate_crc(dev, response, 1, response_crc) != ESP_OK || response[1] != response_crc[0] ||
        response[2] != response_crc[1])
        return false;
    *sak = response[0];
    return true;
}
static void halt_card(spi_device_handle_t dev) {
    uint8_t command[4] = {0x50, 0x00, 0, 0}, crc[2], response[2], bits = 0;
    size_t n = 0;
    if (calculate_crc(dev, command, 2, crc) != ESP_OK)
        return;
    command[2] = crc[0];
    command[3] = crc[1];
    /* A successful HALT has no reply; false from transceive is expected. */
    (void)transceive(dev, command, sizeof(command), 0, response, sizeof(response), &n, &bits);
}

esp_err_t mfrc522_init(void) {
    /* main starts storage first; that module owns the shared SPI bus. */
    esp_err_t e = storage_runtime_init_shared_spi();
    if (e != ESP_OK)
        return e;
    const int cs[2] = {PIN_ENTRY_CS, PIN_EXIT_CS};
    for (unsigned i = 0; i < 2; i++) {
        spi_device_interface_config_t cfg = {
            .clock_speed_hz = 1000000, .mode = 0, .spics_io_num = cs[i], .queue_size = 1};
        e = spi_bus_add_device(SPI2_HOST, &cfg, &devices[i]);
        if (e != ESP_OK)
            return e;
    }
    gpio_config_t rst = {
        .pin_bit_mask = 1ULL << PIN_RST, .mode = GPIO_MODE_OUTPUT, .intr_type = GPIO_INTR_DISABLE};
    e = gpio_config(&rst);
    if (e != ESP_OK)
        return e;
    gpio_set_level(PIN_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(50));
    for (unsigned i = 0; i < 2; i++) {
        e = reg_write(devices[i], REG_COMMAND, CMD_SOFT_RESET);
        if (e != ESP_OK)
            return e;
        vTaskDelay(pdMS_TO_TICKS(50));
        uint8_t version = 0;
        if (reg_read(devices[i], REG_VERSION, &version) != ESP_OK || version == 0 ||
            version == 0xff) {
            ESP_LOGE(TAG, "reader %u not responding (version 0x%02x)", i, version);
            return ESP_ERR_NOT_FOUND;
        }
        reg_write(devices[i], REG_T_MODE, 0x8d);
        reg_write(devices[i], REG_T_PRESCALER, 0x3e);
        reg_write(devices[i], REG_T_RELOAD_L, 30);
        reg_write(devices[i], REG_T_RELOAD_H, 0);
        reg_write(devices[i], REG_TX_ASK, 0x40);
        reg_write(devices[i], REG_MODE, 0x3d);
        uint8_t tx = 0;
        reg_read(devices[i], REG_TX_CONTROL, &tx);
        reg_write(devices[i], REG_TX_CONTROL, tx | 0x03);
    }
    return ESP_OK;
}

bool mfrc522_poll(uint8_t reader, uint8_t *uid, uint8_t *uid_size) {
    if (reader > 1 || !uid || !uid_size || !devices[reader])
        return false;
    uint8_t atqa[2], bits = 0;
    size_t n = 0;
    uint8_t request = PICC_REQA;
    if (!transceive(devices[reader], &request, 1, 7, atqa, sizeof(atqa), &n, &bits)) {
        log_poll_issue(reader, "no card response to REQA");
        return false;
    }
    if (n != 2) {
        char issue[128];
        snprintf(issue, sizeof(issue), "REQA reply length=%u bits=%u (expected 2 bytes)",
                 (unsigned)n, bits);
        log_poll_issue(reader, issue);
        return false;
    }
    /* ISO 14443 cards can return a UID in one, two, or three cascade levels. */
    static const uint8_t sels[3] = {PICC_ANTICOLL_CL1, PICC_ANTICOLL_CL2, PICC_ANTICOLL_CL3};
    uint8_t result[10];
    size_t used = 0;
    for (unsigned level = 0; level < 3; level++) {
        uint8_t anticoll[2] = {sels[level], 0x20}, block[5], rx_bits = 0;
        size_t rx_n = 0;
        if (!transceive(devices[reader], anticoll, 2, 0, block, sizeof(block), &rx_n, &rx_bits)) {
            log_poll_issue(reader, "UID anti-collision received no reply");
            return false;
        }
        if (rx_n != 5) {
            char issue[128];
            snprintf(issue, sizeof(issue),
                     "UID anti-collision reply length=%u bits=%u (expected 5 bytes)",
                     (unsigned)rx_n, rx_bits);
            log_poll_issue(reader, issue);
            return false;
        }
        uint8_t bcc = (uint8_t)(block[0] ^ block[1] ^ block[2] ^ block[3]);
        if (bcc != block[4]) {
            char issue[128];
            snprintf(issue, sizeof(issue), "UID BCC mismatch: %02X %02X %02X %02X, got %02X",
                     block[0], block[1], block[2], block[3], block[4]);
            log_poll_issue(reader, issue);
            return false;
        }
        uint8_t sak = 0;
        if (!select_level(devices[reader], sels[level], block, &sak)) {
            log_poll_issue(reader, "card UID select failed");
            return false;
        }
        if (block[0] == 0x88) {
            if (!(sak & 0x04)) {
                log_poll_issue(reader, "card cascade response was invalid");
                return false;
            }
            memcpy(result + used, block + 1, 3);
            used += 3;
            continue;
        }
        if (sak & 0x04) {
            log_poll_issue(reader, "card cascade response was invalid");
            return false;
        }
        memcpy(result + used, block, 4);
        used += 4;
        break;
    }
    if (used != 4 && used != 7 && used != 10) {
        log_poll_issue(reader, "card UID length is invalid");
        return false;
    }
    last_poll_issue[reader][0] = '\0';
    halt_card(devices[reader]);
    memcpy(uid, result, used);
    *uid_size = (uint8_t)used;
    return true;
}
