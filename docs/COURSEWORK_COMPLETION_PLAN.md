# Coursework implementation status and completion plan

**Status date:** 1 November 2026
**Submission target recorded in the project plan:** 2 November 2026
**Purpose:** Record what is in the repository now, what remains to finish the coursework, and a proposed work split for the new member, Ahurira Grace.

The coursework plan says Wokwi simulation and PlatformIO builds are the required demonstration. Label every result as a Wokwi run, host test, or build; physical hardware is not required for the planned submission. PR #23, which added the roll-call and roster handoff, has merged into `main`. All four environments built successfully on 31 October after the PIN-verifier correction; the three separate role builds warn that the SDK flash size is 2 MB while the selected board specifies 4 MB. The RFID-reader probe detected the demo green card at both readers. The full entry/exit scenario was run after the 08:00-18:00 Kampala window and correctly denied entry, so a granted occupancy round trip still needs to be demonstrated. The local `sdkconfig.esp32dev` update remains uncommitted and is not included in Leon's code changes.

## Current implementation

| Area | Implemented in the repository | Still required for coursework completion |
| --- | --- | --- |
| Entrance and access rules | Two MFRC522 readers; demo-user UID allowlist; entry/exit decisions; inside list/count; anti-passback and duplicate/stale event rejection; Kampala schedule with SNTP; entry denied if time is unavailable or the schedule is closed; an authorized person already inside can exit after closing. | Edward still needs to supply the persistent user source and call Leon's roster API. Grace's simulated door outputs remain separate. Demonstrate schedule boundaries and failure behavior in Wokwi. |
| Entrance operator interface | LCD status, keypad schedule changes, PIN verification with lockout, authenticated Wi-Fi status/schedule dashboard, and NVS schedule persistence. | The PIN/dashboard are presentation-only. Do not describe them as production-grade authentication. Demonstrate keypad and dashboard flows after final integration. |
| Lecture rooms | Two PIR inputs and light outputs; immediate light-on on motion; configurable off-delay; room state protocol with CRC, sequence/session checks, acknowledgements, retries, stale/offline detection, and reconnect snapshots. | PIR detects motion, not continuous occupancy or people count. Confirm the brief accepts this approximation or document/implement a presence sensor. Run the room scenarios in the final demo. Remote schedule/building-count replication and remote room-delay configuration are not implemented. |
| Synchronization and storage boundary | FreeRTOS task ownership, queues, a mutex-protected snapshot, bounded message queues, a serialized storage consumer, access-event enqueue API, and diagnostics for queue drops and unavailable writes. | The default SD callback reports failure. Queueing an event does not mean it was saved. Implement and verify SD writes, read/configuration ownership, missing/full-card handling, and persistence across reboot. |
| Communications | Trevor's room protocol and `coordinator`, `room1`, and `room2` firmware roles exist. The default `esp32dev` presentation build runs one ESP32 with two logical rooms and a simulated link switch. | Demonstrate link loss and recovery with the Wokwi switch, check each room role's restart behavior with a host test or separate build/run, and save evidence. The one-board Wokwi demo is not three physical terminals. Access-event transport between terminals is not implemented. |
| Circuit and build | `diagram.json` contains a one-board demo with the ESP32, two room PIR/light pairs, link switch, two RFID readers, LCD, keypad, and an SD-card part. PlatformIO has `esp32dev`, `coordinator`, `room1`, and `room2` environments. All four environments built successfully on 31 October after the PIN-verifier correction. | The SD part is not driven by firmware and the actuator outputs are not yet in the diagram. Publish/open the shared Wokwi project and record its URL; run the final demo. Resolve the separate role builds' 2 MB versus 4 MB flash-size warning with the team. |
| Tests and evidence | A portable Leon entrance-core test passes for access, anti-passback, safe exit, schedule, and clock rules. Trevor's portable core tests and host adapter tests are also present. The Wokwi reader probe detected the green demo card at both readers. | The full Wokwi entry/exit scenario still needs to show a granted entry and return to zero occupancy during open hours. Add evidence for SD, CLI, actuator, safety, and end-to-end behavior after the missing modules arrive. Keep host tests, simulator results, and physical hardware results clearly distinguished. |
| Report and presentation | Project README, implementation guides, interface notes, and presentation outline exist. | Finish the operation report, all member contribution statements, video walkthroughs, final presentation, shared simulator link, and submission package. Confirm the submission portal and required format with the course team. |

