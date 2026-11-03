# Wokwi RFID verification - 3 November 2026

The submitted warning log repeatedly reported `no card response to REQA`
for both readers. An idle reader, or a card already halted after a scan,
normally does not answer REQA. The driver now treats that timeout quietly
while continuing to report SPI, reader and UID errors.

A live tap before the fix read UID `DEADBEEF` correctly but denied access
with `CLOCK_UNAVAILABLE`. A Wokwi network capture showed late DNS responses
for `pool.ntp.org` after the resolver timeout and no NTP requests. The clock
now uses `162.159.200.1`, a published Cloudflare NTP address:
https://developers.cloudflare.com/time-services/ntp/usage/
Time and schedule checks remain enforced. A warning every 20 seconds makes
an unsynchronized clock visible.

The local RFC2217 bridge is on port 4001, avoiding a collision with another
running project's port 4000. Close and reopen this Wokwi simulator editor
after changing its configuration, then start it with the rebuilt firmware.

## Verification

- ESP32 PlatformIO build: PASS (969601 bytes flash; 39360 bytes static RAM).
- Host suite: room/runtime, entrance policy, door outputs, 13 management/storage
  regressions and five new RFID polling regressions: PASS.
- Live Wokwi: `SNTP synchronized` at 13239 ms after startup.
- Blue card on the left reader: entry granted, inside count 1.
- Same card on the left reader again: denied as already inside, count stays 1.
- Same card on the right reader: exit granted, inside count returns to 0.
- No repeated idle `no card response to REQA` warnings in this test.

Captured access messages:

```text
event=1 utc=1793705385 point=entry user=Demo User 1 uid=DEADBEEF result=GRANTED inside=1
event=2 utc=1793705412 point=entry user=Demo User 1 uid=DEADBEEF result=DENIED inside=1 (already inside)
event=3 utc=1793705438 point=exit user=Demo User 1 uid=DEADBEEF result=GRANTED inside=0
```

These checks cover the RFID/time fault. They do not establish that every
keypad, browser, motion, persistence or power-loss scenario has been retested.
Internet connectivity is still needed for time synchronization after reset.

## Subsequent time-server outage

A later run did not synchronize even after several minutes. The downloaded
`ntp-retry.pcap` contained six outgoing UDP requests to `162.159.200.1:123`
and zero NTP replies. Wi-Fi association and an IP address alone therefore did
not establish a working time-server connection. The earlier 13-second result
is one observed success, not a guaranteed startup time.

The clock configuration now tries `162.159.200.123` first and
`162.159.200.1` second, both published by Cloudflare. SDK configuration enables
two SNTP servers so the client automatically changes server after its receive
timeout. A complete time-service outage still leaves access disabled.

The two-server firmware built successfully (969933 bytes flash, 39400 bytes
static RAM). The generated SDK header confirms two SNTP server slots. A fresh
Wokwi run logged `SNTP synchronized` at 8239 ms. This confirms recovery in that
run, but does not prove that both public endpoints are always reachable or
guarantee an eight-second startup on another connection.

The same rebuilt firmware then granted entry at 25049 ms (`inside=1`) and
exit at 39889 ms (`inside=0`) for UID `DEADBEEF`. The simulator was left
running with the person outside.
