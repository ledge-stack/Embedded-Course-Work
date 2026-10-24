# CoCIS Smart Control and Monitoring System

ESP32 based coursework project for a pilot covering the CoCIS main entrance and two lecture rooms. The design follows the coursework brief and the team's work distribution document. The target stack is ESP32 with ESP-IDF, developed in PlatformIO and demonstrated in Wokwi.

## Team and work areas

| Member | Primary area |
| --- | --- |
| Leon Nsitta | Project setup, entrance terminal, access schedule and integration |
| Kato Trevor | Lecture room terminals, inter-terminal communication and synchronization |
| Kagimu Edward | SD card storage, serial menus and authentication |

## Current status

The repository is at the ESP-IDF starter stage. `src/main.c` contains only an empty `app_main`, and `diagram.json` is empty. The implementation plan and GitHub issue sequence are in [`docs/IMPLEMENTATION_PLAN.md`](docs/IMPLEMENTATION_PLAN.md). No hardware behavior is implemented yet.

## Build and simulation

This project uses PlatformIO's `esp32dev` environment with the ESP-IDF framework.

```sh
pio run
```

Open the project in Wokwi after adding the circuit to `diagram.json`. [`wokwi.toml`](wokwi.toml) points Wokwi to the PlatformIO build outputs under `.pio/build/esp32dev/`. Build successfully before starting the simulation so those firmware files exist.

The Wokwi project link will be added here when the shared simulation is created.

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

The planned module boundaries are `entrance`, `rooms`, `comms`, `storage`, `cli`, `auth` and `common`. The implementation plan defines the interfaces and work sequence before those modules are added.
