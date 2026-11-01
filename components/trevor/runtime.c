#include "trevor/runtime.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <inttypes.h>
#include <string.h>

#if TR_ROLE < 0 || TR_ROLE > 3
#error Invalid TR_ROLE
#endif

/*
 * One runtime supports four builds: combined demo, coordinator, room 1, room 2.
 * In the combined demo, FreeRTOS queues carry room frames and storage events.
 */
static const char *TAG = "trevor";
static const int motion_pin[2] = {27, 33}, light_pin[2] = {26, 25};
typedef struct {
    uint8_t bytes[TR_FRAME_SIZE];
} wire_frame;
typedef struct {
    uint8_t id;
} room_arg;
static room_arg room_args[2] = {{1}, {2}};
static QueueHandle_t up[2], down[2], storage_queue;
static SemaphoreHandle_t state_mutex;
static tr_receiver receivers[2];
static tr_system_view shared;
static uint32_t delays[2] = {TR_DEFAULT_OFF_DELAY_MS, TR_DEFAULT_OFF_DELAY_MS};
static uint64_t last_ack[2];
static uint32_t boot_session;
static bool started;
static bool building_count_known;
static size_t building_inside_count;

static uint64_t now_ms(void) {
    return (uint64_t)esp_timer_get_time() / 1000;
}
static void lock(void) {
    xSemaphoreTake(state_mutex, portMAX_DELAY);
}
static void unlock(void) {
    xSemaphoreGive(state_mutex);
}
static bool owns_room(unsigned i) {
    return TR_ROLE == 0 || TR_ROLE == (int)i + 2;
}
static uart_port_t coordinator_port(unsigned i) {
    return i == 0 ? UART_NUM_1 : UART_NUM_2;
}
static bool demo_link_up(void) {
    return TR_ROLE != 0 || gpio_get_level(32) != 0;
}

static bool send_frame(unsigned i, bool to_coordinator, const tr_message *m) {
    wire_frame frame;
    if (!tr_encode(m, frame.bytes))
        return false;
    bool sent;
    if (TR_ROLE == 0) {
        sent = demo_link_up() && xQueueSend(to_coordinator ? up[i] : down[i], &frame, 0) == pdTRUE;
    } else {
        uart_port_t port = TR_ROLE == 1 ? coordinator_port(i) : UART_NUM_1;
        sent = uart_write_bytes(port, frame.bytes, TR_FRAME_SIZE) == TR_FRAME_SIZE;
    }
    if (!sent) {
        lock();
        shared.transport_drops++;
        unlock();
    }
    return sent;
}
/* Each stream has exactly one owner. Partial UART reads and corrupt frames are
 * fed through a sliding parser; UART driver buffers decouple interrupt timing. */
static bool receive_frame(unsigned i, bool at_coordinator, tr_parser *parser, tr_message *out) {
    if (TR_ROLE == 0) {
        wire_frame frame;
        if (xQueueReceive(at_coordinator ? up[i] : down[i], &frame, 0) != pdTRUE)
            return false;
        if (!demo_link_up())
            return false;
        return tr_decode(frame.bytes, out);
    }
    uint8_t byte;
    uart_port_t port = TR_ROLE == 1 ? coordinator_port(i) : UART_NUM_1;
    /* One frame maximum per call; caller drains until no frame remains. */
    for (unsigned scanned = 0;
         scanned < TR_FRAME_SIZE * 4u && uart_read_bytes(port, &byte, 1, 0) == 1; scanned++) {
        if (tr_parser_push(parser, byte, out))
            return true;
    }
    return false;
}

