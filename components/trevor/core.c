#include "trevor/core.h"

void tr_room_init(tr_room *r, uint8_t id, uint32_t delay) {
    *r = (tr_room){.id = id, .off_delay_ms = delay};
}
void tr_room_set_delay(tr_room *r, uint32_t delay) {
    r->off_delay_ms = delay;
}
bool tr_room_sample(tr_room *r, bool motion, uint64_t now) {
    /* Motion turns on the light and restarts its off-delay timer. */
    bool occupied = r->occupied, light = r->light;
    r->occupied = motion;
    if (motion) {
        r->last_motion_ms = now;
        r->light = true;
    } else if (r->light && now - r->last_motion_ms >= r->off_delay_ms)
        r->light = false;
    return occupied != r->occupied || light != r->light;
}

/* Accumulate the time spent in the previous light state. */
void tr_usage_sample(tr_usage *usage, uint64_t now, bool light_on, bool light_suppressed) {
    if (!usage)
        return;
    if (usage->started && now >= usage->last_update_ms) {
        uint64_t elapsed = now - usage->last_update_ms;
        if (usage->light_was_on)
            usage->light_on_ms += elapsed;
        if (usage->light_was_suppressed)
            usage->empty_suppressed_ms += elapsed;
    }
    usage->last_update_ms = now;
    usage->started = true;
    usage->light_was_on = light_on;
    usage->light_was_suppressed = light_suppressed;
}
