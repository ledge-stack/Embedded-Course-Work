#ifndef TREVOR_RUNTIME_H
#define TREVOR_RUNTIME_H
#include "esp_err.h"
#include "trevor/core.h"
#include "common/system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TR_DEFAULT_OFF_DELAY_MS 5000u

typedef struct {
    bool valid;
    tr_room_state state;
    uint64_t last_seen_ms;
} tr_room_view;
typedef struct {
    tr_room_view rooms[2];
    /* Runtime totals since boot; no watts are assumed, so these are not kWh. */
    uint64_t light_on_ms[2];
    uint64_t empty_building_saved_ms[2];
    uint32_t storage_queue_drops, storage_write_failures;
} tr_system_view;
enum tr_event_kind { TR_EVENT_ROOM = 1, TR_EVENT_ACCESS = 2, TR_EVENT_SCHEDULE = 3 };
typedef struct {
    enum tr_event_kind kind;
    uint64_t received_ms;
    tr_room_state room_state;
    /* Compact access record supplied by the entrance module. */
    uint8_t access_size;
    uint8_t access_data[48];
    char access_name[SYSTEM_NAME_MAX_BYTES];
    uint32_t inside_count;
    uint64_t schedule_utc_seconds;
    system_access_schedule old_schedule, new_schedule;
    TaskHandle_t completion_task;
    uint32_t request_id;
} tr_storage_event;

esp_err_t tr_runtime_start(void);
bool tr_runtime_snapshot(tr_system_view *out);
/* Both rooms are local tasks on the same ESP32. */
esp_err_t tr_runtime_set_off_delay(uint8_t room, uint32_t delay_ms);
/* Entrance coordinator is authoritative. Unknown count keeps room behavior local. */
esp_err_t tr_runtime_set_building_inside_count(size_t count, bool authoritative);
/* Nonblocking event handoff used by room tasks. True means queued, not written. */
bool tr_runtime_record_access(const void *data, size_t length);
/* Access and schedule records wait briefly for the SD worker's write result. */
bool tr_runtime_record_access_durable(const void *data, size_t length, const char *name,
                                      size_t inside_count);
bool tr_runtime_record_schedule_change(system_access_schedule before,
                                       system_access_schedule after, uint64_t utc_seconds);
/* The storage component supplies a strong implementation for the combined
 * application. Exactly one storage task invokes it. Return true only when
 * durably written. */
bool tr_storage_append(const tr_storage_event *event);
#endif