static bool enqueue_storage(const tr_storage_event *event) {
    if (xQueueSend(storage_queue, event, 0) == pdTRUE)
        return true;
    lock();
    shared.storage_queue_drops++;
    unlock();
    return false;
}
#ifndef TR_TEST_STORAGE
__attribute__((weak)) bool tr_storage_append(const tr_storage_event *event) {
    (void)event;
    return false;
}
#endif
static void storage_task(void *arg) {
    (void)arg;
    tr_storage_event event;
    /* One task writes all events, so two room tasks cannot write together. */
    while (true) {
        if (xQueueReceive(storage_queue, &event, portMAX_DELAY) == pdTRUE &&
            !tr_storage_append(&event)) {
            lock();
            shared.storage_write_failures++;
            unlock();
        }
    }
}
bool tr_runtime_record_access(const void *data, size_t size) {
    if (!started || !data || !size || size > sizeof(((tr_storage_event *)0)->access_data))
        return false;
    tr_storage_event event = {
        .kind = TR_EVENT_ACCESS, .received_ms = now_ms(), .access_size = (uint8_t)size};
    memcpy(event.access_data, data, size);
    return enqueue_storage(&event);
}
bool tr_runtime_snapshot(tr_system_view *out) {
    if (!started || !out)
        return false;
    lock();
    uint64_t now = now_ms();
    *out = shared;
    for (unsigned i = 0; i < 2; i++) {
        if (TR_ROLE <= 1) {
            out->rooms[i] = (tr_room_view){.valid = receivers[i].valid,
                                           .online = tr_receiver_online(&receivers[i], now),
                                           .state = receivers[i].state,
                                           .last_seen_ms = receivers[i].last_seen_ms};
        } else if (owns_room(i)) {
            out->rooms[i].online = last_ack[i] != 0 && now - last_ack[i] < TR_STALE_MS;
        }
    }
    unlock();
    return true;
}
esp_err_t tr_runtime_set_off_delay(uint8_t id, uint32_t delay) {
    if (!started)
        return ESP_ERR_INVALID_STATE;
    if (id < 1 || id > 2 || delay > 3600000u)
        return ESP_ERR_INVALID_ARG;
    if (!owns_room(id - 1))
        return ESP_ERR_NOT_SUPPORTED;
    lock();
    delays[id - 1] = delay;
    unlock();
    return ESP_OK;
}
esp_err_t tr_runtime_set_building_inside_count(size_t count, bool authoritative) {
    if (!started)
        return ESP_ERR_INVALID_STATE;
    if (TR_ROLE != 0)
        return ESP_ERR_NOT_SUPPORTED;
    lock();
    building_inside_count = count;
    building_count_known = authoritative;
    unlock();
    return ESP_OK;
}

