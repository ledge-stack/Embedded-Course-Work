# Kagimu Edward — storage

`runtime.c` initializes the shared SPI bus, attempts to mount the microSD card,
appends access and room events to `/sdcard/events.csv`, and stores the registered
user roster in `/sdcard/users.bin` with an NVS backup. Trevor's storage worker is the only event-file writer; a
mutex protects CLI reads from writes. If the SD card is missing or full, local
control continues and failed writes appear in the system counters.

Wokwi folder settings are in the repository's `wokwi.toml`. The 1 November
Wokwi RFID scenario mounted the card and wrote entry and exit records. Roster
persistence across a reboot still needs its own check.
