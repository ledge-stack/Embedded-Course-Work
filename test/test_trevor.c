#include "trevor/core.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void room_timing(void) {
    tr_room r;
    tr_room_init(&r, 1, 1000);
    assert(!r.light && !r.occupied);
    assert(tr_room_sample(&r, true, 50));
    assert(r.light && r.occupied);
    tr_room_sample(&r, false, 100);
    assert(!r.occupied && r.light);
    tr_room_sample(&r, false, 1049);
    assert(r.light);
    assert(tr_room_sample(&r, false, 1050));
    assert(!r.light);
    tr_room_sample(&r, true, 2000);
    tr_room_sample(&r, true, 2500);
    tr_room_sample(&r, false, 3499);
    assert(r.light);
    tr_room_sample(&r, false, 3500);
    assert(!r.light);
    tr_room_set_delay(&r, 0);
    tr_room_sample(&r, true, 4000);
    assert(r.light);
    tr_room_sample(&r, false, 4001);
    assert(!r.light);
    puts("PASS light delay, boundary, renewed motion, zero delay");
}

static void usage_accounting(void) {
    tr_usage usage = {0};
    tr_usage_sample(NULL, 0, false, false);
    tr_usage_sample(&usage, 100, false, false);
    tr_usage_sample(&usage, 350, true, false);
    tr_usage_sample(&usage, 700, false, true);
    tr_usage_sample(&usage, 900, false, true);
    tr_usage_sample(&usage, 1000, true, false);
    tr_usage_sample(&usage, 1100, false, false);
    assert(usage.light_on_ms == 450);
    assert(usage.empty_suppressed_ms == 300);
    puts("PASS room light-use and empty-building time accounting");
}

int main(void) {
    room_timing();
    usage_accounting();
    puts("All room core tests passed.");
    return 0;
}
