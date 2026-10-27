#ifndef TREVOR_RUNTIME_H
#define TREVOR_RUNTIME_H
#include "esp_err.h"
#include "trevor/core.h"

/* TR_ROLE: 0 = single-chip logical-terminal demo; 1 = physical coordinator;
 * 2 = physical room 1; 3 = physical room 2. UART0 remains the serial console. */
#ifndef TR_ROLE
#define TR_ROLE 0
#endif
#define TR_DEFAULT_OFF_DELAY_MS 5000u

typedef struct {
    bool valid, online;
    tr_message state;
    uint64_t last_seen_ms;
} tr_room_view;
typedef struct {
    tr_room_view rooms[2];
    uint32_t retries[2];
    uint32_t timeouts[2];
    uint32_t duplicates, stale_frames, transport_drops, storage_queue_drops, storage_write_failures;
} tr_system_view;
enum tr_event_kind { TR_EVENT_ROOM=1, TR_EVENT_ACCESS=2 };
typedef struct {
    enum tr_event_kind kind;
    uint64_t received_ms;
    tr_message room_state;
    /* Opaque access event data supplied by entrance module; no credentials. */
    uint8_t access_size;
    uint8_t access_data[48];
} tr_storage_event;

esp_err_t tr_runtime_start(void);
bool tr_runtime_snapshot(tr_system_view *out);
/* Local room configuration only: call on the relevant room terminal. Demo
 * hosts both rooms. Physical coordinator returns ESP_ERR_NOT_SUPPORTED. */
esp_err_t tr_runtime_set_off_delay(uint8_t room, uint32_t delay_ms);
/* Thread-safe, nonblocking admission. False means caller must retry/report loss.
 * True means queued, NOT durably persisted. Requires runtime_start first. */
bool tr_runtime_record_access(const void *data, size_t length);
/* Edward overrides this weak hook; exactly one storage task invokes it.
 * Return true only when stored. Default returns false (storage unavailable).
 * Other storage operations must be routed through the same owner task. */
bool tr_storage_append(const tr_storage_event *event);
#endif
