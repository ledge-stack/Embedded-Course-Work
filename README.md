# CoCIS Smart Control and Monitoring System

ESP32 based coursework project for a pilot covering the CoCIS main entrance and two lecture rooms. The design follows the coursework brief and the team's work distribution document. The target stack is ESP32 with ESP-IDF, developed in PlatformIO and demonstrated in Wokwi.

## Team and work areas

| Member | Primary area |
| --- | --- |
| Leon Nsitta Kalungi | Project setup, entrance terminal, access schedule and integration |
| Kato Trevor | Lecture room terminals, inter-terminal communication and synchronization |
| Kagimu Edward | SD card storage, serial menus and authentication |
| Ahurira Grace | Scan feedback LED and its actuator interface |

On a clean install, the Wokwi RFID cards are assigned to these members:

| Wokwi card | Member | UID |
| --- | --- | --- |
| Blue | Leon Nsitta Kalungi | `DE:AD:BE:EF` |
| Green | Kato Trevor | `11:22:33:44` |
| Yellow | Kagimu Edward | `55:66:77:88` |
| Red | Ahurira Grace | `AA:BB:CC:DD` |

## Current status

Leon implemented the entrance RFID readers, member card roster, access and
anti-passback decisions, Kampala schedule, LCD, keypad, dashboard, roll-call,
and roster API. Accepted entry and exit scans show a named welcome or goodbye;
the LCD keypad and signed-in dashboard include roster status and recent access
activity. Trevor's
room sensing, automatic lights, messaging, retries, FreeRTOS synchronization,
serialized storage queue, and light-use counters live under `components/trevor/`.
Edward's SD event
log, NVS user roster, and UART0 status/admin CLI live in `components/storage/`
and `components/cli/`. Grace's scan-feedback LED is in `components/actuator/`.
`src/main.c` starts the combined modules and supplies the entrance inside count
to the room-light safety rule. The [system operation report](docs/SYSTEM_OPERATION_REPORT.md)
covers the architecture, hardware, implementation, operation, member areas,
and verification limits. The documentation index is
[`docs/README.md`](docs/README.md); it links to the system walkthrough,
presentation demo, member speaking guides, and pin/interface contract.

For beginner-friendly navigation, see the [repository structure guide](docs/REPOSITORY_STRUCTURE_GUIDE.md),
[code walkthrough](docs/CODE_WALKTHROUGH.md), [system principles guide](docs/SYSTEM_PRINCIPLES_GUIDE.md),
and [module integration guide](docs/MODULE_INTEGRATION_GUIDE.md). Separate
[presentation guides](docs/presentations/README.md) are available for Leon,
Trevor, Edward, and Grace.

## Build and simulation

This project uses PlatformIO's `esp32dev` environment with the ESP-IDF framework.

```sh
pio run -e esp32dev
```

The combined one-ESP32 presentation build is `pio run -e esp32dev` (`TR_ROLE=0`);
Trevor's `coordinator`, `room1`, and `room2` environments are separate physical
roles and do not start Leon's entrance pins. The demo joins `Wokwi-GUEST` and
uses SNTP for the Kampala access schedule. Both entry and exit fail closed if
time is unavailable or the schedule is closed. If no saved roster exists, the
demo starts with four member cards: Blue (`DE:AD:BE:EF`) for Leon, Green
(`11:22:33:44`) for Trevor, Yellow (`55:66:77:88`) for Edward, and Red
(`AA:BB:CC:DD`) for Grace. Admin roster changes are saved to the SD card with
an NVS backup.
The LCD shows entrance status, schedule, last event, and occupancy. Admins can
change hours from the keypad (press `A`, enter demo PIN `2468`, then `#`) or the
Wi-Fi dashboard; schedule changes persist in NVS and are also recorded in the
SD audit log. The PIN and HTTP dashboard
are presentation-only. Wokwi browser access to the ESP32 web server requires
its Private IoT Gateway; with it enabled, open `http://localhost:9080/`.
The UART0 CLI offers `help`, `status`, `inside`, `rooms`, `usage`, `users`,
`records`, and `schedule`. `usage` shows light-on and empty-building-suppressed
durations since boot (not kWh). Use `login 2468` before `open HH:MM`, `close HH:MM`, `useradd NAME UIDHEX`,
`userdel ID`, or `confirm-empty`. Events are appended to `/sdcard/events.csv` when the
SD card mounts; the CLI reports when it is unavailable.

Open the project in Wokwi after building. [`wokwi.toml`](wokwi.toml) loads the
`esp32dev` firmware from `.pio/build/esp32dev/`. The local circuit and signal
assignments are in [`diagram.json`](diagram.json) and
[`docs/INTERFACES.md`](docs/INTERFACES.md). A published Wokwi project is useful
supporting evidence, but the lecturer accepts a Google Drive or GitHub link for
the submission. The presentation sequence is in
[`docs/PRESENTATION_DEMO.md`](docs/PRESENTATION_DEMO.md).

## Requirements at a glance

- Identify users with RFID at separate entrance and exit points; prevent duplicate entry and exit-without-entry.
- Keep an accurate list and count of people inside, and apply the configured schedule consistently to both doors.
- Detect occupancy in two lecture rooms and control their lights with a delay to prevent flicker.
- Coordinate three ESP32 terminals over a documented message protocol, including reconnect and lost-message handling.
- Persist access, occupancy, lighting, user and configuration information on an SD card, with safe behavior if the card is unavailable or full.
- Provide serial commands for system status and records; require authenticated admin sessions for changes.
- Protect shared state and storage operations against concurrent updates.
- Demonstrate the system in Wokwi and prepare the operation report, individual contribution statements, member details and individual video demonstrations for the 2 November 2026 submission.

## Team workflow

Work from GitHub issues. Use one feature branch per issue, open a pull request to `main`, and have a teammate review it before merging. Keep shared data structures and message formats coordinated through the shared architecture issue. The remote `main` branch must require a pull request and at least one approving review; see the repository setup notes in the implementation plan.

## Project layout

The ESP-IDF build exposes `main`, `entrance`, `rooms`, `comms`, `storage`,
`cli`, `auth`, `actuator`, and `common` component boundaries. Firmware
components are in `components/`, with public headers in `include/`. The `rooms`
and `comms` folders are interface boundaries backed by Trevor's implementation
in `components/trevor/`. Folder ownership is summarized in
[`components/README.md`](components/README.md), with details in
[`docs/COMPONENT_OWNERSHIP.md`](docs/COMPONENT_OWNERSHIP.md).