## Work remaining by member

### Leon Nsitta — entrance owner and integration coordinator

**Already implemented:** Entrance RFID and decision logic, occupancy/anti-passback, access schedule and clock handling, LCD, keypad, Wi-Fi dashboard, schedule persistence, shared system types, combined demo startup, pin map, and one-board circuit.

**Implemented in this update:**

- Keypad `C` prints a read-only evacuation roll-call to serial.
- The schedule watcher prints the same report as an after-hours alert when people remain inside at closing.
- Anti-passback and exit-without-entry denials now get explicit alert labels in the serial log; the LCD shows an after-hours still-inside alert.
- Keypad `B` marks a simulation-only tailgate and changes the display to show occupancy uncertainty. It does not add a person or claim automatic detection.
- `entrance_runtime_set_users()` is the roster handoff for Edward's user store. It checks UIDs and preserves the identity of anyone currently recorded inside. Edward's store is not present yet, so the demo roster remains active at startup.
- All four PlatformIO environments build from this branch.

**Still to finish:**

1. Integrate `entrance_actuator_signal()` calls after every access decision when Grace's actuator module is available. Her module is not in this checkout yet; do not replace it with a Leon-owned actuator implementation.
2. Integrate Edward's persistent roster at startup and after user changes by calling `entrance_runtime_set_users()`. His user-store module is not in this checkout yet.
3. Keep the existing schedule API available for Edward's authenticated commands. `entrance_runtime_set_schedule()` already validates and persists hours; his command handler must authenticate before calling it.
4. Publish/open the shared Wokwi project and add its URL to the README. The Wokwi CLI reader probe detected the green card at both readers. The full entry/exit scenario was denied because the simulation ran after the configured access hours; no granted occupancy round trip or interactive browser demonstration is recorded yet.
5. Run the full integrated acceptance scenarios after Grace's and Edward's modules arrive. The report section draft is in [`LEON_REPORT_SECTION.md`](LEON_REPORT_SECTION.md); add the Wokwi URL and observed evidence after the run.

### Kato Trevor — room terminals and communications

**Already implemented:** Room motion/off-delay logic; room state frame format and CRC; parser recovery; session/sequence validation; acknowledgement/retry/heartbeat behavior; coordinator snapshots and offline state; physical UART role builds; task/queue/mutex ownership; serialized storage queue and access-event enqueue interface; host tests.

**To finish:**

1. Demonstrate both room lights, simultaneous room activity, renewed motion before timeout, and the configured off-delay in the final demo.
2. Demonstrate link loss/recovery with the Wokwi switch and check each role's restart behavior in a host test or separate role run. Save the results. Do not describe the logical one-board Wokwi queues as three physical terminals.
3. Coordinate any needed access-event or authoritative occupancy/schedule messages with Leon and Edward. The current room protocol does not carry these; agree message fields/versioning before extending it.
4. Decide and implement Trevor's approved innovation feature if the assignment requires one per member; record the team's choice before calling it complete.
5. Review the storage callback integration with Edward and verify that room control remains responsive when the SD card is missing, full, or slow.

### Kagimu Edward — persistent storage, users, and serial administration

**Already available to integrate:** Trevor's single-consumer storage callback and bounded queue; Leon's compact access event payload and schedule API; a demo PIN verifier for the presentation keypad/dashboard. These are interfaces, not Edward's completed persistence or serial-admin features.

**To implement:**

1. Implement the SD component and provide the strong `tr_storage_append(const tr_storage_event *)` callback. Persist access, room/light, and required system events. Define a stable record format with timestamps and document it.
2. Make SD access single-owner/serialized, including reads and configuration writes. Handle card absent, mount failure, full card, write failure, and recovery without crashing or blocking local room control. Surface failures in status/diagnostics and verify actual files and reboot persistence.
3. Replace the fixed demo-user allowlist with the agreed registered-user source and add supported user-management operations through APIs; Leon's entrance runtime remains owner of live occupancy.
4. Implement the UART0 serial help/status/read menu for people inside/count, room status/lights, access records, system health, and schedule.
5. Implement authenticated admin sessions for user/schedule changes, including verifier storage (not plaintext credentials), failed-login recording, a retry limit/lockout, logout or session expiry, and authorization checks at the command boundary. Coordinate schedule mutation with Leon's API rather than editing entrance state directly.
6. Demonstrate concurrent event producers and missing/full-card behavior; verify no overlapping writes, silent event loss, or room-control stalls.

