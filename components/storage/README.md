# Kagimu Edward — storage

`runtime.c` initializes the shared SPI bus, attempts to mount the microSD card,
appends access, schedule-change, and room events to `/sdcard/events.csv`, and
stores the registered-user roster in `/sdcard/users.bin` with an NVS backup.
Trevor's storage worker is the only event-file writer; a mutex protects CLI
reads from writes. Access and schedule-change requests wait for the writer to
flush, sync, and close the file before reporting success. If the SD card is
missing or full, local control continues and failed writes appear in the system
counters; the active schedule is still stored in NVS.

New access rows contain the event number, Kampala date/time, person, entry or
exit direction, decision, UID, user number, and inside count. Schedule rows
contain the timestamp and show the previous and new opening/closing times.

Wokwi folder settings are in the repository's `wokwi.toml`. The 1 November
Wokwi RFID scenario mounted the card and wrote entry and exit records using the
previous compact event format. Re-run it to inspect the current readable rows;
roster and schedule persistence across a reboot still need live checks.
