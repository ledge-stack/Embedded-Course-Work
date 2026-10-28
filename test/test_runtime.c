/* Runs the actual ESP-IDF adapter against Windows threads/queues/GPIO stubs. */
#include "trevor/runtime.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

static LONG writing, stored_access, stored_rooms, producer_done;
static volatile LONG storage_ok=1;
bool tr_storage_append(const tr_storage_event *e) {
    assert(InterlockedIncrement(&writing)==1); /* callbacks never overlap */
    Sleep(1);
    if (e->kind==TR_EVENT_ACCESS) InterlockedIncrement(&stored_access);
    if (e->kind==TR_EVENT_ROOM) InterlockedIncrement(&stored_rooms);
    InterlockedDecrement(&writing);
    return storage_ok!=0;
}
static void producer(void *arg) {
    const char *text=arg;
    for(int i=0;i<100;i++) {
        while(!tr_runtime_record_access(text,strlen(text))) Sleep(1);
    }
    InterlockedIncrement(&producer_done);
}
static tr_system_view view(void) {
    tr_system_view v; assert(tr_runtime_snapshot(&v)); return v;
}
static void wait_lights(bool one, bool two) {
    for(int i=0;i<300;i++) {
        tr_system_view v=view();
        if(v.rooms[0].valid && v.rooms[1].valid &&
           v.rooms[0].state.light==one && v.rooms[1].state.light==two) return;
        Sleep(10);
    }
    assert(!"timed out waiting for coordinator light snapshot");
}
int main(void) {
    assert(!tr_runtime_snapshot(NULL));
    assert(tr_runtime_start()==ESP_OK);
    assert(tr_runtime_start()==ESP_ERR_INVALID_STATE);
    assert(tr_runtime_set_off_delay(0,100)==ESP_ERR_INVALID_ARG);
    assert(tr_runtime_set_off_delay(1,3600001)==ESP_ERR_INVALID_ARG);
    assert(!tr_runtime_record_access(NULL,1));
    assert(!tr_runtime_record_access("x",49));
    assert(tr_runtime_set_off_delay(1,150)==ESP_OK);
    assert(tr_runtime_set_off_delay(2,150)==ESP_OK);
    stub_gpio_input(27,1); stub_gpio_input(33,1);
    wait_lights(true,true);
    assert(gpio_get_level(26) && gpio_get_level(25));
    stub_gpio_input(27,0); stub_gpio_input(33,0);
    wait_lights(false,false);
    puts("PASS concurrent room control and off-delay through coordinator frames");

    stub_gpio_input(32,0); /* Disconnect both logical links. */
    stub_gpio_input(27,1);
    Sleep(TR_STALE_MS+250);
    tr_system_view v=view();
    assert(!v.rooms[0].online && !v.rooms[1].online);
    assert(gpio_get_level(26)); /* local control survives lost link */
    assert(v.retries[0]>0 && v.transport_drops>0);
    stub_gpio_input(32,1);
    wait_lights(true,false);
    Sleep(100);
    v=view(); assert(v.rooms[0].online && v.rooms[1].online);
    puts("PASS outage, stale status, local light autonomy and reconnect snapshot");

    xTaskCreate(producer,"p1",4096,"entry",2,NULL);
    xTaskCreate(producer,"p2",4096,"exit",2,NULL);
    for(int i=0;i<500 && (producer_done!=2 || stored_access!=200);i++) Sleep(10);
    assert(producer_done==2 && stored_access==200 && stored_rooms>=4);
    v=view(); assert(v.storage_queue_drops>0);
    storage_ok=0;
    assert(tr_runtime_record_access("test",4));
    Sleep(100);
    assert(view().storage_write_failures>0);
    puts("PASS two concurrent producers, serialized storage, queue pressure, storage failure");
    puts("All runtime integration tests passed.");
    return 0;
}
