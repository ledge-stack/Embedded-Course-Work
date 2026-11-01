#include "trevor/core.h"
#include <string.h>

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

static void put32(uint8_t *p, uint32_t v) {
    for (unsigned i = 0; i < 4; i++)
        p[i] = (uint8_t)(v >> (8 * i));
}
static uint32_t get32(const uint8_t *p) {
    uint32_t v = 0;
    for (unsigned i = 0; i < 4; i++)
        v |= (uint32_t)p[i] << (8 * i);
    return v;
}
static uint16_t crc16(const uint8_t *p, size_t n) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < n; i++) {
        crc ^= (uint16_t)p[i] << 8;
        for (unsigned j = 0; j < 8; j++)
            crc = (uint16_t)((crc & 0x8000) ? (crc << 1) ^ 0x1021 : crc << 1);
    }
    return crc;
}

/* Check that a decoded room message has sensible fields before using it. */
static bool valid(const tr_message *m) {
    return m && (m->kind == TR_STATE || m->kind == TR_ACK) && m->room >= 1 && m->room <= 2 &&
           m->session != 0 && m->sequence != 0 && m->off_delay_ms <= 3600000u;
}
bool tr_encode(const tr_message *m, uint8_t f[TR_FRAME_SIZE]) {
    if (!valid(m) || !f)
        return false;
    /* Write fields in a fixed order so different boards share the same format. */
    memset(f, 0, TR_FRAME_SIZE);
    f[0] = 0xC0;
    f[1] = 0xC1;
    f[2] = 1;
    f[3] = m->kind;
    f[4] = m->room;
    f[5] = (m->occupied ? 1 : 0) | (m->light ? 2 : 0);
    put32(f + 6, m->session);
    put32(f + 10, m->sequence);
    put32(f + 14, (uint32_t)m->uptime_ms);
    put32(f + 18, (uint32_t)(m->uptime_ms >> 32));
    put32(f + 22, m->off_delay_ms);
    uint16_t crc = crc16(f, 26);
    f[26] = (uint8_t)crc;
    f[27] = (uint8_t)(crc >> 8);
    return true;
}
bool tr_decode(const uint8_t f[TR_FRAME_SIZE], tr_message *m) {
    if (!f || !m || f[0] != 0xC0 || f[1] != 0xC1 || f[2] != 1 || (f[5] & ~3u))
        return false;
    /* Reject damaged frames before copying their data into the message. */
    if (crc16(f, 26) != ((uint16_t)f[26] | ((uint16_t)f[27] << 8)))
        return false;
    tr_message decoded = {.kind = f[3],
                          .room = f[4],
                          .occupied = (f[5] & 1) != 0,
                          .light = (f[5] & 2) != 0,
                          .session = get32(f + 6),
                          .sequence = get32(f + 10),
                          .uptime_ms = (uint64_t)get32(f + 14) | ((uint64_t)get32(f + 18) << 32),
                          .off_delay_ms = get32(f + 22)};
    if (!valid(&decoded))
        return false;
    *m = decoded;
    return true;
}
bool tr_parser_push(tr_parser *p, uint8_t byte, tr_message *m) {
    p->bytes[p->used++] = byte;
    if (p->used < TR_FRAME_SIZE)
        return false;
    if (tr_decode(p->bytes, m)) {
        p->used = 0;
        return true;
    }
    memmove(p->bytes, p->bytes + 1, TR_FRAME_SIZE - 1);
    p->used = TR_FRAME_SIZE - 1;
    return false;
}

bool tr_sender_offer(tr_sender *s, const tr_message *m) {
    if (s->waiting || !valid(m) || m->kind != TR_STATE)
        return false;
    s->pending = *m;
    s->waiting = true;
    s->sent = false;
    s->attempts = 0;
    return true;
}
bool tr_sender_poll(tr_sender *s, uint64_t now, tr_message *m) {
    if (!s->waiting || (s->sent && now - s->last_send_ms < TR_RETRY_MS))
        return false;
    /* Stop retrying after the configured maximum number of sends. */
    if (s->attempts >= TR_MAX_ATTEMPTS) {
        s->waiting = false;
        s->timeouts++;
        return false;
    }
    if (s->sent)
        s->retries++;
    s->attempts++;
    s->sent = true;
    s->last_send_ms = now;
    *m = s->pending;
    return true;
}
bool tr_sender_ack(tr_sender *s, const tr_message *m) {
    if (!s->waiting || m->kind != TR_ACK || m->room != s->pending.room ||
        m->session != s->pending.session || m->sequence != s->pending.sequence)
        return false;
    s->waiting = false;
    return true;
}
static bool newer(uint32_t a, uint32_t b) {
    uint32_t d = a - b;
    return d != 0 && d < 0x80000000u;
}
enum tr_result tr_receiver_accept(tr_receiver *r, const tr_message *m, uint64_t now) {
    if (!valid(m) || m->kind != TR_STATE || (r->valid && m->room != r->state.room))
        return TR_INVALID;
    if (r->valid) {
        if (m->session == r->state.session && m->sequence == r->state.sequence) {
            r->last_seen_ms = now;
            return TR_DUPLICATE;
        }
        if (m->session == r->state.session) {
            if (!newer(m->sequence, r->state.sequence))
                return TR_STALE;
        } else if (!newer(m->session, r->state.session))
            return TR_STALE;
    }
    r->valid = true;
    r->state = *m;
    r->last_seen_ms = now;
    return TR_NEW;
}
bool tr_receiver_online(const tr_receiver *r, uint64_t now) {
    return r->valid && now - r->last_seen_ms < TR_STALE_MS;
}
