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

static tr_message state(uint32_t boot, uint32_t seq) {
    return (tr_message){.kind=TR_STATE,.room=1,.session=boot,.sequence=seq,
        .uptime_ms=123456789012ULL,.occupied=true,.light=true,.off_delay_ms=1000};
}

static void protocol(void) {
    uint8_t wire[TR_FRAME_SIZE]; tr_message a=state(1,1), b;
    assert(tr_encode(&a, wire));
    assert(tr_decode(wire, &b));
    assert(b.session==1 && b.sequence==1 && b.room==1 && b.light && b.occupied);
    assert(b.uptime_ms==a.uptime_ms && b.off_delay_ms==1000);
    for (unsigned i=0;i<TR_FRAME_SIZE;i++) {
        wire[i]^=1; assert(!tr_decode(wire,&b)); wire[i]^=1;
    }
    a.room=3; assert(!tr_encode(&a,wire));
    a=state(0,1); assert(!tr_encode(&a,wire));
    a=state(1,0); assert(!tr_encode(&a,wire));
    a=state(1,1); a.kind=99; assert(!tr_encode(&a,wire));
    a=state(1,1); assert(tr_encode(&a,wire));
    tr_parser p={0};
    for(unsigned i=0;i<17;i++) assert(!tr_parser_push(&p,0xAB,&b));
    for(unsigned i=0;i<TR_FRAME_SIZE-3;i++) assert(!tr_parser_push(&p,wire[i],&b));
    bool got=false;
    for(unsigned i=0;i<TR_FRAME_SIZE;i++) got=tr_parser_push(&p,wire[i],&b)||got;
    assert(got && b.sequence==1);
    for(unsigned i=0;i<TR_FRAME_SIZE;i++) got=tr_parser_push(&p,wire[i],&b);
    assert(got);
    puts("PASS frame round trip, corruption, invalid fields, truncated stream recovery");
}

static void delivery(void) {
    tr_sender tx={0}; tr_message a=state(1,1), out, ack=a;
    ack.kind=TR_ACK;
    assert(tr_sender_offer(&tx,&a));
    assert(!tr_sender_offer(&tx,&a));
    assert(tr_sender_poll(&tx,0,&out));
    assert(!tr_sender_poll(&tx,TR_RETRY_MS-1,&out));
    assert(tr_sender_poll(&tx,TR_RETRY_MS,&out));
    assert(tx.retries==1 && out.sequence==1);
    ack.sequence=2; assert(!tr_sender_ack(&tx,&ack));
    ack.sequence=1; ack.session=2; assert(!tr_sender_ack(&tx,&ack));
    ack.session=1; ack.room=2; assert(!tr_sender_ack(&tx,&ack));
    ack.room=1; assert(tr_sender_ack(&tx,&ack));
    assert(!tr_sender_poll(&tx,2000,&out));
    assert(!tr_sender_ack(&tx,&ack));
    assert(tr_sender_offer(&tx,&a));
    for(unsigned n=0;n<TR_MAX_ATTEMPTS;n++) assert(tr_sender_poll(&tx,3000+n*TR_RETRY_MS,&out));
    assert(!tr_sender_poll(&tx,3000+TR_MAX_ATTEMPTS*TR_RETRY_MS,&out));
    assert(!tx.waiting && tx.timeouts==1);
    a.sequence=2; assert(tr_sender_offer(&tx,&a));
    assert(tr_sender_poll(&tx,6000,&out) && out.sequence==2);
    a.sequence=1;
    tr_receiver rx={0};
    assert(tr_receiver_accept(&rx,&a,10)==TR_NEW);
    assert(tr_receiver_accept(&rx,&a,20)==TR_DUPLICATE);
    assert(rx.last_seen_ms==20);
    a.sequence=2; a.light=false;
    assert(tr_receiver_accept(&rx,&a,30)==TR_NEW);
    a.sequence=1;
    assert(tr_receiver_accept(&rx,&a,40)==TR_STALE);
    assert(!rx.state.light && rx.last_seen_ms==30);
    a.session=2;
    assert(tr_receiver_accept(&rx,&a,50)==TR_NEW);
    a.session=1; a.sequence=100;
    assert(tr_receiver_accept(&rx,&a,60)==TR_STALE);
    assert(!tr_receiver_online(&rx,50+TR_STALE_MS));
    assert(tr_receiver_online(&rx,50+TR_STALE_MS-1));
    a=state(3,0xFFFFFFFFu);
    assert(tr_receiver_accept(&rx,&a,70)==TR_NEW);
    a.sequence=1;
    assert(tr_receiver_accept(&rx,&a,80)==TR_NEW);
    a.room=2; assert(tr_receiver_accept(&rx,&a,90)==TR_INVALID);
    a.room=1; a.kind=TR_ACK; assert(tr_receiver_accept(&rx,&a,90)==TR_INVALID);
    puts("PASS retry, lost ACK, ACK identity, duplicates, stale/reboot/rollover, offline timeout");
}

int main(void) { room_timing(); protocol(); delivery(); puts("All Trevor core tests passed."); return 0; }
