# Repository Structure Guide

This guide is a map of the CoCIS coursework repository. Start at the root and
follow the path from the application entry point to the module that owns the
feature you want to understand.

## Repository at a glance

```text
Coursework/
├── src/                 Application startup and integration
├── components/          Firmware implementations, grouped by module/owner
├── include/             Public headers used between modules
├── lib/                 Place for optional project-specific PlatformIO libraries
├── scenarios/           Wokwi CLI interaction scripts
├── sdcard/              Starting contents for the simulated SD card
├── docs/                Reports, architecture notes, and presentation guides
├── test/                Portable host-side logic tests and test shims
├── scripts/             Helper scripts for local test workflows
├── diagram.json         Wokwi circuit and simulated component IDs
├── wokwi.toml           Firmware and SD-card mapping for Wokwi
├── platformio.ini       Build environments and compile-time role selection
├── CMakeLists.txt       ESP-IDF project build entry
└── sdkconfig.*          ESP-IDF settings for the build roles
```

## The most important distinction: `components/` and `include/`

`components/` contains the C implementation: the instructions the ESP32 runs.
For example, `components/entrance/runtime.c` manages RFID access and
`components/trevor/runtime.c` manages room control and communication.

`include/` contains public `.h` header files. A header tells another module
which functions and shared data types it may use. For example,
`include/entrance/runtime.h` declares the entrance APIs. Storage, CLI, and the
room runtime can call those APIs without reading or changing the entrance's
private variables. This separation helps keep one clear owner for each piece of
live state.

Each ESP-IDF component has a `CMakeLists.txt` file. It lists the source files
that belong to that component, public include paths, and other components or
ESP-IDF drivers it depends on.

## `src/`: the application start point

- `src/main.c` contains `app_main()`, the ESP-IDF entry function.
- It initializes NVS and starts the storage, room, actuator, entrance, and CLI
  modules in order.
- In the combined Wokwi build, it also creates an occupancy bridge task. This
  bridge reads the entrance's building count and passes the count and its
  certainty to Trevor's room controller every 250 ms.
- `src/CMakeLists.txt` registers the application and its module dependencies.

Most feature code does not belong in `main.c`. `main.c` connects modules; the
module folders implement them.

## `components/`: firmware modules and ownership

| Folder | Owner or role | What its source code does |
| --- | --- | --- |
| `components/entrance/` | Leon Nsitta | `core.c` contains testable access rules. `runtime.c` coordinates two RFID readers, occupancy, schedule, logs, and tasks. `mfrc522.c` drives RFID readers. `display.c` updates the LCD. `keypad.c` handles keypad state and admin actions. `dashboard.c` serves the Wi-Fi page and routes. |
| `components/trevor/` | Kato Trevor | `core.c` contains room/light and message rules. `runtime.c` runs the room tasks, coordinator, queue-based storage worker, retry logic, status counters, and logical or UART message links. |
| `components/storage/` | Kagimu Edward | Initializes shared SPI, mounts the microSD card, appends CSV events, and commits the authoritative user roster in NVS with an SD mirror. |
| `components/cli/` | Kagimu Edward | Reads UART0 commands, prints status/records, and gates schedule and roster changes behind admin login. |
| `components/actuator/` | Ahurira Grace | Controls the blue scan-feedback LED and its grant/deny pulse durations. |
| `components/auth/` | Shared | Checks the demonstration PIN, protects failed-login state, and applies a temporary lockout. |
| `components/common/` | Shared build boundary | An ESP-IDF component target for shared code dependencies. Shared project data types are declared in `include/common/system.h`. |
| `components/rooms/` | Trevor interface boundary | Compatibility component target that exposes the room interfaces implemented in `components/trevor/`. |
| `components/comms/` | Trevor interface boundary | Compatibility component target that exposes communication interfaces implemented in `components/trevor/`. |

Every component also has a `README.md` and `CMakeLists.txt`. The README says
who owns the module and what it does. The CMake file tells ESP-IDF how to build
it. The `rooms/` and `comms/` boundaries do not duplicate Trevor's source code.

