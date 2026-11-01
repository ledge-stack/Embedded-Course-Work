# Coursework code ownership and completion

## Member-to-folder map

| Member | Owned folders | Main responsibility |
| --- | --- | --- |
| Leon Nsitta | `components/entrance/`, `include/entrance/` | RFID access, schedule, inside list, entrance display/keypad/dashboard, integration |
| Kato Trevor | `components/trevor/`, `components/rooms/`, `components/comms/`, `include/trevor/` | Room sensors/lights, off-delay, message protocol, queues, synchronization |
| Kagimu Edward | `components/storage/`, `components/cli/`, `include/storage/`, `include/cli/` | SD event persistence and roster with NVS backup, serial queries and admin changes |
| Ahurira Grace | `components/actuator/`, `include/actuator/` | Access feedback LED and circuit/wiring documentation |

`components/common/`, `components/auth/`, `include/common/`, and
`include/auth/` are shared integration support. `src/main.c` starts the modules
and connects the authoritative entrance count to Trevor's room safety rule.
No folder was moved or renamed; each code folder has an owner README so existing
ESP-IDF component paths continue to work.

## What is now implemented

- Leon: two RFID readers, known-user access, anti-passback, inside count,
  configurable schedule, clock handling, keypad/dashboard, and event handoff.
- Trevor: two room PIR/light channels, off-delay, room messaging, retries,
  synchronization counters, event queue, serialized storage worker, and a
  building-empty shutdown input from Leon.
- Edward: SD mount and CSV event append, SD/NVS registered-user roster, serial
  status/read commands, and PIN-gated schedule and roster changes.
- Grace: one combined-board scan-feedback LED with distinct pulse durations for
  granted and denied scans. It is a visual simulation output, not a door lock.
- Shared integration: one-ESP32 Wokwi topology, logical room queues, SPI sharing
  between SD and MFRC522, occupancy bridge, and member-labelled folders.
- On startup, occupancy is marked uncertain because the live inside list is
  volatile. After checking the building, an administrator can run
  `confirm-empty`; room lights are only forced off at a known count of zero.

## Completion evidence still required

The 1 November Wokwi RFID scenario passed: SD mounted and stored both access
events, the green demo card entered and exited during open hours, and the count
returned to zero. The remaining Wokwi checks are user roster persistence,
authenticated CLI schedule changes, empty-building light shutdown, visual LED
feedback, link loss/recovery, missing/full-card handling, and reboot behavior.
Retain the logs and inspect the event file. A real door actuator, independent
tailgate sensor, and three separate ESP32 terminals are not part of this
one-board demonstration.
