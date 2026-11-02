#include "entrance/runtime.h"
#include "entrance/core.h"
#include "entrance/mfrc522.h"
#include "entrance/display.h"
#include "entrance/keypad.h"
#include "entrance/dashboard.h"
#include "auth/runtime.h"
#include "trevor/runtime.h"
#include "actuator/runtime.h"
#include "driver/gpio.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "lwip/apps/sntp.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define WIFI_CONNECTED_BIT BIT0

static const char *TAG = "entrance";
static entrance_state state;
static SemaphoreHandle_t state_mutex;
static EventGroupHandle_t wifi_events;
static bool started;
static volatile bool clock_ready;
static bool last_event_valid;
static char last_person[SYSTEM_NAME_MAX_BYTES];
static char last_action[8];
static char last_time[6];
static bool occupancy_uncertain;

static void wifi_handler(void *arg, esp_event_base_t base, int32_t id, void *data) {
    (void)arg;
    (void)data;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START)
        esp_wifi_connect();
    else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(wifi_events, WIFI_CONNECTED_BIT);
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP)
        xEventGroupSetBits(wifi_events, WIFI_CONNECTED_BIT);
}

static void clock_task(void *arg) {
    (void)arg;
    esp_err_t e = esp_netif_init();
    if (e != ESP_OK && e != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "network init: %s", esp_err_to_name(e));
        vTaskDelete(NULL);
    }
    e = esp_event_loop_create_default();
    if (e != ESP_OK && e != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "event loop: %s", esp_err_to_name(e));
        vTaskDelete(NULL);
    }
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_handler, NULL));
    wifi_events = xEventGroupCreate();
    if (!wifi_events) {
        ESP_LOGE(TAG, "could not allocate Wi-Fi event group");
        vTaskDelete(NULL);
    }
    wifi_config_t config = {
        .sta = {.ssid = "Wokwi-GUEST", .password = "", .threshold.authmode = WIFI_AUTH_OPEN}};
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &config));
    ESP_ERROR_CHECK(esp_wifi_start());
    EventBits_t bits;
    /* Entry stays closed until Wi-Fi time is available. */
    while (true) {
        bits = xEventGroupWaitBits(wifi_events, WIFI_CONNECTED_BIT, pdFALSE, pdFALSE,
                                   pdMS_TO_TICKS(20000));
        if (bits & WIFI_CONNECTED_BIT)
            break;
        ESP_LOGW(TAG, "waiting for Wokwi-GUEST; entry remains disabled until time is synchronized");
    }
    e = entrance_dashboard_start();
    if (e != ESP_OK)
        ESP_LOGE(TAG, "Wi-Fi dashboard unavailable: %s", esp_err_to_name(e));
    setenv("TZ", "EAT-3", 1);
    tzset();
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_init();
    while (true) {
        time_t now = time(NULL);
        if (now > 1760000000 && !clock_ready) {
            clock_ready = true;
            ESP_LOGI(TAG, "SNTP synchronized; timezone Africa/Kampala (UTC+03:00)");
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

static void save_event(system_access_event *event, const char *name, size_t inside_count) {
    /* Stable compact encoding: no compiler padding, UID bytes are the only credential material
     * logged. */
    /* Pack the fields one by one so the saved record has no compiler padding. */
    uint8_t bytes[40] = {0};
    bytes[0] = (uint8_t)(event->event_id);
    bytes[1] = (uint8_t)(event->event_id >> 8);
    bytes[2] = (uint8_t)(event->event_id >> 16);
    bytes[3] = (uint8_t)(event->event_id >> 24);
    bytes[4] = (uint8_t)(event->user_id);
    bytes[5] = (uint8_t)(event->user_id >> 8);
    bytes[6] = (uint8_t)(event->user_id >> 16);
    bytes[7] = (uint8_t)(event->user_id >> 24);
    for (unsigned i = 0; i < 8; i++) {
        bytes[8 + i] = (uint8_t)(event->utc_epoch_seconds >> (8 * i));
        bytes[16 + i] = (uint8_t)(event->monotonic_ms >> (8 * i));
    }
    bytes[24] = (uint8_t)event->point;
    bytes[25] = (uint8_t)event->result;
    bytes[26] = event->uid_size;
    memcpy(bytes + 27, event->uid, event->uid_size);
    if (!tr_runtime_record_access_durable(bytes, 27u + event->uid_size, name, inside_count))
        ESP_LOGE(TAG, "access event %" PRIu32 " was not confirmed on SD card", event->event_id);
}

static void load_schedule(void) {
    nvs_handle_t handle;
    if (nvs_open("access_cfg", NVS_READONLY, &handle) != ESP_OK)
        return;
    uint16_t start = 0, end = 0;
    esp_err_t a = nvs_get_u16(handle, "start", &start);
    esp_err_t b = nvs_get_u16(handle, "end", &end);
    nvs_close(handle);
    system_access_schedule saved = {start, end};
    if (a == ESP_OK && b == ESP_OK && entrance_schedule_valid(saved))
        state.schedule = saved;
}

void entrance_runtime_print_roll_call(bool after_hours) {
    system_user users[SYSTEM_MAX_USERS];
    bool inside[SYSTEM_MAX_USERS];
    size_t inside_count = 0;
    bool uncertain;

    if (!started)
        return;
    xSemaphoreTake(state_mutex, portMAX_DELAY);
    memcpy(users, state.users, sizeof(users));
    memcpy(inside, state.inside, sizeof(inside));
    uncertain = occupancy_uncertain;
    for (size_t i = 0; i < SYSTEM_MAX_USERS; i++)
        if (inside[i])
            inside_count++;
    xSemaphoreGive(state_mutex);

    time_t utc = time(NULL);
    struct tm local = {0};
    bool time_valid = clock_ready && utc > 1760000000 && localtime_r(&utc, &local) != NULL;
    ESP_LOGI(TAG, "%s",
             after_hours ? "AFTER-HOURS ALERT - PEOPLE STILL INSIDE" : "EVACUATION REPORT");
    if (time_valid)
        ESP_LOGI(TAG, "Time: %02d:%02d EAT", local.tm_hour, local.tm_min);
    else
        ESP_LOGI(TAG, "Time: clock not synchronized");
    ESP_LOGI(TAG, "Recorded inside: %u", (unsigned)inside_count);
    if (!inside_count)
        ESP_LOGI(TAG, "- none");
    for (size_t i = 0; i < SYSTEM_MAX_USERS; i++)
        if (inside[i])
            ESP_LOGI(TAG, "- %s",
                     users[i].display_name[0] ? users[i].display_name : "Unnamed user");
    ESP_LOGI(TAG, "Occupancy: %s%s", uncertain ? "UNCERTAIN" : "CONFIRMED",
             uncertain ? " - simulated tailgate needs staff check" : "");
}

void entrance_runtime_note_simulated_tailgate(void) {
    if (!started)
        return;
    xSemaphoreTake(state_mutex, portMAX_DELAY);
    occupancy_uncertain = true;
    xSemaphoreGive(state_mutex);
    ESP_LOGW(
        TAG,
        "SIMULATED TAILGATE ALERT: occupancy is uncertain; no person was added to the inside list");
}

esp_err_t entrance_runtime_confirm_empty(void) {
    if (!started)
        return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(state_mutex, portMAX_DELAY);
    if (entrance_inside_count(&state) != 0) {
        xSemaphoreGive(state_mutex);
        return ESP_ERR_INVALID_STATE;
    }
    occupancy_uncertain = false;
    xSemaphoreGive(state_mutex);
    ESP_LOGI(TAG, "administrator confirmed building empty after physical check");
    return ESP_OK;
}

static void schedule_watch_task(void *arg) {
    (void)arg;
    bool previous_open = false, initialized = false;
    while (true) {
        if (clock_ready) {
            time_t utc = time(NULL);
            struct tm local = {0};
            if (utc > 1760000000 && localtime_r(&utc, &local)) {
                system_access_schedule schedule;
                xSemaphoreTake(state_mutex, portMAX_DELAY);
                schedule = state.schedule;
                xSemaphoreGive(state_mutex);
                bool is_open =
                    entrance_is_open(schedule, (unsigned)(local.tm_hour * 60 + local.tm_min));
                if (initialized && previous_open && !is_open) {
                    size_t inside_count;
                    xSemaphoreTake(state_mutex, portMAX_DELAY);
                    inside_count = entrance_inside_count(&state);
                    xSemaphoreGive(state_mutex);
                    if (inside_count)
                        entrance_runtime_print_roll_call(true);
                    else
                        ESP_LOGI(TAG, "access window closed; building occupancy is zero");
                }
                previous_open = is_open;
                initialized = true;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(15000));
    }
}

static void process_scan(uint8_t reader, const uint8_t *uid, uint8_t uid_size) {
    uint64_t mono = (uint64_t)esp_timer_get_time() / 1000u;
    time_t utc = time(NULL);
    struct tm local = {0};
    bool valid = clock_ready && utc > 1760000000;
    if (valid)
        localtime_r(&utc, &local);
    system_access_point point = reader == 0 ? SYSTEM_ENTRY : SYSTEM_EXIT;
    uint32_t user_id, event_id;
    /* Protect the shared user list while deciding and recording the scan. */
    xSemaphoreTake(state_mutex, portMAX_DELAY);
    system_access_result result = entrance_apply_scan(
        &state, point, uid, uid_size, valid,
        valid ? (unsigned)(local.tm_hour * 60 + local.tm_min) : 0, &user_id, &event_id);
    system_access_event event = {.event_id = event_id,
                                 .user_id = user_id,
                                 .utc_epoch_seconds = valid ? (uint64_t)utc : 0,
                                 .monotonic_ms = mono,
                                 .point = point,
                                 .result = result,
                                 .uid_size = uid_size};
    memcpy(event.uid, uid, uid_size);
    size_t inside_count = entrance_inside_count(&state);
    char name[SYSTEM_NAME_MAX_BYTES];
    snprintf(name, sizeof(name), "%s",
             user_id < SYSTEM_MAX_USERS ? state.users[user_id].display_name : "unknown");
    bool granted = result == SYSTEM_ACCESS_GRANTED;
    last_event_valid = true;
    snprintf(last_person, sizeof(last_person), "%s", name);
    snprintf(last_action, sizeof(last_action), "%s",
             granted ? (point == SYSTEM_ENTRY ? "ENTER" : "LEFT") : "DENIED");
    if (valid)
        strftime(last_time, sizeof(last_time), "%H:%M", &local);
    else
        snprintf(last_time, sizeof(last_time), "--:--");
    /* Release the mutex before logging or writing to storage. */
    xSemaphoreGive(state_mutex);
    ESP_LOGI(TAG,
             "event=%" PRIu32 " utc=%" PRIu64
             " point=%s user=%s uid=%02X%02X%02X%02X result=%s inside=%u%s",
             event_id, event.utc_epoch_seconds, point == SYSTEM_ENTRY ? "entry" : "exit", name,
             uid[0], uid_size > 1 ? uid[1] : 0, uid_size > 2 ? uid[2] : 0,
             uid_size > 3 ? uid[3] : 0, granted ? "GRANTED" : "DENIED", (unsigned)inside_count,
             result == SYSTEM_ACCESS_SCHEDULE_CLOSED     ? " (schedule closed)"
             : result == SYSTEM_ACCESS_CLOCK_UNAVAILABLE ? " (clock unavailable)"
             : result == SYSTEM_ACCESS_ALREADY_INSIDE    ? " (already inside)"
             : result == SYSTEM_ACCESS_NOT_INSIDE        ? " (not inside)"
             : result == SYSTEM_ACCESS_UNKNOWN_USER      ? " (unknown user)"
                                                         : "");
    if (result == SYSTEM_ACCESS_ALREADY_INSIDE)
        ESP_LOGW(TAG, "ANTI-PASSBACK ALERT: this person is already recorded inside");
    else if (result == SYSTEM_ACCESS_NOT_INSIDE)
        ESP_LOGW(TAG, "EXIT WITHOUT ENTRY ALERT: this person is not recorded inside");
    save_event(&event, name, inside_count);
    actuator_runtime_signal(granted);
}

static void entrance_task(void *arg) {
    (void)arg;
    uint8_t last_uid[2][SYSTEM_UID_MAX_BYTES] = {{0}}, last_size[2] = {0};
    uint64_t last_seen[2] = {0};
    /* Poll both RFID readers and ignore a card held against a reader. */
    while (true) {
        uint64_t now = (uint64_t)esp_timer_get_time() / 1000u;
        actuator_runtime_update();
        for (uint8_t reader = 0; reader < 2; reader++) {
            uint8_t uid[SYSTEM_UID_MAX_BYTES], size = 0;
            if (mfrc522_poll(reader, uid, &size)) {
                bool duplicate = last_size[reader] == size &&
                                 memcmp(last_uid[reader], uid, size) == 0 &&
                                 now - last_seen[reader] < 1500u;
                last_seen[reader] = now;
                if (!duplicate) {
                    memcpy(last_uid[reader], uid, size);
                    last_size[reader] = size;
                    process_scan(reader, uid, size);
                }
            } else if (now - last_seen[reader] > 500u)
                last_size[reader] = 0;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

static void display_task(void *arg) {
    (void)arg;
    char rows[4][21] = {{0}}, previous[4][21] = {{0}};
    bool first_write = true;
    while (true) {
        if (!entrance_keypad_admin_view(rows)) {
            entrance_status status;
            if (entrance_runtime_status(&status)) {
                char start[6], end[6];
                memset(rows, ' ', sizeof(rows));
                snprintf(rows[0], 21, "CoCIS: %s",
                         !status.clock_valid ? "TIME SYNC..."
                         : status.is_open    ? "OPEN"
                                             : "CLOSED");
                unsigned start_minute = status.schedule.start_minute % 1440u;
                unsigned end_minute = status.schedule.end_minute % 1440u;
                snprintf(start, sizeof(start), "%02u:%02u", start_minute / 60u, start_minute % 60u);
                snprintf(end, sizeof(end), "%02u:%02u", end_minute / 60u, end_minute % 60u);
                snprintf(rows[1], 21, "Hours %.5s-%.5s", start, end);
                if (status.occupancy_uncertain) {
                    snprintf(rows[2], 21, "OCCUPANCY UNCERTAIN");
                    snprintf(rows[3], 21, "Recorded inside: %u", (unsigned)status.inside_count);
                } else if (status.clock_valid && !status.is_open && status.inside_count > 0) {
                    snprintf(rows[2], 21, "AFTER-HOURS ALERT");
                    snprintf(rows[3], 21, "Still inside: %u", (unsigned)status.inside_count);
                } else if (status.has_last_event) {
                    snprintf(rows[2], 21, "Last: %.7s %.5s", status.last_person,
                             status.last_action);
                    snprintf(rows[3], 21, "At %s | Inside %u", status.last_time,
                             (unsigned)status.inside_count);
                } else {
                    snprintf(rows[2], 21, "Waiting for RFID...");
                    snprintf(rows[3], 21, "Inside: %u", (unsigned)status.inside_count);
                }
            }
        }
        /* Avoid rewriting the LCD when its four lines have not changed. */
        if (first_write || memcmp(rows, previous, sizeof(rows)) != 0) {
            entrance_display_write((const char (*)[21])rows);
            memcpy(previous, rows, sizeof(rows));
            first_write = false;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

esp_err_t entrance_runtime_start(void) {
    if (started)
        return ESP_ERR_INVALID_STATE;
    state_mutex = xSemaphoreCreateMutex();
    if (!state_mutex)
        return ESP_ERR_NO_MEM;
    entrance_state_init(&state);
    /* The live inside list is not saved; require a physical empty-building check. */
    occupancy_uncertain = true;
    load_schedule();
    auth_runtime_init();
    esp_err_t e = mfrc522_init();
    if (e != ESP_OK)
        return e;
    e = entrance_display_init();
    if (e != ESP_OK)
        return e;
    started = true;
    if (xTaskCreate(clock_task, "entrance_clock", 4096, NULL, 3, NULL) != pdPASS)
        return ESP_ERR_NO_MEM;
    if (xTaskCreate(entrance_task, "entrance_rfid", 4096, NULL, 4, NULL) != pdPASS)
        return ESP_ERR_NO_MEM;
    if (xTaskCreate(schedule_watch_task, "entrance_schedule", 4096, NULL, 2, NULL) != pdPASS)
        return ESP_ERR_NO_MEM;
    if (xTaskCreate(display_task, "entrance_lcd", 4096, NULL, 2, NULL) != pdPASS)
        return ESP_ERR_NO_MEM;
    e = entrance_keypad_start();
    if (e != ESP_OK)
        return e;
    ESP_LOGI(TAG,
             "entrance ready; demo UIDs DE:AD:BE:EF and 11:22:33:44; schedule %02u:%02u-%02u:%02u "
             "Kampala time",
             state.schedule.start_minute / 60, state.schedule.start_minute % 60,
             state.schedule.end_minute / 60, state.schedule.end_minute % 60);
    return ESP_OK;
}

esp_err_t entrance_runtime_set_users(const system_user users[SYSTEM_MAX_USERS]) {
    if (!started)
        return ESP_ERR_INVALID_STATE;
    if (!users)
        return ESP_ERR_INVALID_ARG;

    for (size_t i = 0; i < SYSTEM_MAX_USERS; i++) {
        if (!users[i].enabled)
            continue;
        if (!users[i].uid_size || users[i].uid_size > SYSTEM_UID_MAX_BYTES ||
            !memchr(users[i].display_name, '\0', SYSTEM_NAME_MAX_BYTES))
            return ESP_ERR_INVALID_ARG;
        for (size_t j = 0; j < i; j++) {
            if (users[j].enabled && users[j].uid_size == users[i].uid_size &&
                memcmp(users[j].uid, users[i].uid, users[i].uid_size) == 0)
                return ESP_ERR_INVALID_ARG;
        }
    }

    xSemaphoreTake(state_mutex, portMAX_DELAY);
    for (size_t i = 0; i < SYSTEM_MAX_USERS; i++) {
        if (state.inside[i] &&
            (!users[i].enabled || users[i].uid_size != state.users[i].uid_size ||
             memcmp(users[i].uid, state.users[i].uid, state.users[i].uid_size) != 0)) {
            xSemaphoreGive(state_mutex);
            return ESP_ERR_INVALID_STATE;
        }
    }
    memcpy(state.users, users, sizeof(state.users));
    xSemaphoreGive(state_mutex);
    ESP_LOGI(TAG, "registered-user roster updated");
    return ESP_OK;
}

bool entrance_runtime_snapshot(system_user *users, bool *inside, size_t capacity,
                               size_t *user_count, size_t *inside_count,
                               system_access_schedule *schedule) {
    if (!started || !user_count || !inside_count || capacity < SYSTEM_MAX_USERS || !users ||
        !inside)
        return false;
    xSemaphoreTake(state_mutex, portMAX_DELAY);
    memcpy(users, state.users, sizeof(state.users));
    memcpy(inside, state.inside, sizeof(state.inside));
    *user_count = 0;
    *inside_count = 0;
    for (size_t i = 0; i < SYSTEM_MAX_USERS; i++) {
        if (state.users[i].enabled)
            (*user_count)++;
        if (state.inside[i])
            (*inside_count)++;
    }
    if (schedule)
        *schedule = state.schedule;
    xSemaphoreGive(state_mutex);
    return true;
}

esp_err_t entrance_runtime_set_schedule(system_access_schedule schedule) {
    if (!started)
        return ESP_ERR_INVALID_STATE;
    if (!entrance_schedule_valid(schedule))
        return ESP_ERR_INVALID_ARG;
    nvs_handle_t handle;
    esp_err_t e = nvs_open("access_cfg", NVS_READWRITE, &handle);
    if (e != ESP_OK)
        return e;
    e = nvs_set_u16(handle, "start", schedule.start_minute);
    if (e == ESP_OK)
        e = nvs_set_u16(handle, "end", schedule.end_minute);
    if (e == ESP_OK)
        e = nvs_commit(handle);
    nvs_close(handle);
    if (e != ESP_OK)
        return e;
    xSemaphoreTake(state_mutex, portMAX_DELAY);
    system_access_schedule previous = state.schedule;
    state.schedule = schedule;
    xSemaphoreGive(state_mutex);
    ESP_LOGI(TAG, "access schedule saved: %02u:%02u-%02u:%02u", schedule.start_minute / 60,
             schedule.start_minute % 60, schedule.end_minute / 60, schedule.end_minute % 60);
    time_t utc = time(NULL);
    uint64_t logged_utc = clock_ready && utc > 1760000000 ? (uint64_t)utc : 0;
    if (!tr_runtime_record_schedule_change(previous, schedule, logged_utc))
        ESP_LOGW(TAG, "schedule is saved in NVS, but its SD audit row was not confirmed");
    return ESP_OK;
}

bool entrance_runtime_status(entrance_status *out) {
    if (!started || !out)
        return false;
    xSemaphoreTake(state_mutex, portMAX_DELAY);
    memset(out, 0, sizeof(*out));
    out->clock_valid = clock_ready;
    out->schedule = state.schedule;
    out->inside_count = entrance_inside_count(&state);
    out->has_last_event = last_event_valid;
    out->occupancy_uncertain = occupancy_uncertain;
    snprintf(out->last_person, sizeof(out->last_person), "%s", last_person);
    snprintf(out->last_action, sizeof(out->last_action), "%s", last_action);
    snprintf(out->last_time, sizeof(out->last_time), "%s", last_time);
    xSemaphoreGive(state_mutex);
    if (out->clock_valid) {
        time_t utc = time(NULL);
        struct tm local = {0};
        if (utc > 1760000000 && localtime_r(&utc, &local)) {
            out->local_minute = (unsigned)(local.tm_hour * 60 + local.tm_min);
            out->is_open = entrance_is_open(out->schedule, out->local_minute);
        } else
            out->clock_valid = false;
    }
    return true;
}
