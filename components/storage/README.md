# Kagimu Edward — storage

`runtime.c` initializes the shared SPI bus, attempts to mount the microSD card,
appends access, schedule-change, and room events to `/sdcard/events.csv`, and
commits the registered-user roster to NVS with a mirror at `/sdcard/users.bin`.
NVS is loaded first; the legacy SD roster is used only when no NVS roster exists.
An SD mirror failure is reported without undoing a successful NVS commit.
Trevor's storage worker is the only event-file writer; a mutex protects CLI
reads from writes. Access and schedule-change requests wait for the writer to
flush, sync, and close the file before reporting success. If the SD card is
missing or full, local control continues and failed writes appear in the system
counters; the active schedule is still stored in NVS.

The `records` reader copies approximately the last 2 KB under the file mutex,
then releases it before printing to UART0, so slow output does not block writes.

New access rows contain the event number, Kampala date/time, person, entry or
exit direction, decision, UID, user number, and inside count. Schedule rows
contain the timestamp and show the previous and new opening/closing times.

Wokwi folder settings are in the repository's `wokwi.toml`. The 1 November
Wokwi RFID scenario mounted the card and wrote entry and exit records using the
previous compact event format. Re-run it to inspect the current readable rows;
roster and schedule persistence across a reboot still need live checks.
