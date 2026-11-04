# Code and System Walkthrough

This guide follows one request through the firmware: startup, an RFID scan,
room control, administration, and saving records. It assumes no prior knowledge
of the codebase and explains what each layer contributes.

## 1. How to read a C module

A `.c` file contains the implementation: the actual function bodies and the
module's private variables. A `.h` file contains the public interface: the
function names and data shapes other modules are allowed to use. For example,
the entrance module's implementation lives in `components/entrance/`, while
other modules use declarations in `include/entrance/`.

An API call is a request from one module to another. The caller uses the
function declared in a header; the implementation decides how the request is
performed. This keeps each module's private state in one place.

## 2. Begin at `app_main()`

Open `src/main.c`. ESP-IDF calls its `app_main()` function after the chip boots.
In the combined `TR_ROLE=0` build the startup order is:

1. Initialize NVS, the ESP32's non-volatile settings store.
2. Start Edward's storage module. It initializes the shared SPI bus and tries
   to mount the SD card.
3. Start Trevor's room runtime. It creates room, coordinator, event-storage,
   and status tasks.
4. Start Grace's feedback LED.
5. Start Leon's entrance runtime and its RFID, clock, display, schedule, and
   keypad tasks. The Wi-Fi dashboard is also started when network setup
   succeeds.
6. Load an SD/NVS saved user roster if available.
7. Create the occupancy bridge task to pass entrance occupancy into room
   control every 250 ms.
8. Start Edward's serial command task on UART0.

The startup function connects the modules. It does not contain the detailed
RFID rules, SD file format, or room-light algorithm; those live with their
respective modules.

## 3. What happens when a person scans a card

Follow this path:

```text
RFID card
  -> MFRC522 reader driver
  -> entrance polling task
  -> access rules and user lookup
  -> occupancy update if granted
  -> display, serial log, feedback LED, and storage queue
```

### Step 1: the reader supplies a UID

`components/entrance/mfrc522.c` communicates with each MFRC522 reader over
SPI. It returns the card's UID (identifier bytes) and the reader number. The
entry and exit readers have distinct chip-select pins, so the same SPI bus can
talk to either reader.

### Step 2: the entrance runtime asks the rules

`components/entrance/runtime.c` polls for cards and determines whether a scan
came from the entry or exit side. It looks up the UID in the registered roster
and obtains the current clock and schedule state.

`components/entrance/core.c` contains the small, testable decision rules. It
rejects unknown or disabled cards, a second entry by someone already inside,
an exit by someone not recorded inside, a missing clock, and a closed schedule.
The schedule check applies to both doors. The inside list is changed only when
the result is `GRANTED`.

### Step 3: the result is shown and saved

The entrance runtime updates its state, logs the decision, updates the LCD,
signals Grace's LED, and hands the access event to Trevor's event queue. The
entrance waits up to five seconds for the storage worker's result. Success
means the CSV line was written, flushed, synced, and closed. A successful scan
changes the building count; a denial does not. If the queue is full or the SD
write fails, the firmware reports that the access record was not confirmed.
Local access logic continues running.

## 4. How the schedule and keypad work

`components/entrance/keypad.c` scans the 4x4 keypad and uses a small state
machine. A state machine means the same key can do different things depending
on the current screen, such as public screen, PIN entry, admin menu, or time
entry.

- In public mode, `A` opens PIN entry, `B` marks a simulated tailgate alert,
  and `C` prints a read-only roll-call to the serial console.
- Enter `2468` and press `#` to log in. In the admin menu, `1` sets the opening
  time, `2` sets the closing time, `3` pages through registered people and
  their current inside state, `4` pages through the latest access attempts, and
  `5` confirms the building is empty after a physical check.
- Enter the new time as four digits in `HHMM` form and press `#`; for example,
  `0730` means 7:30. `*` clears input and `D` cancels or logs out.
- The schedule API validates the value, saves accepted hours to NVS, updates
  the schedule shared by both RFID readers, and writes a change row to the SD
  audit log. If the SD audit write fails, the NVS schedule is still saved and
  the system reports the missing audit row.

`components/auth/runtime.c` checks the PIN against a stored PBKDF2 verifier.
It does not store the four-digit PIN as a plain string. Five failed attempts
cause a one-minute lockout. The verifier currently uses 1,000 PBKDF2 rounds to
reduce wait time in the simulator. This is a demo security control, not a
production credential system.

The Wi-Fi page in `components/entrance/dashboard.c` uses the same shared PIN
and schedule API. After sign-in it shows the registered people and their
current state, together with recent access activity. For browser access to a
Wokwi ESP32 web server, the Wokwi Private IoT Gateway is required.

