# Single-board simplification implementation plan

> **For agentic workers:** Use superpowers:executing-plans to implement this plan task by task. Steps use checkboxes for tracking.

**Goal:** Make the approved one-ESP32 project easier to explain while preserving the keypad, web dashboard and LCD.

**Architecture:** Keep the team module boundaries and public entrance/storage interfaces. Replace simulated UART frames, acknowledgements, retries, boot sessions and coordinator modes with local room snapshots and one typed FreeRTOS event queue consumed by the existing single SD writer. Room tasks control their own lights and publish changed states; entrance state remains protected by its existing mutex and schedule API.

**Tech stack:** ESP32, PlatformIO esp32dev, ESP-IDF, FreeRTOS, SPI RFID/SD, I2C LCD, GPIO keypad and LED door indicators, Wi-Fi SNTP/HTTP.

**Spec:** User-approved comparison in this chat, amended on 3 November to retain the keypad and web dashboard. The existing LCD and all three authenticated management interfaces stay. Required RFID, occupancy, schedule, SD/NVS, synchronization and light-use behavior stays. Preserve a source backup before edits; work directly in the user-specified Desktop repository. No commit, push or publication is requested.

## Global constraints

- Preserve the 2 November keypad, HTTP form, CLI, schedule-lock and roster fixes.
- Preserve default hours 08:00-18:00, PIN 2468, both RFID readers and current roster API.
- Preserve existing sensor, RFID, keypad and LCD pins. Remove the logical-link switch; use its GPIO32 for the entry door indicator, and existing LED GPIO12 for exit. Retain GPIO12's pulldown.
- Only granted scans open the corresponding door indicator for three seconds. A timer task closes indicators independently of slow SD writes. Denials remain visible on LCD, serial and records.
- Keep room control responsive even if SD writes fail or the queue fills; expose queue-drop/write-failure counters.
- Keep occupancy uncertainty after restart; do not add a new presence persistence model.

## Review focus

- Both room lights and their delayed-off/confirmed-empty behavior must survive queue pressure and failed SD writes.
- Durable access and schedule requests must receive their own completion result, including after an earlier timeout.
- Keypad and dashboard must continue calling the same authenticated schedule API.
- Door outputs must remain separate from keypad pins and close without waiting for storage.
- Active documentation and simulator controls must not claim removed CRC/ACK/UART roles.

## Task 1: Preserve and establish proof

- [x] Save and verify the complete current source/document archive outside the repository.
- [x] Preserve timing/usage tests and replace protocol/outage tests with direct snapshot, queue-pressure and durable-completion tests.
- [x] Add failing host tests for separate granted door pulses, independent closure and denied-scan behavior.

## Task 2: Simplify the room runtime

Files: `components/trevor/core.c`, `runtime.c`, their headers, storage room-row formatting, CLI room output, startup and PlatformIO configuration.

- [x] Keep pure room/off-delay/usage functions; remove the wire protocol.
- [x] Keep two room tasks, one event/storage worker and a mutex-protected local snapshot.
- [x] Preserve storage APIs and task notifications; improve the host shim so durable replies can be tested.
- [x] Remove alternate physical-role build branches and simulated-link status/counters.
- [x] Run core/runtime/entrance and previous regression tests.

## Task 3: Door indicators and circuit

Files: `components/actuator/runtime.c`, public header, entrance grant call, `diagram.json`.

- [x] Open GPIO32 for granted entry and GPIO12 for granted exit; keep denials closed.
- [x] Run a short independent door timer task; close both outputs when the access window closes.
- [x] Remove the link switch and add an entry LED/resistor; label the existing LED as exit.
- [x] Run actuator tests and validate connections and retained keypad/LCD parts.

## Task 4: Explain and verify

- [x] Update active module READMEs, report, pin/interface contract and presentation guides to the simpler local event flow.
- [x] Preserve historical verification notes with a clear superseded notice.
- [x] Build esp32dev directly in the actual folder; run all applicable host suites and check the diff.
- [x] Request one independent final source review and resolve important findings.
- [x] Report actual before/after application and Trevor-module line counts and live-test limits.

## Execution rulings

- User explicitly chose direct work in this folder, so an external verified archive replaces creating an isolated worktree.
- No commits are made during this workflow because the user's requested result is local working files.

- Ruling: keep the existing access-payload encoding and durable-completion APIs to preserve storage compatibility; only the room wire protocol is removed.
