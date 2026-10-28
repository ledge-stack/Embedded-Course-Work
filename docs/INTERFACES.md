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
  entrance_runtime_set_schedule and must only be called after Edward's admin
  authentication succeeds.
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
- Edward owns user/config persistence and authenticated command handling.
  Entrance currently uses two clearly labelled demo users until his user source
  is integrated; no password or admin credential is compiled into this module.

The ESP-IDF component targets are main, entrance, rooms, comms, storage, cli,
auth and common. Rooms, comms and storage currently expose integration
boundaries over Trevor's existing component; cli and auth are scaffolds for
Edward's assigned implementation.

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
| Shared MFRC522 SCK / MOSI / MISO | 21 / 22 / 23 |
| Entry MFRC522 SDA/SS / shared RST | 5 / 4 |
| Exit MFRC522 SDA/SS / shared RST | 13 / 4 |
| Entry and exit grant indicators | 14 / 15 |
| Entry and exit deny indicators | 16 / 17 |
| microSD SPI chip-select (shared SPI bus) | 18 |
| UART0 | USB serial console, 115200 baud |

Each MFRC522 operates at 3.3 V. The two readers share SPI and reset, with
separate chip-selects. GPIO 14-17 drive grant/deny indicator LEDs in Wokwi, not
door actuators. A physical door needs a rated driver and an independently
reviewed fail-safe design.

The Wokwi microSD card shares SCK/MOSI/MISO and has a separate CS. The diagram
includes the card hardware, but Edward's storage component has not yet mounted
it or written files.

## Clock and safety behavior

The demo joins the open Wokwi-GUEST network and synchronizes with SNTP. The
timezone conversion is EAT-3 (Kampala, UTC+03:00, no daylight-saving change).
If network/time synchronization fails, entry is denied and reported as a clock
fault. Exit for an authorized person already inside remains available. At the
closing boundary the serial log lists any people still inside. The current
schedule is RAM-only, defaults to 08:00-18:00, and resets at reboot;
Edward's authenticated persistent configuration path is a required integration
step. No Wokwi project URL is claimed until the circuit is published and opened
successfully in the shared simulator.
