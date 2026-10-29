# Leon's work in this change

This note separates Leon's coursework contribution from Trevor's existing
room-control contribution.

## Implemented by Leon

- Added the entrance access module in components/entrance and its public
  interfaces in include/entrance. It reads the entry and exit MFRC522 readers,
  checks the demo allowlist, and records timestamped access outcomes. The LCD
  reports open/closed state, access hours, last person/action/time, and current
  occupancy.
- Added building occupancy and anti-passback behavior. Re-entry while inside
  and exit without an active entry are denied. The event API rejects duplicate
  and older sequence IDs.
- Added the Kampala access-window rules and SNTP time setup. Entry fails closed
  when time is unavailable or the schedule is closed. A person already inside
  can exit after closing. Schedule changes from the authenticated keypad or
  Wi-Fi dashboard are saved in ESP32 NVS.
- Added a keypad PIN flow for changing opening and closing times, plus an
  authenticated Wi-Fi dashboard for viewing live entrance status and changing
  the same schedule. The presentation-only demo PIN is `2468`.
- Added the one-ESP32 Wokwi diagram, RFID/LCD/keypad pin plan, and
  presentation steps. The diagram includes the two-room demo hardware and
  microSD card, but SD mounting and writes are not implemented here.
- Added shared user, access-event, schedule, and terminal-message definitions
  under include/common, ESP-IDF component build boundaries, and README/build
  documentation.

## Trevor's existing work

Trevor's room-control and communications implementation is in
components/trevor/core.c and components/trevor/runtime.c, with public headers
in include/trevor. It provides the room motion/off-delay logic, room
state protocol, logical link queues, UART roles, synchronization, and the
serialized storage hook. Those implementation files were moved from src/trevor
to components/trevor so ESP-IDF can build them as a component; their behavior
and authorship remain Trevor's. Leon's src/main.c integration starts both
runtimes together in the single-chip `TR_ROLE=0` presentation build. In
Trevor's optional physical-role builds it starts only Trevor's runtime, because
those UART roles reuse GPIOs assigned to Leon's entrance hardware.

## Not claimed as completed here

The denied-access buzzer, tailgate alerts, and evacuation report are planned,
not implemented. Anti-passback denial already exists, but a reader alone
cannot detect an extra person passing behind a cardholder. See
[`LEON_ENTRANCE_SAFETY_DESIGN.md`](LEON_ENTRANCE_SAFETY_DESIGN.md) for the
simulation-only trigger and the sensor needed for real tailgate detection.

Edward's SD-card event persistence, registered-user management, and serial
command handlers remain integration work. This contribution's PIN and Wi-Fi
dashboard are demo-only and do not replace production authentication. Wokwi's
HTTP server access from a browser requires the Private IoT Gateway. The shared
Wokwi project has not been published or run in the simulator, so this change
provides a local diagram but no public simulation URL or runtime evidence.
