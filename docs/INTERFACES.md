# Shared interfaces and terminal pin plan

Version 1, 28 October 2026. This is the working contract for the combined
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
  entrance_runtime_set_schedule. Only the authenticated entrance keypad or
  Wi-Fi dashboard should expose schedule changes to an administrator.
- Access state updates use nonzero 32-bit sequence IDs. entrance_apply_event
  applies only newer IDs and returns SYSTEM_ACCESS_STALE_EVENT for duplicate or
  out-of-order deliveries. The serial RFID task is the only event producer in
  this simulation; a remote access-event transport is not implemented.
- Trevor's tr_runtime_record_access is a nonblocking handoff to the
  serialized storage owner. true means queued, not durable. The compact
  version-1 access payload is event ID (4 LE bytes), user ID (4 LE), UTC epoch
  (8 LE), monotonic milliseconds (8 LE), access point (1), result (1), UID
  size (1), followed by UID bytes. Total size is 27-37 bytes.
- Room state and its UART protocol remain owned by Trevor. The current binary
  room frame is documented in docs/TREVOR_GUIDE.md; it is not reused as an
  access-event wire message. Remote occupancy/schedule replication still needs
  a team-approved protocol extension.
- Leon's entrance keypad and Wi-Fi dashboard share the demo PIN verifier in
  components/auth. Both mutate the same entrance schedule API; it commits hours
  to NVS. The demo PIN is `2468` and is not production authentication.
- Edward's registered-user management, SD event persistence and serial command
  handling remain integration work. Entrance currently uses two demo users.

The ESP-IDF component targets are main, entrance, rooms, comms, storage, cli,
auth and common. Rooms, comms and storage expose integration boundaries over
Trevor's existing component; auth supplies the presentation PIN verifier and
the serial CLI remains an integration task for Edward.

system_access_schedule uses local minutes after midnight in
Africa/Kampala (UTC+03:00). A start before end defines [start,end). A start
after end defines an overnight interval. Equal endpoints are rejected. Only
entry is schedule-gated. A known person already inside can exit after closing;
unknown cards and exit-without-entry remain denied.

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
| microSD SPI chip-select (shared SPI bus) | 0 (10 kOhm pull-up to 3.3 V) |
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
room lighting only. No physical door actuator is connected in this simulation.

The Wokwi microSD card shares SCK/MOSI/MISO and has a separate CS. The diagram
includes the card hardware, but Edward's storage component has not yet mounted
it or written files.

## Clock and safety behavior

The demo joins the open Wokwi-GUEST network and synchronizes with SNTP. The
timezone conversion is EAT-3 (Kampala, UTC+03:00, no daylight-saving change).
If network/time synchronization fails, entry is denied and reported as a clock
fault. Exit for an authorized person already inside remains available. At the
closing boundary the serial log lists any people still inside. The schedule
defaults to 08:00-18:00 and keypad/dashboard changes persist in NVS. The Wi-Fi
dashboard listens on HTTP port 80 and requires Wokwi's Private IoT Gateway for
browser access from outside the simulated ESP32. This dashboard and PIN are
presentation features, not production security. No Wokwi project URL is claimed
until the circuit is published and opened successfully in the shared simulator.
