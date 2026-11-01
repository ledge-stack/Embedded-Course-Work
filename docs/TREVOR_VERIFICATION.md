# Trevor subsystem verification record — historical (27 October 2026)

This is a record of Trevor's earlier module checks. For the latest combined
firmware, integration evidence, and outstanding acceptance work, see
[`COURSEWORK_COMPLETION_PLAN.md`](COURSEWORK_COMPLETION_PLAN.md). The Windows
commands and environment details below describe the original verification
machine and are not the current Fedora workflow.

| Check | Result |
| --- | --- |
| Portable C behavior suite | PASS |
| Threaded runtime adapter suite on Windows | PASS |
| ESP-IDF demo firmware (`esp32dev`) | SUCCESS |
| ESP-IDF physical coordinator firmware | SUCCESS |
| ESP-IDF physical room 1 firmware | SUCCESS |
| ESP-IDF physical room 2 firmware | SUCCESS |
| Wokwi JSON syntax, IDs, endpoints and assigned GPIO mapping | PASS |
| Git whitespace check on implementation | PASS |

Tools: PlatformIO Core 6.2.0, Espressif32 platform 7.0.1, ESP-IDF 6.0.1, Xtensa GCC 15.2.0. Host tests used TinyCC 0.9.27 with `-std=c11 -Wall -Wextra -Werror`. The Windows adapter uses native threads, mutexes and bounded queues behind a FreeRTOS test shim; GPIO, UART and NVS interfaces are stubbed. This verifies adapter logic, not electrical behavior or ESP32 scheduler timing.

Core coverage: immediate light-on; exact off-delay boundary; renewed motion; zero delay; 64-bit uptime; single-bit corruption in every frame byte; invalid fields; partial/noisy frame recovery; retry timing; wrong ACK identity; retry expiry allowing fresh snapshots; duplicates; stale state; boot sessions; sequence rollover; stale timeout boundary.

Adapter coverage: simultaneous room activity; encoded coordinator snapshots; light outputs; link outage/offline status; local control during outage; reconnect; two concurrent access producers delivering 200 records through one non-overlapping callback; queue pressure/rejection with caller retry; callback failure reporting.

Reproduce host checks on Windows with `./scripts/test-trevor.ps1 -Compiler 'C:\path\to\tcc.exe'`. Build all roles with `pio run -e esp32dev -e coordinator -e room1 -e room2`.

The sandbox stalled when Ninja spawned compiler probes. For verification only, a local wrapper executed Ninja's expanded compiler-probe commands synchronously; SCons compiled and linked the firmware normally. The wrapper/local configuration are excluded from coursework. No compiler result was assumed. The SDK emitted upstream Kconfig notices and a pre-existing 4 MB board versus 2 MB SDK flash-size warning. Confirm the physical board size with the team; existing SDK/editor edits were excluded from our implementation commit.

## Pending team checks

- Interactive Wokwi execution: browser access stalled; successful visual simulation is not claimed. The diagram and built firmware are supplied for the VS Code extension.
- Three physical ESP32 UART wiring, separate clocks and reboot checks: not executed. Wokwi does not support multiple wired microcontrollers per simulation.
- Edward's SD/authentication and Leon's entrance/occupancy/schedule integration: outside this module. The default storage hook reports unavailable.
- A Google Drive or GitHub submission link, teammate review/PR, agreed innovation, final report and recorded video remain team deliverables. A Wokwi link is optional supporting evidence.

The working copy uses `feat/trevor-room-terminals`. Git metadata in the requested OneDrive repository was restricted, so source files were copied there without changing its branch, index or commits.