static void room_task(void *arg) {
    unsigned i = ((room_arg *)arg)->id - 1;
    tr_room room;
    tr_room_init(&room, (uint8_t)(i + 1), TR_DEFAULT_OFF_DELAY_MS);
    tr_sender sender = {0};
    tr_parser parser = {0};
    bool dirty = true;
    bool previous_effective_light = false;
    uint32_t sequence = 0;
    uint64_t last_snapshot = 0;
    /* Each room task reads its PIR and controls only its own light. */
    while (true) {
        uint64_t now = now_ms();
        lock();
        uint32_t delay = delays[i];
        unlock();
        if (room.off_delay_ms != delay) {
            tr_room_set_delay(&room, delay);
            dirty = true;
        }
        if (tr_room_sample(&room, gpio_get_level(motion_pin[i]) != 0, now)) {
            dirty = true;
            ESP_LOGI(TAG, "Room %u motion=%d light=%d", i + 1, room.occupied, room.light);
        }
        lock();
        bool effective_light = room.light && (!building_count_known || building_inside_count > 0);
        unlock();
        if (effective_light != previous_effective_light)
            dirty = true;
        previous_effective_light = effective_light;
        gpio_set_level(light_pin[i], effective_light);
        tr_message incoming;
        /* Bound processing so an input flood cannot starve local light control. */
        for (unsigned n = 0; n < 8 && receive_frame(i, false, &parser, &incoming); n++) {
            if (tr_sender_ack(&sender, &incoming)) {
                lock();
                last_ack[i] = now;
                unlock();
            }
        }
        if (!sender.waiting && (dirty || now - last_snapshot >= TR_HEARTBEAT_MS)) {
            if (++sequence == 0)
                sequence = 1;
            tr_message m = {.kind = TR_STATE,
                            .room = (uint8_t)(i + 1),
                            .session = boot_session,
                            .sequence = sequence,
                            .uptime_ms = now,
                            .occupied = room.occupied,
                            .light = effective_light,
                            .off_delay_ms = room.off_delay_ms};
            tr_sender_offer(&sender, &m);
            dirty = false;
            last_snapshot = now;
        }
        tr_message outgoing;
        if (tr_sender_poll(&sender, now, &outgoing))
            send_frame(i, true, &outgoing);
        lock();
        shared.retries[i] = sender.retries;
        shared.timeouts[i] = sender.timeouts;
        if (TR_ROLE >= 2) {
            shared.rooms[i].valid = true;
            shared.rooms[i].state = (tr_message){.kind = TR_STATE,
                                                 .room = (uint8_t)(i + 1),
                                                 .session = boot_session,
                                                 .sequence = sequence,
                                                 .uptime_ms = now,
                                                 .occupied = room.occupied,
                                                 .light = effective_light,
                                                 .off_delay_ms = room.off_delay_ms};
            shared.rooms[i].last_seen_ms = now;
        }
        unlock();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
static void coordinator_task(void *arg) {
    (void)arg;
    tr_parser parsers[2] = {0};
    /* The coordinator receives room updates, records them, and sends ACKs. */
    while (true) {
        for (unsigned i = 0; i < 2; i++) {
            tr_message m;
            for (unsigned n = 0; n < 8 && receive_frame(i, true, &parsers[i], &m); n++) {
                if (m.room != i + 1 || m.kind != TR_STATE)
                    continue;
                uint64_t now = now_ms();
                lock();
                enum tr_result result = tr_receiver_accept(&receivers[i], &m, now);
                if (result == TR_DUPLICATE)
                    shared.duplicates++;
                if (result == TR_STALE)
                    shared.stale_frames++;
                unlock();
                if (result == TR_NEW) {
                    tr_storage_event event = {
                        .kind = TR_EVENT_ROOM, .received_ms = now, .room_state = m};
                    enqueue_storage(&event);
                }
                if (result != TR_INVALID) {
                    m.kind = TR_ACK;
                    send_frame(i, false, &m);
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
static void status_task(void *arg) {
    (void)arg;
    while (true) {
        tr_system_view v;
        if (tr_runtime_snapshot(&v)) {
            for (unsigned i = 0; i < 2; i++) {
                if (TR_ROLE >= 2 && !owns_room(i))
                    continue;
                printf("ROOM %u valid=%d online=%d motion=%d light=%d delay=%" PRIu32
                       " seq=%" PRIu32 " retries=%" PRIu32 " timeouts=%" PRIu32 "\n",
                       i + 1, v.rooms[i].valid, v.rooms[i].online, v.rooms[i].state.occupied,
                       v.rooms[i].state.light, v.rooms[i].state.off_delay_ms,
                       v.rooms[i].state.sequence, v.retries[i], v.timeouts[i]);
            }
            printf("SYNC duplicate=%" PRIu32 " stale=%" PRIu32 " transport_drop=%" PRIu32
                   " storage_queue_drop=%" PRIu32 " storage_unwritten=%" PRIu32 "\n",
                   v.duplicates, v.stale_frames, v.transport_drops, v.storage_queue_drops,
                   v.storage_write_failures);
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
static esp_err_t init_uart(uart_port_t port, int tx, int rx) {
    uart_config_t config = {.baud_rate = 115200,
                            .data_bits = UART_DATA_8_BITS,
                            .parity = UART_PARITY_DISABLE,
                            .stop_bits = UART_STOP_BITS_1,
                            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
                            .source_clk = UART_SCLK_DEFAULT};
    esp_err_t err = uart_param_config(port, &config);
    if (err != ESP_OK)
        return err;
    err = uart_set_pin(port, tx, rx, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK)
        return err;
    return uart_driver_install(port, 512, 0, 0, NULL, 0);
}
static esp_err_t next_boot_session(void) {
    esp_err_t err = nvs_flash_init();
    /* Never erase shared NVS to recover: Edward may store configuration there. */
    if (err != ESP_OK)
        return err;
    nvs_handle_t handle;
    err = nvs_open("trevor_link", NVS_READWRITE, &handle);
    if (err != ESP_OK)
        return err;
    uint32_t previous = 0;
    err = nvs_get_u32(handle, "boot", &previous);
    if (err == ESP_ERR_NVS_NOT_FOUND)
        err = ESP_OK;
    if (err == ESP_OK) {
        boot_session = previous + 1;
        if (!boot_session)
            boot_session = 1;
        err = nvs_set_u32(handle, "boot", boot_session);
    }
    if (err == ESP_OK)
        err = nvs_commit(handle);
    nvs_close(handle);
    return err;
}
esp_err_t tr_runtime_start(void) {
    if (started)
        return ESP_ERR_INVALID_STATE;
    esp_err_t err = next_boot_session();
    if (err != ESP_OK)
        return err;
    state_mutex = xSemaphoreCreateMutex();
    storage_queue = xQueueCreate(16, sizeof(tr_storage_event));
    if (!state_mutex || !storage_queue)
        return ESP_ERR_NO_MEM;
    uint64_t inputs = 0, outputs = 0;
    for (unsigned i = 0; i < 2; i++)
        if (owns_room(i)) {
            inputs |= 1ULL << motion_pin[i];
            outputs |= 1ULL << light_pin[i];
        }
    if (inputs) {
        gpio_config_t c = {.pin_bit_mask = inputs,
                           .mode = GPIO_MODE_INPUT,
                           .pull_up_en = GPIO_PULLUP_DISABLE,
                           .pull_down_en = GPIO_PULLDOWN_ENABLE,
                           .intr_type = GPIO_INTR_DISABLE};
        err = gpio_config(&c);
        if (err != ESP_OK)
            return err;
        c = (gpio_config_t){
            .pin_bit_mask = outputs, .mode = GPIO_MODE_OUTPUT, .intr_type = GPIO_INTR_DISABLE};
        err = gpio_config(&c);
        if (err != ESP_OK)
            return err;
        for (unsigned i = 0; i < 2; i++)
            if (owns_room(i))
                gpio_set_level(light_pin[i], 0);
    }
    if (TR_ROLE == 0) {
        gpio_config_t c = {.pin_bit_mask = 1ULL << 32,
                           .mode = GPIO_MODE_INPUT,
                           .pull_up_en = GPIO_PULLUP_ENABLE,
                           .intr_type = GPIO_INTR_DISABLE};
        err = gpio_config(&c);
        if (err != ESP_OK)
            return err;
        for (unsigned i = 0; i < 2; i++) {
            up[i] = xQueueCreate(8, sizeof(wire_frame));
            down[i] = xQueueCreate(8, sizeof(wire_frame));
            if (!up[i] || !down[i])
                return ESP_ERR_NO_MEM;
        }
    } else {
        err = init_uart(UART_NUM_1, 17, 16);
        if (err != ESP_OK)
            return err;
        if (TR_ROLE == 1) {
            err = init_uart(UART_NUM_2, 19, 18);
            if (err != ESP_OK)
                return err;
        }
    }
    started = true;
    /* Failure after tasks begin is fatal; never leave a partially live runtime. */
    ESP_ERROR_CHECK(xTaskCreate(storage_task, "tr_store", 4096, NULL, 3, NULL) == pdPASS
                        ? ESP_OK
                        : ESP_ERR_NO_MEM);
    if (TR_ROLE <= 1)
        ESP_ERROR_CHECK(xTaskCreate(coordinator_task, "tr_coord", 4096, NULL, 4, NULL) == pdPASS
                            ? ESP_OK
                            : ESP_ERR_NO_MEM);
    for (unsigned i = 0; i < 2; i++)
        if (owns_room(i))
            ESP_ERROR_CHECK(xTaskCreate(room_task, "tr_room", 4096, &room_args[i], 4, NULL) ==
                                    pdPASS
                                ? ESP_OK
                                : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(xTaskCreate(status_task, "tr_status", 4096, NULL, 1, NULL) == pdPASS
                        ? ESP_OK
                        : ESP_ERR_NO_MEM);
    ESP_LOGI(TAG, "role=%d boot=%" PRIu32 " check storage_unwritten for persistence failures",
             TR_ROLE, boot_session);
    return ESP_OK;
}
