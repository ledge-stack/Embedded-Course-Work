# Trevor's implementation and demonstration guide

Scope: Kato Trevor — lecture room terminals, inter-terminal communication and synchronization (backlog #4, #8, #9, #10). This is his part of the team project, not the completed entrance/storage/admin system.

## Build and run

From this repository in PowerShell:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -e esp32dev
```

`esp32dev` is a one-ESP32 demonstration with two room tasks and a coordinator task. Open the repository in VS Code and run **Wokwi: Start Simulator**. `wokwi.toml` loads this build. The circuit has two PIR sensors, two LEDs with 330-ohm resistors and a link-failure switch. The yellow LED represents room 1; green represents room 2. UART0 prints room and synchronization status every two seconds at 115200 baud.

Wokwi's [FAQ](https://docs.wokwi.com/faq) explicitly states that multiple wired microcontrollers in one simulation are unsupported. The default circuit is therefore labelled **three logical terminals on ONE ESP32**. It passes encoded binary frames through FreeRTOS queues. It does not verify UART electrical behavior, separate clock scheduling, or three physical boards. Do not describe it as a three-board simulation in the report.

Build the physical deployment separately:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -e coordinator -e room1 -e room2
# Select the appropriate COM port for EACH physical board before uploading:
# pio run -e coordinator -t upload --upload-port COM5
# pio run -e room1 -t upload --upload-port COM6
# pio run -e room2 -t upload --upload-port COM7
```

The commented COM ports are examples. `wokwi.toml` intentionally keeps the demo build; physical roles require separate boards.

## Hardware pins

| Terminal | Function | GPIO |
| --- | --- | --- |
| Room 1 | PIR OUT / light output | 27 / 26 |
| Room 2 | PIR OUT / light output | 33 / 25 |
| Coordinator | UART1 TX / RX to room 1 | 17 / 16 |
| Coordinator | UART2 TX / RX to room 2 | 19 / 18 |
| Each physical room | UART1 TX / RX | 17 / 16 |
| Demo only | Link switch common | 32 |

For room 1 connect coordinator 17 → room 1 GPIO16, room 1 GPIO17 → coordinator 16. For room 2 connect coordinator 19 → room 2 GPIO16, room 2 GPIO17 → coordinator 18. Connect all grounds. Each UART is point-to-point; never tie the two room TX outputs together. UART settings: 115200, 8 data bits, no parity, 1 stop bit, no hardware flow control. UART0 stays available for Edward's console. GPIO18/19 are often used for SPI, so Edward must coordinate SD pin assignments before integration.

Use 3.3 V compatible signal levels. LEDs demonstrate light outputs; actual room loads require appropriately rated driver/relay hardware. PIR supply depends on the chosen module; verify its output level before connecting it to the ESP32. The Wokwi sensor uses VIN for VCC, ground for GND, and OUT to the listed input.

The demo switch is right = link up, left = down. Common pin 2 connects to GPIO32, pin 1 to ground, pin 3 to 3V3. Bounce is disabled to make outage testing reproducible.

## Behavior and configuration

Inputs are sampled every 20 ms. Motion immediately sets occupancy true and light on. Each high sample refreshes the last-motion time. When the input falls, occupancy becomes false but light remains on for the configured delay. New motion extends the deadline. Delay is 5000 ms by default, range 0–3600000 ms. The simulated PIR itself holds high for one second; the software off-delay starts at the last high sample, not at the initial click. A PIR estimates activity; it cannot prove an empty room or count people.

Edward's authenticated command handler can call:

```c
esp_err_t err = tr_runtime_set_off_delay(1, 10000); // room 1: 10 seconds
tr_system_view status;
if (tr_runtime_snapshot(&status)) {
    // Use status.rooms[0].valid and .online before displaying its state.
}
```

The setter works only on the board owning that room, or both rooms in the demo. It returns `ESP_ERR_NOT_SUPPORTED` on a physical coordinator; remote configuration commands are not implemented. Authentication and persistent configuration are Edward's integration responsibilities. Delay changes are RAM-only and reset to default on reboot. Initialization runs once from app_main; other modules must call APIs after it completes.

## Message protocol, version 1

Frames are exactly 28 bytes. Integers are unsigned, little-endian. No C struct is transmitted directly.

| Offset | Bytes | Meaning |
| --- | --- | --- |
| 0 | 2 | Magic C0 C1 |
| 2 | 1 | Protocol version 1 |
| 3 | 1 | Kind 1 = STATE, 2 = ACK |
| 4 | 1 | Room ID 1 or 2 |
| 5 | 1 | Bit 0 occupancy, bit 1 light; other bits zero |
| 6 | 4 | Nonzero persisted boot session |
| 10 | 4 | Nonzero sequence number |
| 14 | 8 | Sender monotonic uptime in milliseconds |
| 22 | 4 | Off-delay in milliseconds, at most 3600000 |
| 26 | 2 | CRC-16/CCITT-FALSE over bytes 0–25, polynomial 1021, initial FFFF |

ACK echoes the accepted STATE payload with kind changed to 2 and CRC recomputed. Matching uses room, session and sequence; another room's ACK cannot clear a pending message. Coordinator checks that room ID matches the physical link. Unknown versions/types/flags, invalid IDs/zero identifiers, excessive delay and bad CRC are discarded. The UART parser scans a sliding window and recovers from noise, partial reads and truncated frames. Parsing work per poll is bounded.

Changed state is published once the pending delivery completes. A 2-second heartbeat sends full snapshots even without changes. An unacknowledged frame is sent up to five times, at 500 ms intervals, then counted as a timeout. A fresh snapshot follows; this avoids permanent stale state when only the ACK direction fails. Coordinator accepts newer sequences, acknowledges duplicates without reapplying/logging them, and rejects old sessions/sequences. After six seconds without an accepted or duplicate frame, the snapshot is offline. Offline values remain available as last known values and must not be shown as live.

Boot counters are committed to NVS namespace `trevor_link`, key `boot`, before tasks start. NVS initialization/commit errors stop startup rather than reuse identifiers; the code never erases team NVS automatically. Comparisons use serial-number arithmetic and assume less than 2^31 increments between observations. If a board's NVS is erased or replaced, restart the coordinator as part of re-pairing, because its previous session history is no longer valid. Uptime is not wall-clock time; Edward supplies timestamps for permanent records.

This is **eventual synchronization of latest room state**, not guaranteed delivery of every intermediate transition or a durable event journal. During outages, intermediate changes can be coalesced. Access records are handed directly to the storage queue by Leon's module; remote access-event transport and schedule replication are not defined by this room protocol. Agree those extensions with the team before integration.

## Synchronization and storage integration

`room_task` owns its room state, retry sender and ACK parser. `coordinator_task` owns both receiver streams. A priority-inheriting FreeRTOS mutex protects public snapshots, configuration and counters. Snapshot reads copy both rooms while holding that mutex. GPIO and UART operations do not hold it. A single `storage_task` consumes a 16-entry queue; no room-control task waits for SD writes. Logical transport uses an eight-frame queue per direction per room. These are task-context APIs, not ISR APIs.

Edward supplies a strong definition of `tr_storage_append(const tr_storage_event *)`, declared in `include/trevor/runtime.h`. It is called by exactly one task and returns true only on successful persistence. The weak default returns false. `storage_unwritten` counts failed/unavailable writes. `storage_queue_drop` counts rejected queue admissions. These counters reset on reboot; records are not retained for retry after a failed callback. Do not claim SD persistence until Edward's implementation is connected. All other SD reads/configuration writes must share the same storage owner (or Edward must provide his own lock around every SD operation); the hook alone cannot protect unrelated direct filesystem calls.

Leon can enqueue up to 48 bytes of an access event through `tr_runtime_record_access(data, length)`. A true return means queued, not durably stored. On false, the caller must retry or report loss. The concurrency test launches two independent producers, deliberately fills the queue, retries rejected admissions and checks that all 200 records reach a non-overlapping storage callback. It also injects a callback failure and checks the diagnostic counter. Building-wide occupancy and schedule ownership remain with Leon; this module does not create competing versions of that shared state.

## Manual demonstration checklist

These steps require a running Wokwi or physical setup. Automated Windows tests exercise the adapter with simulated GPIO/queues; they do not replace these hardware checks.

1. Build demo, open Wokwi, keep switch right. Wait for both `valid=1 online=1` status rows.
2. Click each PIR and **Simulate Motion**. Each corresponding LED turns on; simultaneous triggers leave both lights on. Release motion and observe `motion=0 light=1`, followed by light off after five seconds from the final high input sample.
3. Retrigger before timeout and confirm the light remains on until a full delay after the renewed motion ends.
4. Set switch left for over six seconds. Coordinator eventually reports `online=0`; retries/timeouts rise. Trigger a PIR while disconnected: its local LED still responds.
5. Set switch right. Both coordinator snapshots recover and report the current state. Counters expose any retry/duplicate behavior; do not expect every intermediate disconnected transition in storage.
6. On physical boards, disconnect each UART wire independently and repeat. In particular disconnect only coordinator TX (ACK direction), then change motion: fresh state must still arrive after retry timeout. Restart a room with NVS intact; its higher session must be accepted. Restart coordinator; the next heartbeat must restore both snapshots.
7. Connect Edward's SD hook and repeat with a missing/full card. Confirm `storage_unwritten` increases and local lighting continues. Run concurrent entrance events and room motion. Verify actual persisted contents before marking SD integration complete.

## Trevor's report contribution draft

The lecture-room subsystem uses two PIR inputs and independent light outputs. FreeRTOS room tasks implement immediate switch-on with a configurable switch-off delay, preventing rapid light changes between motion detections. Local room control continues if the coordinator link is unavailable.

The communication layer defines a versioned binary frame with CRC checking, per-boot session identifiers and sequence numbers. Acknowledgements and bounded retries recover temporary loss. Duplicate and stale messages do not overwrite newer receiver state. Periodic full snapshots restore coordinator knowledge after reconnection, while an offline timeout distinguishes live readings from last known values.

Synchronization uses task ownership, queues and a mutex. A serialized storage consumer provides an integration point for the team's SD-card module. Queue pressure and failed persistence are counted explicitly. Automated C tests cover timing/protocol edge cases, and threaded adapter tests exercise simultaneous rooms, outage/reconnection and concurrent storage producers. Physical UART and Wokwi demonstrations must be reported separately from these automated tests.

This draft describes the supplied implementation. Trevor should review the code, perform the manual demonstrations, adapt the wording to his own explanation and record only evidence actually observed. Innovation #14 still requires team agreement; no extra feature is represented as approved.

## Individual video outline

1. State Trevor's assigned scope and point to `core.c`, `runtime.c` and the two public headers.
2. Explain `tr_room_sample`: high samples refresh the deadline; low samples retain the light until timeout.
3. Show the frame layout, CRC, matching ACK, sequence/session checks and bounded retry behavior.
4. Explain room/coordinator/storage task ownership, snapshot mutex and queue overflow counters.
5. Demonstrate both rooms, simultaneous motion, delayed off, an outage and restored state. Clearly identify logical simulation versus physical hardware.
6. Show the automated test results and accurately identify outstanding team integration, physical checks and innovation approval.
