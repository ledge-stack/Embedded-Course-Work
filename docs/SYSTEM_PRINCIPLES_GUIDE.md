# System Principles Guide

This guide explains the embedded-system ideas used by the CoCIS project in
plain terms and points to where each idea appears in the source.

## 1. Modules and public interfaces

A module is a part of the program responsible for one job. The entrance module
owns access decisions; the room module owns room state; storage owns files.
Keeping these jobs separate makes it easier to find and explain behavior.

The `.c` file implements a module. Its `.h` file lists the public functions and
types other modules may use. The public header is like a counter at an office:
other modules can make an approved request without reaching behind the counter
and changing private state. See `components/` and `include/`.

## 2. FreeRTOS tasks

A task is an independently scheduled function that keeps running. It lets the
firmware do several kinds of work without one long loop handling everything.
For example, one task scans the keypad while another checks RFID cards and
others sample room motion or write events.

Tasks yield time with `vTaskDelay()` or wait for a queue. That gives other work
a chance to run. A task is not the same as a second processor: FreeRTOS
schedules the work on the ESP32's available cores.

## 3. Queues

A queue is a first-in, first-out mailbox for fixed-size items. One task can put
an event or message in the queue; another task can remove it later. Trevor's
runtime uses bounded queues for simulated room links and a 16-item event queue
for the single storage worker.

The queue separates making a decision from the slower act of writing the SD
card. Queue capacity is limited so memory use is predictable. A non-blocking
send fails immediately when the queue is full, and the firmware counts that
drop. A queued event is not proof that the SD card successfully saved it.

For access and schedule changes, the caller also waits for a **task
notification** from the storage worker. This is a small signal sent directly
to the waiting task with the write result. The wait is limited to five seconds;
a timeout or failed write is reported as not confirmed. Room-state records stay
asynchronous so room-control tasks do not wait for storage.

## 4. Mutexes and semaphores

A mutex is a lock with an owner. A task takes the lock before using shared data
and gives it back after finishing. This prevents two tasks from changing the
same state at once. For example, the entrance mutex protects the inside list
and schedule, while the storage file mutex protects SD operations.

In FreeRTOS, a mutex is implemented through the semaphore API, which is why the
code calls `xSemaphoreTake()` and `xSemaphoreGive()`. This project uses mutexes
for mutual exclusion; it does not currently use separate binary or counting
semaphores as event signals. The lock should be held only while reading or
updating the shared value. The entrance runtime releases its state lock before
logging or sending a storage event, so slow work does not hold up state access.

## 5. Shared state and one owner

Each important live value has one module that owns updates to it:

- Leon's entrance runtime owns the building inside list and access schedule.
- Trevor's room runtime owns room motion, light, link, and usage state.
- Edward's storage module owns SD files and saved roster copies.
- Grace's actuator module owns the status LED output.

Other modules use a public function to request a change or get a snapshot. This
avoids separate copies of the same schedule or occupancy count drifting apart.

## 6. SPI, I2C, and UART

These are ways for the ESP32 to communicate with devices:

- **SPI** uses clock and data wires plus a chip-select line for each device.
  The two MFRC522 readers and microSD card share SPI clock/data but have
  separate chip-selects. Only the selected device should respond.
- **I2C** uses shared SDA and SCL lines. The LCD uses this bus and its address
  identifies it on that bus.
- **UART** sends serial bits between devices or to a computer. UART0 connects
  the USB serial console to the CLI and ESP-IDF logs. Separate Trevor roles use
  UART for room links; the combined simulation uses queues instead.

The interfaces are different, so pin assignments and device drivers must match
the selected bus. See `docs/INTERFACES.md` for the project's wiring map.

## 7. Persistent memory: NVS and SD

RAM is fast working memory. It is cleared when the board loses power or resets.
The current inside list is in RAM, so it cannot by itself tell the system who
remained in the building after a reboot.

NVS is ESP32 non-volatile storage for small settings. This project stores the
access schedule and authoritative user roster there. The SD card holds
the larger event log and a saved roster file. Both storage paths can fail; the
firmware reports availability and keeps local access/light decisions running
when the SD card is unavailable.

## 8. Authentication and lockout

The demo PIN is not stored as a readable string. The auth module derives a
value using PBKDF2-HMAC-SHA-256 and compares it to a stored verifier. The
comparison checks all bytes. A mutex protects failure count and lockout state.
Five failures start a one-minute lockout. Current cost is 1,000 rounds to
reduce the keypad wait in Wokwi. This four-digit presentation PIN remains a
demonstration feature, not a production identity system.

The PIN verifier is a type of password-based key derivation. Its repeated work
makes each guess more expensive, but also makes a correct login take longer.
Reducing rounds improves response time while lowering the cost of guessing a
PIN if the stored verifier is copied.

## 9. Time and fail-closed behavior

The schedule uses Kampala local time. SNTP synchronizes the clock over Wi-Fi.
Before time is valid, the system denies entry and exit rather than guessing.
When time is valid, the entrance module checks the schedule at both readers.
This is called failing closed: when a required safety fact is unavailable, the
system chooses the restricted action.

The schedule uses minutes after midnight. A normal interval such as 08:00 to
18:00 allows access from the start up to (but not including) the end. An
overnight schedule such as 22:00 to 06:00 is supported. Equal start and end
times are rejected.

## 10. Room protocol: sessions, sequence, checksums, and retries

A room message contains a room ID, a boot session number, a sequence number,
motion/light state, and the off-delay. A checksum detects a corrupted frame.
The receiver checks that a message is new; duplicate or old messages do not
replace newer room state. Acknowledgements tell the sender that a frame was
received. If an acknowledgement does not arrive, the sender retries and later
counts a timeout.

The boot session distinguishes a new run from messages left over from an older
run. A heartbeat republishes state even when it has not changed, allowing the
coordinator to notice that a room is still online. The simulated link switch
can interrupt the logical queue transport; separate builds use UART wiring.

## 11. Logging and diagnostics

ESP-IDF log calls and serial CLI output go to UART0. Logs explain decisions and
errors; counters summarize repeated events such as retries, stale messages,
queue drops, and failed writes. The `status` command presents a snapshot, while
`records` reads saved SD entries. The room `usage` command reports accumulated
durations since boot and does not claim to measure electrical energy.

Diagnostics help distinguish different problems. For example, a room may be
locally controlling its light while its coordinator status becomes stale; an
SD write counter may increase while RFID decisions continue. These are
different paths with different owners.

## 12. Safety boundaries and simulation limits

- The RFID inside count is a logical access count, not a people-counting
  sensor. Tailgating can make it inaccurate; keypad `B` marks a simulated
  uncertainty event.
- The PIR signal reports motion, not how many people are in a lecture room.
- The LEDs represent feedback and lighting states. They do not drive door
  locks or mains fixtures.
- The supplied Wokwi diagram uses one ESP32 and logical links. It does not
  validate physical multi-board wiring or electrical behavior.
- A stored event or accepted command should be distinguished from a confirmed
  physical outcome. The simulation only demonstrates the modeled parts.
