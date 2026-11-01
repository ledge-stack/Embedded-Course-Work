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
- Added the Kampala access-window rules and SNTP time setup. Both entry and
  exit fail closed when time is unavailable or the schedule is closed.
  Schedule changes from the authenticated keypad or
  Wi-Fi dashboard are saved in ESP32 NVS.
- Added a keypad PIN flow for changing opening and closing times, plus an
  authenticated Wi-Fi dashboard for viewing live entrance status and changing
  the same schedule. The presentation-only demo PIN is `2468`; its verifier
  uses one PSA PBKDF2 operation with inputs in cost, salt, PIN order. The
  keypad shows a checking message while it verifies or saves a time.
- Added the one-ESP32 Wokwi diagram, RFID/LCD/keypad pin plan, and
  presentation steps. The diagram includes the two-room demo hardware and
  microSD card; Edward's storage module now mounts the card and writes events.
- Added shared user, access-event, schedule, and terminal-message definitions
  under include/common, ESP-IDF component build boundaries, and README/build
  documentation.
- Added a read-only evacuation roll-call on keypad C, an after-hours report
  when the schedule closes with occupants still inside, and a simulation-only
  tailgate marker on keypad B. The marker flags occupancy as uncertain and
  does not invent or add a person.
- Added explicit anti-passback and exit-without-entry alert messages. The LCD
  also displays an after-hours alert while the schedule is closed and people
  remain recorded inside.
- Added `entrance_runtime_set_users()` as the stable roster handoff for
  Edward's user store. The current demo roster remains active until Edward's
  source loads and supplies the registered users.

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

Grace's scan-feedback LED is now integrated through `components/actuator/`.
It gives different pulse lengths for allowed and denied scans, but does not
operate a door or buzzer. The keypad tailgate input is simulation-only; a
reader alone cannot detect an extra person passing behind a cardholder. See
[`LEON_ENTRANCE_SAFETY_DESIGN.md`](LEON_ENTRANCE_SAFETY_DESIGN.md) for the
simulation-only trigger and the sensor needed for real tailgate detection.

Edward's SD event persistence, saved user roster, and serial command handlers
are now integrated in `components/storage/` and `components/cli/`. The Wokwi
RFID scenario confirmed the SD mount and entry/exit event writes; user editing
and CLI commands still need interactive checks. This contribution's PIN and Wi-Fi
dashboard are demo-only and do not replace production authentication. Wokwi's
HTTP server access from a browser requires the Private IoT Gateway. The local
Wokwi RFID-reader probe detected the registered green card (`11:22:33:44`) at
both readers. An earlier entry-flow run was after the default 08:00-18:00
Kampala window and correctly denied the card. A later Wokwi scenario granted
entry and exit during open hours and stored both events on SD. The project has
not been published, so there is no shared simulation URL. Remaining evidence
includes user/CLI changes, room behavior, feedback LED, and a published link.
Leon's report section is in
[`LEON_REPORT_SECTION.md`](LEON_REPORT_SECTION.md).