### Ahurira Grace — entrance hardware and feedback owner

The 1 November completion plan assigns Grace the door mechanisms, indicators, buzzer, tailgate input and their Wokwi evidence. Leon's runtime owns access decisions and calls her actuator API; it does not own the inside list or schedule.

1. Add an entrance output/actuator module (for example, `components/entrance/actuator.c` with a small public header) that receives grant/deny outcomes from the entrance runtime and drives the correct entry/exit door indicator or simulated actuator. Use safe inactive defaults on startup and on errors.
2. Add the corresponding components and pin assignments to `diagram.json` and `docs/INTERFACES.md`. Check for pin conflicts with the existing RFID, LCD, keypad, room, and future SD SPI assignments.
3. Add denial feedback (such as a buzzer/indicator) and implement a clearly labelled simulation-only tailgate trigger if Leon's safety design uses that trigger. Coordinate the event contract with Leon; the simulated trigger must not be presented as actual tailgate sensing.
4. Add a reproducible demo checklist and evidence showing granted entry, granted exit, denied entry/exit, and safe outputs after reboot/error. If real door hardware is used, document the driver/isolation and do not drive a load directly from an ESP32 GPIO.
5. Review whether the course brief requires a separate innovation feature for Grace. Door actuation and denial feedback satisfy interface/hardware needs but should not automatically be counted as an innovation feature.

If Grace is assigned another role by the team, update this section and the README together so the report and code ownership agree.

## Shared integration tasks

These tasks need more than one owner and are not complete when a single module compiles:

1. **Agree interfaces:** Confirm the versioned shared types and ownership in `docs/INTERFACES.md`. Decide whether remote occupancy, schedule, access-event messages, and room configuration are required by the coursework demonstration. Avoid creating a second writer for inside count, schedule, room state, or SD files.
2. **Finish the simulation circuit:** Add and review door outputs and the SD card using Wokwi parts; check the shared SPI wiring, pull-ups, boot-safe levels, and pin assignments in the diagram. Publish and open the Wokwi project. Its one-board limit means it cannot show three separate ESP32s in one simulation.
3. **Run end-to-end scenarios:** Exercise authorized and unauthorized entry/exit, anti-passback, schedule open/closed and clock fault, both room lights and delay, link outage/reconnect, serial reads/admin changes, SD success/failure, concurrent updates, and the safety/evacuation flow. Save logs/screenshots or video and label whether each result came from a host test, Wokwi, or physical hardware.
4. **Verify release build:** Build all intended PlatformIO roles from a clean final revision. Confirm `diagram.json` parses and the firmware/ELF paths match `wokwi.toml`. Review the generated `sdkconfig.esp32dev` change; commit it only if the team confirms it is a deliberate, compatible project configuration update.
5. **Finish coursework package:** Complete the system operation report (architecture, hardware, software, limitations, setup/use, test evidence, Wokwi URL); add all four members' names and contribution statements; collect one walkthrough video per member and prepare the final presentation. Submit by the current course deadline and retain submission confirmation.

## Completion gate

The team can call the implementation complete when every required behavior above has an owner, the missing modules are integrated, the end-to-end scenarios have recorded evidence, the final builds pass, and the report/videos/submission package are complete. A host test or successful firmware build alone does not prove physical UART, SD-card, Wokwi, or full-system behavior.

## Source notes

- Original requirement and issue mapping: [`IMPLEMENTATION_PLAN.md`](IMPLEMENTATION_PLAN.md).
- Current module ownership and limits: [`LEON_WORK.md`](LEON_WORK.md), [`TREVOR_GUIDE.md`](TREVOR_GUIDE.md), and [`INTERFACES.md`](INTERFACES.md).
- Leon's planned safety feature: [`LEON_ENTRANCE_SAFETY_DESIGN.md`](LEON_ENTRANCE_SAFETY_DESIGN.md).
- Existing demo sequence: [`PRESENTATION_DEMO.md`](PRESENTATION_DEMO.md).
