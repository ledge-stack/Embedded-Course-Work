# Coursework implementation and completion plan

**Status date:** 1 November 2026

**Submission date in the original plan:** 2 November 2026

This document records the current code by member and separates build evidence
from simulation evidence. Folder ownership is also listed in
[`COMPONENT_OWNERSHIP.md`](COMPONENT_OWNERSHIP.md).

## Implemented by member

### Leon Nsitta — entrance and integration

- Two RFID readers, known-user checks, entry/exit decisions, anti-passback,
  inside list/count, and schedule checks at both doors.
- Kampala SNTP clock, schedule persistence, LCD, keypad, Wi-Fi dashboard,
  evacuation roll-call, after-hours alert, and access-event handoff.
- `entrance_runtime_set_users()` accepts Edward's saved roster without allowing
  an occupied person's identity to be replaced or removed.
- `src/main.c` connects Leon's authoritative inside count to Trevor's rooms.

### Kato Trevor — room control and communication

- Two PIR/light channels, immediate on behavior, configurable off-delay, room
  state snapshots, CRC and sequence checking, acknowledgements, retries,
  duplicate/stale checks, link state and diagnostic counters.
- FreeRTOS room/coordinator/storage tasks, bounded queues, a mutex-protected
  snapshot and a single serialized event-storage worker.
- In the one-board presentation build, room lights are forced off when Leon's
  known inside count is zero. Unknown count does not mean empty.
- Per-room light-on time and time suppressed by the known-empty rule are
  counted since boot and exposed through the serial `usage` command. These are
  runtime-duration metrics, not measured energy or kWh.

### Kagimu Edward — storage and serial administration

- Shared SPI2 bus initialization for the RFID readers and microSD card.
- SD mount attempt and append-only CSV event log at `/sdcard/events.csv` for
  access and room events. New access rows include person and Kampala time, and
  schedule changes add an old/new-hours audit row. Access and schedule events
  wait for the storage worker's flush/sync result. Missing-card or write
  failures are counted and do not stop local control; the active schedule is
  saved independently in NVS.
- Registered-user roster saved to SD with an NVS backup and loaded at startup.
- UART0 commands: `help`, `status`, `inside`, `rooms`, `usage`, `users`, `records`,
  `schedule`, `login PIN`, and `logout`. A 60-second admin session is required
  for `open HH:MM`, `close HH:MM`, `useradd NAME UIDHEX`, and `userdel ID`.
- User updates go through Leon's validation API. A user who is inside cannot
  be removed. The coursework demo PIN is `2468` and is not production security.

### Ahurira Grace — entrance feedback

- A blue Wokwi LED gives a short pulse after an allowed scan and a longer pulse
  after a denied scan. It is a visual feedback signal, not a door lock or a
  physical actuator.
- GPIO12 is used because it is the remaining output in the combined circuit.
  This pin is an ESP32 boot-strapping pin; the diagram now includes an external
  10 kOhm pulldown. The final Wokwi scenario booted successfully; physical
  hardware behavior still needs a board-level check.

## Combined diagram and pin assignments

`diagram.json` is one ESP32 with two logical rooms, two RFID readers, an LCD,
keypad, slide switch, SD card and feedback LED. The other PlatformIO roles are
separate firmware builds; this diagram does not contain three physical ESP32s.
The detailed GPIO map is in [`INTERFACES.md`](INTERFACES.md).

The RFID readers and SD card share SCK/MOSI/MISO GPIO 18/23/19. They have CS
GPIOs 5/13/2. The firmware initializes the bus for SD block-sized transfers
before attaching both RFID readers. The SD part has a Wokwi folder configured
in `wokwi.toml`.

## Verification status

Local evidence from this update:

- All four PlatformIO environments passed after the both-door schedule rule
  was added; the three auxiliary SDK configs match the board's 4 MB flash size.
- The portable entrance test passed for schedule enforcement at entry and exit,
  denied exit with unchanged occupancy, anti-passback, invalid exit and clock
  failure. Trevor's portable core suite, including light-use time accounting,
  also passed.
- On 1 November, the Wokwi RFID scenario mounted the SD card, granted the
  green demo card's entry and exit during open hours, returned occupancy to
  zero, and wrote both access records into `sdcard/events.csv`. It completed
  successfully with the final GPIO12 pulldown wiring in the diagram.
- The after-hours Wokwi scenario also passed: the ESP32 booted, joined Wi-Fi,
  synchronized time, mounted the SD card, read the registered UID
  `11223344`, and correctly denied entry while the schedule was closed. It
  then read and denied the unregistered Key Fob UID `C0FFEE99`.
- The current access/schedule CSV format and storage-completion acknowledgement
  compile in all four PlatformIO environments. The updated readable rows and
  survival of records through a power cycle have not yet been demonstrated in
  Wokwi.
- The expanded `presentation-smoke.yaml` scenario adds repeat-entry and
  unknown-card assertions. Its current run remains unverified because the
  synchronized Wokwi clock was after closing time, so its grant assertion was
  denied by the configured schedule.
- Roster management, serial CLI actions, empty-building light shutdown, visual
  LED feedback, SD-full/recovery and reboot scenarios are not yet demonstrated.

## Still required before submission

1. Run the Wokwi circuit to check schedule denial at both doors (including a
   person already inside), clock-unavailable denial, roster persistence,
   authenticated CLI actions, empty-building light shutdown, and the feedback
   LED. Test missing/full SD card handling and reboot behavior.
2. During open hours, demonstrate a denied repeat entry and verify the event
   records remain readable with the CLI.
3. Demonstrate CLI queries, admin schedule changes, adding/deleting a user, and
   the refusal to delete an occupied user.
4. Demonstrate both room lights and off-delay, empty-building light shutdown,
   link outage/recovery, serial diagnostics, and the new `usage` command. Verify
   that on-time increases while a light is active and suppressed time increases
   when the building is confirmed empty.
5. Check the feedback LED pulses visually in Wokwi. The current feedback LED
   does not actuate a real door, and the keypad B tailgate marker is simulated
   rather than an automatic sensor.
   After reboot or a simulated tailgate, occupancy remains uncertain until a
   staff member checks the building and an administrator runs `confirm-empty`.
6. Save simulator logs, screenshots, or video if available. Submit the project,
   report, member contribution statements, and walkthroughs through the Google
   Drive or GitHub link accepted by the lecturer. A published Wokwi URL is
   optional supporting evidence.

The Wokwi credential is not stored in the repository. If the CLI cannot
authenticate, perform the interactive run in the Wokwi website using a valid
private project session; do not treat `wokwi-cli lint --offline` as a simulation.
