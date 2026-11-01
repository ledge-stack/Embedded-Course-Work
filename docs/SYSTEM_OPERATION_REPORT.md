# CoCIS Smart Control and Monitoring System

## System Operation Report

- **Prepared:** 1 November 2026
- **Coursework:** Embedded Systems
- **Project repository:** [Embedded-Course-Work](https://github.com/ledge-stack/Embedded-Course-Work)
- **Implementation branch:** [`feat/system-presentation-guide`](https://github.com/ledge-stack/Embedded-Course-Work/tree/feat/system-presentation-guide)

## 1. Purpose and scope

The system demonstrates access monitoring at the CoCIS entrance and automatic
lighting for two lecture rooms. It identifies people with RFID cards, keeps a
live list of registered users inside, checks the configured access hours,
records events, provides serial management commands, and coordinates firmware
tasks using FreeRTOS queues and mutexes.

The main presentation firmware runs on one ESP32. It simulates the entrance,
two rooms, and room coordinator as logical modules on that board. Separate
PlatformIO builds are provided for a coordinator and two physical room roles;
the supplied diagram does not contain three wired ESP32 boards.

## 2. System architecture

```mermaid
flowchart LR
    RFID[Entry and exit RFID readers] --> EN[Entrance runtime and rules]
    KEYPAD[4x4 keypad] --> EN
    EN --> LCD[20x4 LCD]
    EN --> AUTH[Shared PIN verifier]
    WEB[Wi-Fi dashboard] --> AUTH
    CLI[UART0 serial menu] --> AUTH
    AUTH --> CONFIG[Schedule and roster APIs]
    EN --> OCC[Inside count]
    OCC --> ROOMS[Two room tasks]
    PIR[PIR sensors] --> ROOMS
    ROOMS --> LIGHTS[Room light outputs]
    ROOMS <--> LINK[Coordinator queues and protocol]
    EN --> EVENTQ[Serialized event queue]
    ROOMS --> EVENTQ
    EVENTQ --> SD[SD event log]
    CONFIG --> SD
    LED[Grant/deny feedback LED] <-- EN
```

`src/main.c` starts the modules in a controlled order. It initializes NVS,
starts SD storage in the combined build, starts Trevor's room runtime, starts
Grace's feedback output, starts Leon's entrance runtime, loads the saved user
roster, starts the occupancy bridge, and starts Edward's serial command task.
The bridge copies the entrance occupancy status into the room-light safety
rule every 250 ms.

### Main software areas

| Area | Location | Responsibility |
| --- | --- | --- |
| Application startup | `src/main.c` | Starts modules and connects entrance occupancy to room control. |
| Entrance rules and hardware | `components/entrance/`, `include/entrance/` | RFID scanning, user checks, occupancy, schedule, keypad, LCD, and dashboard. |
| Room control and communication | `components/trevor/`, `include/trevor/` | PIR sampling, delayed lights, room messages, retries, snapshots, and diagnostics. |
| SD storage | `components/storage/`, `include/storage/` | Shared SPI setup, SD mount, event log, and registered-user roster. |
| Serial menu | `components/cli/`, `include/cli/` | UART0 queries, login/logout, schedule commands, and roster commands. |
| Authentication | `components/auth/` | Shared demonstration PIN verifier and failed-login control. |
| Scan feedback | `components/actuator/`, `include/actuator/` | Different LED pulse lengths for granted and denied scans. |
| Shared definitions | `include/common/` | Data types used across modules. |

## 3. Hardware and pin assignments

The combined circuit uses an ESP32 development board, two MFRC522 readers,
one 20x4 I2C LCD, one 4x4 keypad, two PIR sensors, two room-light LEDs, a link
switch, a microSD card, and one blue feedback LED.

| Device or signal | GPIO assignment |
| --- | --- |
| Room 1 PIR / light | 27 / 26 |
| Room 2 PIR / light | 33 / 25 |
| Simulated room-link switch | 32 |
| Shared SPI SCK / MOSI / MISO | 18 / 23 / 19 |
| Entry RFID chip select / shared reset | 5 / 4 |
| Exit RFID chip select / shared reset | 13 / 4 |
| microSD chip select | 2 |
| LCD SDA / SCL | 21 / 22 |
| Keypad rows | 14, 15, 16, 17 |
| Keypad columns | 34, 35, 36, 39 |
| Feedback LED | 12 |
| USB serial console | UART0, 115200 baud |

The two RFID readers and SD card share the SPI data and clock wires and use
separate chip-select signals. The keypad columns need external pull-up
resistors. GPIO2 and GPIO12 are ESP32 boot-strapping pins; the circuit includes
pull resistors to keep their startup levels defined. The diagram is a
simulation design, so verify these pins and electrical levels before building
physical hardware. The room LEDs and feedback LED are indicators; they are not
door locks or mains-light drivers.

## 4. Software operation from startup to normal use

1. **Load settings.** ESP-IDF starts the application. NVS provides persistent
   schedule settings and a backup of the user roster.
2. **Start storage.** The combined build initializes the shared SPI bus and
   attempts to mount the SD card. If mounting fails, the firmware reports the
   problem and local entrance and room logic can continue.
3. **Start room control.** Trevor's tasks sample the two PIR sensors, control
   their light outputs, and send room-state frames through FreeRTOS queues.
4. **Start entrance control.** Leon's runtime initializes the readers, display,
   keypad, clock synchronization, schedule monitor, and entrance state. The
   firmware loads a saved roster when available; otherwise it uses two demo
   cards (`DE:AD:BE:EF` and `11:22:33:44`).
5. **Mark occupancy state.** The inside list is held in RAM. After a reboot,
   the firmware treats the count as uncertain until staff check the building
   and an administrator uses `confirm-empty`.
6. **Connect the modules.** The occupancy bridge sends the entrance count to
   room control. Room lights are forced off when the count is known to be zero;
   an unknown count is not treated as an empty building.
7. **Start administration.** The UART0 task accepts status and management
   commands. The LCD and Wi-Fi page provide additional status and schedule
   interfaces.

### RFID access decisions

The entry and exit readers return card UIDs to the entrance runtime. The core
checks that the UID belongs to an enabled user, checks the user's current
inside status, checks that the clock is valid, and checks that the schedule is
open. Only then does it update the inside list. A repeated entry and an
exit-without-entry are denied. Every accepted or denied access result is
available to the display and event logger.

The coursework requires the same schedule to restrict both doors. Therefore,
after closing time a person already recorded inside is denied an exit scan and
remains in the inside list until the schedule opens. The schedule monitor logs
and reports any people still recorded inside at closing time. The system
cannot determine from RFID alone whether another person followed a cardholder;
keypad `B` is a labelled simulated tailgate alert, not a real passage sensor.

### Schedule and authentication

The schedule defaults to 08:00–18:00 Africa/Kampala time. SNTP supplies the
clock. When time is unavailable, both entry and exit fail closed. Schedule
changes are available through the keypad, Wi-Fi dashboard, and serial CLI;
all three use the same schedule API and the saved hours are written to NVS.

The presentation PIN is `2468`. The keypad, dashboard, and CLI use the shared
verifier, with an authenticated serial session lasting 60 seconds. The PIN and
HTTP dashboard are demonstration features, not production security credentials.

## 5. Occupancy and intelligent lighting

Each room has a PIR input and a light output. Motion switches the light on
immediately. When motion stops, the light remains on for its configured delay
(five seconds by default); renewed motion restarts the delay. The PIR indicates
activity and does not count people.

The building count comes from successful RFID entry and exit transitions. When
the count is authoritatively zero, the combined demo turns room lights off. If
occupancy is uncertain, the system does not assume the building is empty. A
room-link outage makes remote status stale but does not stop local room light
control.

The safety feature set also includes anti-passback alerts, a read-only keypad
roll-call on `C`, and a closing-time report. These improve monitoring, but the
team has not recorded a formally approved innovation selection in the project
issues.

## 6. Data storage and serial management

The SD card is mounted at `/sdcard`. The system appends access and room events
to `/sdcard/events.csv`. The registered-user roster is stored on the card with
an NVS backup. A FreeRTOS storage worker is the single event-file writer, and
the file mutex coordinates that writer with serial reads and roster operations.
Queue drops and failed/unavailable writes are counted. SD-full, missing-card
recovery, and reboot restoration behavior still need live verification. The
live inside list itself is not restored after reboot.

The UART0 menu is available at 115200 baud:

| Command | Purpose |
| --- | --- |
| `help` | List available commands. |
| `status` | Show system and room synchronization status. |
| `inside` | Show the current inside list and occupancy count. |
| `rooms` | Show room motion, light, delay, and online status. |
| `users` | Show registered users. |
| `records` | Read recent SD event records. |
| `schedule` | Show current access hours. |
| `login 2468` / `logout` | Start or end the demonstration administrator session. |
| `open HH:MM` / `close HH:MM` | Change schedule hours while logged in. |
| `useradd NAME UIDHEX` / `userdel ID` | Manage the roster while logged in. |
| `confirm-empty` | Confirm a physically checked empty building after login. |

## 7. Resource synchronization and FreeRTOS

FreeRTOS runs independent tasks for room sampling, coordinator communication,
storage, entrance scanning, keypad input, display updates, schedule checks,
serial commands, and status reporting. This prevents one slow task, such as an
SD write, from being performed inside the room-sampling loop.

Queues pass complete work items between tasks. The logical room links use
bounded queues for state frames; the storage queue holds up to 16 events. Queue
sends from time-sensitive control paths are non-blocking. When full, the system
counts the drop instead of waiting indefinitely.

Mutexes protect data that multiple tasks can read or change. The entrance
mutex protects the inside list and schedule; the room mutex protects room
snapshots; the SD mutex serializes file access; and smaller UI/session mutexes
protect display, keypad, and dashboard state. The current implementation uses
mutex-type FreeRTOS semaphores for these locks; it does not use a separate
binary or counting semaphore. Shared values are read through module APIs so
one module does not edit another module's internal state.

## 8. Group member details and contribution areas

The table maps the module responsibilities in the project work distribution
and repository ownership documentation. It describes the current code areas;
it is not independent proof of who personally wrote each line.

| Member | Identification details | Project responsibility and corresponding code area |
| --- | --- | --- |
| Leon Nsitta Kalungi | Registration No. `25/U/24111`; Student No. `2500724111` | Project setup, entrance RFID rules, schedule, occupancy, display/keypad/dashboard, and system integration. `components/entrance/`, `src/main.c`. |
| Kato Trevor | Registration/Student No.: **to be supplied by the member** | Room sensing and lighting, room communication, retries, synchronization, and event queue. `components/trevor/`. The Git history includes Trevor's initial subsystem commit. |
| Kagimu Edward | Registration/Student No.: **to be supplied by the member** | SD event storage, registered-user roster, UART0 menu, and authenticated administration. `components/storage/`, `components/cli/`. Confirm personal completion/authorship with Edward before submission. |
| Ahurira Grace | Registration/Student No.: **to be supplied by the member** | Scan-feedback LED and its GPIO interface. `components/actuator/`. Confirm personal completion/authorship with Grace before submission. |

**Authorship note:** Recent integration, formatting, code, and documentation
changes were prepared with coding-agent assistance and committed using the
repository's configured Leon identity. Trevor's room subsystem also has a
prior commit attributed to Trevor. The report does not claim that Edward or
Grace personally authored their assigned code solely because it is stored in
their ownership folders. Each member should review their section and confirm
the exact work they completed before the group submits it.

## 9. Verification and known limitations

- The entrance rules and Trevor's portable core tests pass on the host.
- PlatformIO builds pass for `esp32dev`, `coordinator`, `room1`, and `room2`.
- A Wokwi entry/exit scenario previously mounted the SD card, granted a demo
  card's entry and exit during open hours, returned occupancy to zero, and
  wrote both access events.
- A later expanded Wokwi test ended with WebSocket error 1006 before its serial
  assertions completed. Schedule denial at the exit door, serial administration,
  roster persistence, empty-building shutdown, LED pulses, SD failure recovery,
  and reboot behavior remain to be demonstrated.
- The one-board diagram demonstrates logical modules, not three physically
  connected ESP32s. The feedback LED is not a door lock, the tailgate marker is
  simulated, and the live occupancy list is volatile.

The diagram, firmware, test commands, full module documentation, and this
report are in the [GitHub repository](https://github.com/ledge-stack/Embedded-Course-Work)
on the [`feat/system-presentation-guide` branch](https://github.com/ledge-stack/Embedded-Course-Work/tree/feat/system-presentation-guide).
