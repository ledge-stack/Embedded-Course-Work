# Leon's work in this change

This note separates Leon's coursework contribution from Trevor's existing
room-control contribution.

## Implemented by Leon

- Added the entrance access module in components/entrance and its public
  interfaces in include/entrance. It reads the entry and exit MFRC522 readers,
  checks the demo allowlist, controls grant/deny LEDs, and records timestamped
  access outcomes.
- Added building occupancy and anti-passback behavior. Re-entry while inside
  and exit without an active entry are denied. The event API rejects duplicate
  and older sequence IDs.
- Added the Kampala access-window rules and SNTP time setup. Entry fails closed
  when time is unavailable or the schedule is closed. A person already inside
  can exit after closing. The schedule API is RAM-only until the authenticated
  CLI and configuration storage are integrated.
- Added the one-ESP32 Wokwi diagram, RFID/door indicator pin plan, and
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
and authorship remain Trevor's. Leon's src/main.c change only starts Trevor's
runtime alongside Leon's entrance runtime.

## Not claimed as completed here

Edward's SD-card persistence, registered-user/configuration storage, serial
command handlers, and authenticated admin flow remain integration work. The
shared Wokwi project has not been published or run in the simulator, so this
change provides a local diagram but no public simulation URL or runtime
evidence.