## `include/`: public module contracts

| Folder | Header purpose |
| --- | --- |
| `include/entrance/` | Public access rules, runtime snapshots, display, keypad, dashboard, and RFID-driver functions. |
| `include/trevor/` | Public room rules, messages, state snapshots, and room-runtime functions. |
| `include/storage/` | Public SD initialization, availability, record reading, and roster load/save functions. |
| `include/cli/` | Public CLI startup function. |
| `include/actuator/` | Public LED startup, signal, and update functions. |
| `include/auth/` | Shared PIN verifier API. |
| `include/common/` | Shared types and constants such as users, events, UIDs, and schedules. |

Each include subfolder has a short README. There is not a separate
`include/rooms/` or `include/comms/` tree: those ESP-IDF target names are
compatibility boundaries, while Trevor's API headers are under
`include/trevor/`.

## `lib/`: optional local libraries

`lib/` is PlatformIO's place for project-specific reusable libraries. It is
currently empty apart from the PlatformIO example README. The current firmware
uses ESP-IDF components under `components/` and public headers under `include/`,
so no custom PlatformIO library is required. This folder exists for a future
standalone helper library; it is not where the current core system lives.

## `scenarios/`: repeatable Wokwi interactions

These YAML files tell Wokwi CLI which simulated controls to operate and which
serial messages to wait for. They are simulation scripts, not firmware source.

| Scenario | Intended check |
| --- | --- |
| `boot-check.yaml` | Wait for entrance startup and SNTP synchronization. |
| `rfid-reader-probe.yaml` | Check that both simulated RFID readers detect the demo card. |
| `rfid-entry-exit.yaml` | Demonstrate a registered card entering and then exiting during open hours. |
| `rfid-after-hours.yaml` | Demonstrate schedule denial after closing and rejection of an unknown card. |
| `presentation-smoke.yaml` | Broader flow including a grant, repeat-entry denial, exit, and unknown-card denial; its full run is not yet confirmed. |

Scenarios use the part IDs from `diagram.json`, such as `entry_rfid` and
`exit_rfid`. The schedule affects the result, so a grant scenario must run when
the simulated Kampala clock is within the allowed hours.

## Circuit, build settings, and SD card

- `diagram.json` defines the simulated ESP32, readers, keypad, LCD, PIRs,
  indicators, switches, wiring, and part IDs.
- `wokwi.toml` tells Wokwi which compiled firmware to load and maps `sdcard/`
  into the simulated card.
- `platformio.ini` defines the `esp32dev`, `coordinator`, `room1`, and `room2`
  build environments. `TR_ROLE` selects the Trevor runtime role. `esp32dev`
  (`TR_ROLE=0`) is the combined one-board demo.
- `sdkconfig.esp32dev`, `sdkconfig.coordinator`, `sdkconfig.room1`, and
  `sdkconfig.room2` hold ESP-IDF configuration for their respective builds.
- `sdcard/` contains the initial Wokwi SD-card files. Runtime records are
  written to the simulated card; generated output should not be mistaken for
  source code.

## Tests and project documents

- `test/` contains portable C tests for access rules, room/message logic, and
  selected runtime adapters. `test/README` lists commands and limits.
- `scripts/` contains helper scripts for the host-test workflow.
- `docs/` contains the report, ownership map, interface contract, completion
  record, design notes, and presentation guides. Start at `docs/README.md`.
- `.github/` holds repository contribution templates. `.vscode/` holds editor
  settings. `.pio/` is generated build output and is not handwritten source.

## A simple navigation route

1. Read `README.md` for the project overview and run commands.
2. Read this guide to locate the feature folder.
3. Read that folder's README and public header under `include/`.
4. Follow the corresponding `.c` file in `components/`.
5. Use `src/main.c` and `docs/MODULE_INTEGRATION_GUIDE.md` to see how the
   modules are connected.
6. Use `docs/COURSEWORK_COMPLETION_PLAN.md` for the dated evidence and checks
   that still need to be shown.
