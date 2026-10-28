# CoCIS Smart Control and Monitoring System

ESP32 based coursework project for a pilot covering the CoCIS main entrance and two lecture rooms. The design follows the coursework brief and the team's work distribution document. The target stack is ESP32 with ESP-IDF, developed in PlatformIO and demonstrated in Wokwi.

## Team and work areas

| Member | Primary area |
| --- | --- |
| Leon Nsitta | Project setup, entrance terminal, access schedule and integration |
| Kato Trevor | Lecture room terminals, inter-terminal communication and synchronization |
| Kagimu Edward | SD card storage, serial menus and authentication |

## Current status

Leon implemented the entrance RFID readers, occupancy and anti-passback
decisions, Kampala access schedule, shared interfaces, and one-board Wokwi
diagram in this change. `src/main.c` starts Leon's entrance runtime alongside
Trevor's existing room runtime. Trevor's room-control, communications,
synchronization, and storage-queue code remains his contribution under
`components/trevor/`; it is not claimed as Leon's work. Edward's SD persistence,
user management, and authenticated CLI remain outstanding. See
[`docs/LEON_WORK.md`](docs/LEON_WORK.md) for the contribution boundary and
[`docs/INTERFACES.md`](docs/INTERFACES.md) for contracts and pin assignments.

## Build and simulation

This project uses PlatformIO's `esp32dev` environment with the ESP-IDF framework.

```sh
pio run -e esp32dev
```

The demo joins `Wokwi-GUEST` and uses SNTP for the Kampala access schedule. Entry
fails closed if time is unavailable; an authorized person who is inside can
still exit. The demo RFID allowlist contains UID `DE:AD:BE:EF` and `11:22:33:44`.
Schedule changes are available through `entrance_runtime_set_schedule` for the
authenticated admin CLI integration; values are RAM-only until Edward's
configuration storage is connected.

Open the project in Wokwi after building. [`wokwi.toml`](wokwi.toml) loads the
`esp32dev` firmware from `.pio/build/esp32dev/`. The local circuit and signal
assignments are in [`diagram.json`](diagram.json) and
[`docs/INTERFACES.md`](docs/INTERFACES.md). Publishing and validating a shared
Wokwi project link remains a team action. The presentation sequence is in
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

The ESP-IDF build exposes `main`, `entrance`, `rooms`, `comms`,
`storage`, `cli`, `auth` and `common` component boundaries. Entrance
and common are implemented under `components/` with public headers under
`include/`. Trevor's room, transport and storage-queue implementation remains
in `components/trevor/`; the rooms/comms/storage targets expose integration
boundaries over his pre-existing implementation. The CLI and auth targets are
scaffolds for Edward's assigned work, not completed features.
