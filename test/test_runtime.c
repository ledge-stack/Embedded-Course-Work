/* Runs the actual ESP-IDF adapter against Windows threads/queues/GPIO stubs. */
#include "trevor/runtime.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

static LONG writing, stored_access, stored_rooms, producer_done;
static volatile LONG storage_ok = 1;
static volatile LONG storage_delay = 1;
bool tr_storage_append(const tr_storage_event *e) {
    assert(InterlockedIncrement(&writing) == 1); /* callbacks never overlap */
    Sleep(storage_delay);
    if (e->kind == TR_EVENT_ACCESS)
        InterlockedIncrement(&stored_access);
    if (e->kind == TR_EVENT_ROOM)
        InterlockedIncrement(&stored_rooms);
    InterlockedDecrement(&writing);
    return storage_ok != 0;
}
static void producer(void *arg) {
    const char *text = arg;
    for (int i = 0; i < 100; i++) {
        while (!tr_runtime_record_access(text, strlen(text)))
            Sleep(1);
    }
    InterlockedIncrement(&producer_done);
}
static tr_system_view view(void) {
    tr_system_view v;
    assert(tr_runtime_snapshot(&v));
    return v;
}
static void wait_lights(bool one, bool two) {
    for (int i = 0; i < 300; i++) {
        tr_system_view v = view();
        if (v.rooms[0].valid && v.rooms[1].valid && v.rooms[0].state.light == one &&
            v.rooms[1].state.light == two)
            return;
        Sleep(10);
    }
    assert(!"timed out waiting for local light snapshot");
}
int main(void) {
    assert(!tr_runtime_snapshot(NULL));
    assert(tr_runtime_start() == ESP_OK);
    assert(tr_runtime_start() == ESP_ERR_INVALID_STATE);
    assert(tr_runtime_set_off_delay(0, 100) == ESP_ERR_INVALID_ARG);
    assert(tr_runtime_set_off_delay(1, 3600001) == ESP_ERR_INVALID_ARG);
    assert(!tr_runtime_record_access(NULL, 1));
    assert(!tr_runtime_record_access("x", 49));
    assert(tr_runtime_set_off_delay(1, 150) == ESP_OK);
    assert(tr_runtime_set_off_delay(2, 150) == ESP_OK);
    stub_gpio_input(27, 1);
    stub_gpio_input(33, 1);
    wait_lights(true, true);
    assert(gpio_get_level(26) && gpio_get_level(25));
    stub_gpio_input(27, 0);
    stub_gpio_input(33, 0);
    wait_lights(false, false);
    puts("PASS concurrent room control and off-delay through local snapshots");

    /* GPIO32 is no longer a link switch. Local snapshots need no UART/ACK. */
    stub_gpio_input(32, 0);
    stub_gpio_input(27, 1);
    wait_lights(true, false);
    tr_system_view v = view();
    assert(v.rooms[0].state.occupied && gpio_get_level(26));
    puts("PASS direct room snapshots without a simulated communication link");

    assert(tr_runtime_set_building_inside_count(0, true) == ESP_OK);
    Sleep(100);
    v = view();
    assert(!gpio_get_level(26));
    assert(v.empty_building_saved_ms[0] > 0);
    assert(tr_runtime_set_building_inside_count(1, true) == ESP_OK);
    wait_lights(true, false);
    Sleep(100);
    v = view();
    assert(v.light_on_ms[0] > 0);
    assert(tr_runtime_set_building_inside_count(0, true) == ESP_OK);
    puts("PASS room light usage and empty-building suppressed-on-time counters");

    xTaskCreate(producer, "p1", 4096, "entry", 2, NULL);
    xTaskCreate(producer, "p2", 4096, "exit", 2, NULL);
    for (int i = 0; i < 500 && (producer_done != 2 || stored_access != 200); i++)
        Sleep(10);
    assert(producer_done == 2 && stored_access == 200 && stored_rooms >= 4);
    v = view();
    assert(v.storage_queue_drops > 0);
    storage_ok = 0;
    assert(tr_runtime_record_access("test", 4));
    Sleep(100);
    assert(view().storage_write_failures > 0);
    puts("PASS two concurrent producers, serialized storage, queue pressure, storage failure");
    assert(!tr_runtime_record_access_durable("test", 4, "name", 1));
    storage_ok = 1;
    assert(tr_runtime_record_access_durable("test", 4, "name", 1));
    system_access_schedule before = {480, 1080}, after = {480, 1380};
    assert(tr_runtime_record_schedule_change(before, after, 1760000001));
    puts("PASS actual task-notification completion for access and schedule records");
    storage_delay = 5200;
    assert(!tr_runtime_record_access_durable("slow", 4, "name", 1));
    storage_delay = 1;
    assert(tr_runtime_record_access_durable("next", 4, "name", 1));
    puts("PASS timed-out write's late reply cannot complete the next request");
    puts("All runtime integration tests passed.");
    return 0;
}
