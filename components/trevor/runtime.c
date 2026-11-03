#include "trevor/runtime.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "rooms";
static const int motion_pin[2] = {27, 33}, light_pin[2] = {26, 25};
static QueueHandle_t storage_queue;
static SemaphoreHandle_t state_mutex;
static tr_system_view shared;
static uint32_t delays[2] = {5000, 5000}, storage_request_id;
static bool started, building_count_known;
static size_t building_inside_count;

static uint64_t now_ms(void) { return (uint64_t)esp_timer_get_time() / 1000; }
static void lock(void) { xSemaphoreTake(state_mutex, portMAX_DELAY); }
static void unlock(void) { xSemaphoreGive(state_mutex); }

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
        if (xQueueReceive(storage_queue, &event, portMAX_DELAY) == pdTRUE) {
            bool saved = tr_storage_append(&event);
            if (!saved) {
                lock();
                shared.storage_write_failures++;
                unlock();
            }
            if (event.completion_task) {
                uint32_t reply = (event.request_id << 1) | (saved ? 1u : 0u);
                (void)xTaskNotify(event.completion_task, reply, eSetValueWithOverwrite);
            }
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

static bool enqueue_storage_and_wait(tr_storage_event *event) {
    if (!started || !event)
        return false;
    /* Discard an old completion before assigning this request a new ID. */
    uint32_t ignored;
    (void)xTaskNotifyWait(0, UINT32_MAX, &ignored, 0);
    lock();
    storage_request_id = (storage_request_id + 1u) & 0x7fffffffu;
    if (!storage_request_id)
        storage_request_id = 1;
    event->request_id = storage_request_id;
    unlock();
    event->completion_task = xTaskGetCurrentTaskHandle();
    if (!enqueue_storage(event))
        return false;

    TickType_t started_at = xTaskGetTickCount();
    const TickType_t timeout = pdMS_TO_TICKS(5000);
    while (true) {
        TickType_t elapsed = xTaskGetTickCount() - started_at;
        if (elapsed >= timeout)
            return false;
        uint32_t reply = 0;
        if (xTaskNotifyWait(0, UINT32_MAX, &reply, timeout - elapsed) != pdTRUE)
            return false;
        if ((reply >> 1) == event->request_id)
            return (reply & 1u) != 0;
        /* A timed-out earlier write may finish late; ignore its reply. */
    }
}

bool tr_runtime_record_access_durable(const void *data, size_t size, const char *name,
                                      size_t inside_count) {
    if (!data || !size || size > sizeof(((tr_storage_event *)0)->access_data))
        return false;
    tr_storage_event event = {.kind = TR_EVENT_ACCESS,
                              .received_ms = now_ms(),
                              .access_size = (uint8_t)size,
                              .inside_count = (uint32_t)inside_count};
    memcpy(event.access_data, data, size);
    if (name)
        snprintf(event.access_name, sizeof(event.access_name), "%s", name);
    return enqueue_storage_and_wait(&event);
}

bool tr_runtime_record_schedule_change(system_access_schedule before,
                                       system_access_schedule after, uint64_t utc_seconds) {
    tr_storage_event event = {.kind = TR_EVENT_SCHEDULE,
                              .received_ms = now_ms(),
                              .schedule_utc_seconds = utc_seconds,
                              .old_schedule = before,
                              .new_schedule = after};
    return enqueue_storage_and_wait(&event);
}
bool tr_runtime_snapshot(tr_system_view *out) {
    if (!started || !out) return false;
    lock();
    *out = shared;
    unlock();
    return true;
}

esp_err_t tr_runtime_set_off_delay(uint8_t room, uint32_t delay) {
    if (!started) return ESP_ERR_INVALID_STATE;
    if (room < 1 || room > 2 || delay > 3600000u) return ESP_ERR_INVALID_ARG;
    lock();
    delays[room - 1] = delay;
    unlock();
    return ESP_OK;
}

esp_err_t tr_runtime_set_building_inside_count(size_t count, bool authoritative) {
    if (!started) return ESP_ERR_INVALID_STATE;
    lock();
    building_inside_count = count;
    building_count_known = authoritative;
    unlock();
    return ESP_OK;
}

static void room_task(void *arg) {
    unsigned i = (unsigned)(uintptr_t)arg;
    tr_room room;
    tr_room_init(&room, (uint8_t)(i + 1), TR_DEFAULT_OFF_DELAY_MS);
    tr_usage usage = {0};
    tr_room_state previous = {0};
    bool first = true;
    while (true) {
        uint64_t now = now_ms();
        lock();
        uint32_t delay = delays[i];
        bool known = building_count_known;
        size_t count = building_inside_count;
        unlock();
        tr_room_set_delay(&room, delay);
        tr_room_sample(&room, gpio_get_level(motion_pin[i]) != 0, now);
        bool light = room.light && (!known || count > 0);
        tr_usage_sample(&usage, now, light, room.light && known && count == 0);
        ESP_ERROR_CHECK(gpio_set_level(light_pin[i], light));
        tr_room_state current = {.room = (uint8_t)(i + 1), .occupied = room.occupied,
                                 .light = light, .off_delay_ms = delay, .uptime_ms = now};
        lock();
        shared.rooms[i] = (tr_room_view){.valid = true, .state = current, .last_seen_ms = now};
        shared.light_on_ms[i] = usage.light_on_ms;
        shared.empty_building_saved_ms[i] = usage.empty_suppressed_ms;
        unlock();
        if (first || current.occupied != previous.occupied || current.light != previous.light ||
            current.off_delay_ms != previous.off_delay_ms) {
            tr_storage_event event = {.kind = TR_EVENT_ROOM, .received_ms = now, .room_state = current};
            enqueue_storage(&event);
            ESP_LOGI(TAG, "Room %u motion=%d light=%d", i + 1, current.occupied, current.light);
            previous = current;
            first = false;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

esp_err_t tr_runtime_start(void) {
    if (started) return ESP_ERR_INVALID_STATE;
    state_mutex = xSemaphoreCreateMutex();
    storage_queue = xQueueCreate(16, sizeof(tr_storage_event));
    if (!state_mutex || !storage_queue) return ESP_ERR_NO_MEM;
    gpio_config_t pins = {.pin_bit_mask = (1ULL << 27) | (1ULL << 33),
                          .mode = GPIO_MODE_INPUT, .pull_down_en = GPIO_PULLDOWN_ENABLE,
                          .intr_type = GPIO_INTR_DISABLE};
    ESP_ERROR_CHECK(gpio_config(&pins));
    pins = (gpio_config_t){.pin_bit_mask = (1ULL << 26) | (1ULL << 25),
                           .mode = GPIO_MODE_OUTPUT, .intr_type = GPIO_INTR_DISABLE};
    ESP_ERROR_CHECK(gpio_config(&pins));
    for (unsigned i = 0; i < 2; i++) ESP_ERROR_CHECK(gpio_set_level(light_pin[i], 0));
    started = true;
    ESP_ERROR_CHECK(xTaskCreate(storage_task, "event_writer", 4096, NULL, 3, NULL) == pdPASS
                    ? ESP_OK : ESP_ERR_NO_MEM);
    for (unsigned i = 0; i < 2; i++)
        ESP_ERROR_CHECK(xTaskCreate(room_task, i == 0 ? "Room1" : "Room2", 4096,
                                    (void *)(uintptr_t)i, 4, NULL) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_LOGI(TAG, "two local room tasks; one event queue and SD writer");
    return ESP_OK;
}
