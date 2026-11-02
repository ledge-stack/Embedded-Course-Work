# Shared interfaces and terminal pin plan

Version 1, updated 1 November 2026. This is the working contract for the combined
single-board Wokwi presentation simulation.
All members should review it before extending the message protocol.

## Ownership and types

- include/common/system.h owns the shared user, access event, schedule and
  terminal-message types. User IDs are stable indices in the 0-31 table; UID
  lengths are bytes (up to 10); timestamps are UTC epoch seconds plus a
  monotonic millisecond value for ordering within one boot.
- Leon's entrance coordinator is the sole writer of inside[], building count,
  door decisions and the access schedule. The current snapshot API is
  entrance_runtime_snapshot; schedule mutation is
  entrance_runtime_set_schedule. The authenticated keypad, Wi-Fi dashboard,
  and UART0 CLI call this same API; none keeps a second schedule copy.
- Edward's storage module loads a fixed array of SYSTEM_MAX_USERS
  entries to entrance_runtime_set_users() after it loads or changes the
  registered-user list. Array indexes are stable user IDs. The API rejects
  duplicate/invalid UIDs and refuses to remove or replace a user who is still
  recorded inside. It saves the roster to SD with an NVS backup; on a clean device the
  presentation build begins with four member cards: Blue/Leon, Green/Trevor,
  Yellow/Edward, and Red/Grace. Old default demo-user names are migrated to the
  member names at startup.
- The public keypad reserves B for a simulation-only tailgate marker and C
  for a read-only evacuation roll-call. B marks occupancy uncertain but never
  edits the inside list; C prints names and count without card UIDs.
- entrance_runtime_set_schedule() is the shared schedule-write API for the
  keypad, dashboard, and Edward's authenticated serial commands. Callers
  must authenticate before invoking it. The entrance runtime validates and
  persists accepted values in NVS, then asks the storage worker to append a
  schedule-change audit row to SD. NVS remains the active schedule at boot; an
  SD audit-write failure is reported separately.
- Access state updates use nonzero 32-bit sequence IDs. entrance_apply_event
  applies only newer IDs and returns SYSTEM_ACCESS_STALE_EVENT for duplicate or
  out-of-order deliveries. The serial RFID task is the only event producer in
  this simulation; a remote access-event transport is not implemented.
- Room state uses a nonblocking handoff to the serialized storage owner. True
  means queued, not durable. Entrance access records use
  tr_runtime_record_access_durable(), which waits up to five seconds for the
  storage worker to append, flush, sync, and close the SD file. The compact
  version-1 access payload is event ID (4 LE bytes), user ID (4 LE), UTC epoch
  (8 LE), monotonic milliseconds (8 LE), access point (1), result (1), UID
  size (1), followed by UID bytes. Total size is 28-37 bytes. New rows are
  written in readable CSV form with Kampala time, name, direction, result,
  UID, user ID, and inside count.
- Room state and its UART protocol remain owned by Trevor. The current binary
  room frame is documented in docs/TREVOR_GUIDE.md; it is not reused as an
  access-event wire message. Remote occupancy/schedule replication still needs
  a team-approved protocol extension.
- Leon's entrance keypad, Wi-Fi dashboard, and Edward's UART0 CLI share the
  demo PIN verifier in `components/auth`. They mutate the same entrance
  schedule API; it commits hours to NVS. The demo PIN is `2468` and is not
  production authentication.
- Edward's SD event append, NVS registered-user roster and UART0 CLI are
  implemented in `components/storage/` and `components/cli/`. The SD card's
  mount/write behavior passed the RFID entry/exit Wokwi scenario; roster
  persistence and card-full/recovery cases still need testing.

The ESP-IDF component targets are main, entrance, rooms, comms, storage, cli,
auth and common. The `rooms` and `comms` targets are interface aliases over
Trevor's implementation. Storage and CLI are Edward's modules; Grace's scan
feedback output is in the `actuator` component.

system_access_schedule uses local minutes after midnight in
Africa/Kampala (UTC+03:00). A start before end defines [start,end). A start
after end defines an overnight interval. Equal endpoints are rejected. Both
entry and exit are schedule-gated. A known person recorded inside is
denied exit while the schedule is closed; the inside list remains unchanged.
Unknown cards and exit-without-entry remain denied at all times.

## One-board Wokwi pin map

| Signal | GPIO |
| --- | ---: |
| Room 1 PIR / light | 27 / 26 |
| Room 2 PIR / light | 33 / 25 |
| Link up/down switch | 32 |
| Shared MFRC522 SCK / MOSI / MISO | 18 / 23 / 19 |
| Entry MFRC522 SDA/SS / shared RST | 5 / 4 |
| Exit MFRC522 SDA/SS / shared RST | 13 / 4 |
| LCD2004 SDA / SCL (I2C address 0x27) | 21 / 22 |
| Keypad rows R1-R4 | 14 / 15 / 16 / 17 |
| Keypad columns C1-C4 | 34 / 35 / 36 / 39 |
| Keypad column pull-ups | 10 kOhm to 3.3 V, one per column |
| microSD SPI chip-select (shared SPI bus) | 2 (10 kOhm pull-up to 3.3 V) |
| Grace's scan feedback LED | 12 (330 Ohm series resistor; 10 kOhm pulldown; boot-strap pin) |
| UART0 | USB serial console, 115200 baud |

Build the combined one-chip Wokwi demo only with `pio run -e esp32dev`
(`TR_ROLE=0`). This starts Trevor's logical rooms and Leon's entrance module
together. The `coordinator`, `room1`, and `room2` environments are Trevor's
separate physical-role builds; `main.c` does not start the entrance module in
those modes because the UART pins overlap the entrance RFID/keypad assignments.

Each MFRC522 operates at 3.3 V. The two readers share SPI and reset, with
separate chip-selects. The LCD is a 20x4 I2C module. The 4x4 keypad scans four
output rows and four input columns; GPIO 34, 35, 36 and 39 need external pull-up
resistors because they do not provide internal pulls. The room LEDs represent
room lighting only. Grace's separate blue LED signals scan results; it does not
operate a physical door.

The Wokwi microSD card shares SCK/MOSI/MISO and has a separate CS. Edward's
storage component mounts it at `/sdcard` and appends room/access events to
`/sdcard/events.csv` through Trevor's single storage worker. The 1 November
Wokwi RFID scenario mounted the card and wrote the entry and exit events.

## Clock and safety behavior

The demo joins the open Wokwi-GUEST network and synchronizes with SNTP. The
timezone conversion is EAT-3 (Kampala, UTC+03:00, no daylight-saving change).
If network/time synchronization fails, entry is denied and reported as a clock
fault. Entry and exit are denied if the clock is unavailable or the schedule is
closed. At the closing boundary the serial log lists any people still inside.
The schedule
defaults to 08:00-18:00 and keypad/dashboard changes persist in NVS. The Wi-Fi
dashboard listens on HTTP port 80 and requires Wokwi's Private IoT Gateway for
browser access from outside the simulated ESP32. This dashboard and PIN are
presentation features, not production security. A Wokwi URL can be added as
optional supporting evidence after publication and verification; the lecturer
also accepts a Google Drive or GitHub submission link.