With the default roster, Blue belongs to Leon Nsitta Kalungi, Green to Kato
Trevor, Yellow to Kagimu Edward, and Red to Ahurira Grace. The LCD uses the
short call names Leon, Trevor, Edward, and Grace for welcome and goodbye
messages. The dashboard and keypad show up to eight latest scans from the
current boot; the full persistent event log is viewed with the serial `records`
command.

## 5. How each room controls its light

Trevor's room task in `components/trevor/runtime.c` reads one PIR input and
controls one light output. The portable room rule in `components/trevor/core.c`
turns a light on when motion is detected and starts an off-delay after motion
stops. If motion appears again during the delay, the delay starts over.

The room task also calculates the effective light request. The occupancy bridge
passes the entrance's count and a flag that says whether the count is
authoritative. If the count is known to be zero, a requested light is forced
off. If the count is uncertain, the code avoids treating the building as
empty.

Each room publishes its motion, light, delay, sequence, and session information.
In the one-board demo, FreeRTOS queues emulate the room/coordinator links. In
separate firmware roles the room protocol uses UART. A room that stops
responding becomes stale after the timeout, and the sender tracks retries and
timeouts.

## 6. How event logging reaches the SD card

Room and access code do not open the event file directly. They place a complete
event item in Trevor's bounded storage queue. One `storage_task` removes events
and calls the storage API. Edward's `components/storage/runtime.c` serializes
file access and appends records to `/sdcard/events.csv`. Room updates remain
asynchronous; access and schedule-change requests wait up to five seconds for
the writer's result.

This design means multiple tasks do not write to the CSV at the same time. The
queue has a fixed capacity. If it fills, the system increments a drop counter
and keeps running. Access rows include a readable Kampala timestamp, person,
entry/exit direction, decision, UID, and resulting inside count. Schedule rows
show the old and new opening/closing times. The active schedule is committed to
NVS before the audit row is written, so a failed SD audit does not undo the new
schedule.

For example, a new entry row is shaped like:

```text
ACCESS,event_id,Kampala_time,"person",ENTRY,GRANTED,UID,user_id,inside_count,utc_epoch,uptime_ms
```

A schedule row shows the previous and new hours, such as
`SCHEDULE,time,OPEN=08:00->08:00,CLOSE=18:00->23:00`. The firmware appends new
rows; it does not erase earlier records. Rows saved by older firmware may still
use the previous compact hexadecimal format. If the 23:00 setting was saved
before this audit-row update, it remains in NVS, but its earlier change is not
added retroactively to the SD history. Saving the schedule again creates a new
audit row.

The roster is different from the event log. It is stored as a binary roster on
the SD card and backed up in NVS. The live list of people currently inside is
held in RAM and is not restored after reboot. The system therefore marks
occupancy uncertain after restart until staff check the building.

## 7. How the serial menu works

`components/cli/runtime.c` reads text lines from UART0 at 115200 baud. It
separates the first word as a command and then calls the module APIs to answer.
For example, `rooms` reads Trevor's room snapshot and `records` asks storage to
print the SD log. The read-only commands include `help`, `status`, `inside`,
`rooms`, `usage`, `users`, `records`, and `schedule`.

`login 2468` creates a 60-second admin session. While it is active, `open
HH:MM` and `close HH:MM` change the schedule; `useradd NAME UIDHEX` and
`userdel ID` edit the roster; `confirm-empty` sets occupancy to known-empty
after the building has been physically checked. The CLI does not edit another
module's private state directly.

## 8. Follow a request to its source file

| Question | Start here | Then follow |
| --- | --- | --- |
| Which cards are accepted? | `components/entrance/core.c` | `components/entrance/runtime.c` |
| What is shown on the LCD? | `components/entrance/display.c` | `components/entrance/runtime.c` |
| How does the keypad change hours? | `components/entrance/keypad.c` | `components/entrance/runtime.c` schedule API |
| How are rooms and lights controlled? | `components/trevor/core.c` | `components/trevor/runtime.c` |
| How are room messages retried? | `components/trevor/core.c` | `components/trevor/runtime.c` |
| Where does the SD record go? | `components/trevor/runtime.c` storage task | `components/storage/runtime.c` |
| How are serial commands handled? | `components/cli/runtime.c` | the API in the relevant `include/` folder |
| Where do the pieces start together? | `src/main.c` | the public headers used there |

For a map of every folder, see [`REPOSITORY_STRUCTURE_GUIDE.md`](REPOSITORY_STRUCTURE_GUIDE.md).
