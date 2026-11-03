# Interfaces and pin contract — 3 November 2026

The only firmware target is `esp32dev`, one ESP32 with logical room and entrance
tasks. The keypad, LCD, dashboard and serial interface remain enabled.

| Signal | GPIO |
| --- | --- |
| Room 1 PIR / light | 27 / 26 |
| Room 2 PIR / light | 33 / 25 |
| RFID and SD SCK / MOSI / MISO | 18 / 23 / 19 |
| Entry RFID CS / exit RFID CS / shared reset | 5 / 13 / 4 |
| SD CS | 2, existing 10 kOhm pull-up |
| LCD SDA / SCL, address 0x27 | 21 / 22 |
| Keypad rows | 14 / 15 / 16 / 17 |
| Keypad columns | 34 / 35 / 36 / 39, existing external pull-ups |
| Entry door LED | 32, new 330-ohm series resistor |
| Exit door LED | 12, existing series resistor and 10 kOhm pulldown |
| Serial console | UART0, 115200 baud |

The old GPIO32 link switch is removed. Door outputs are indicators, not real
locks. GPIO12 remains a boot-strapping pin with its existing pulldown.

## Ownership

- Entrance owns registered users, live inside flags and schedule. Use
  `entrance_runtime_status()` / `snapshot()` and authenticated mutation APIs.
  Schedule updates serialize the complete NVS/RAM transaction.
- Rooms own their lights and local snapshots. `tr_runtime_snapshot()` returns
  both rooms and storage/usage counters. The `valid` field means sampled,
  not a remote network connection. Off-delay changes are RAM-only.
- `tr_runtime_set_building_inside_count(count, authoritative)` receives the
  entrance count. Known zero suppresses lights; unknown does not.
- One `tr_storage_event` queue carries changed rooms, access rows and schedule
  audit rows. No C structure is transmitted over UART. There are no room frames,
  CRC, ACKs, retry counters or boot-session numbers.
- `tr_runtime_record_access_durable()` and `record_schedule_change()` wait for
  their request-ID-specific SD completion, at most five seconds. Room events
  only confirm queue admission. Failures/drops remain observable.
- `tr_storage_append()` is called by one writer. The SD filesystem mutex also
  protects roster operations and bounded recent-record reads.
- NVS owns schedule and roster. SD is a roster mirror and event journal. Legacy
  SD roster files are accepted only if NVS has no roster; the live inside list
  remains volatile and uncertain after reset.
- `actuator_runtime_signal(point, granted)` opens only the granted direction's
  output. A separate task closes each after three seconds. The occupancy bridge
  calls `actuator_runtime_close()` outside hours, with about 250 ms resolution.

## SD record formats

New room row: `ROOM,uptime_ms,room,motion,light,off_delay_ms`.
Access rows retain event ID, Kampala time, name, direction, result, UID, user ID,
inside count, UTC seconds and uptime. Schedule rows retain old/new times.
Old rows are not rewritten and may use the previous room sequence column.

All admin interfaces use PIN 2468 and the same schedule API. Ordinary and
overnight hours are supported; equal endpoints are invalid; both directions
are denied when time is unavailable or outside hours. The web UI requires the
Wokwi Private IoT Gateway for host browser access.
