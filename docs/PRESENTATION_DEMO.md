# One-board Wokwi presentation demo

## Start

From this project directory, build the firmware:

```sh
pio run -e esp32dev
```

Open `diagram.json` in Wokwi and start the simulator. The USB serial monitor uses
115200 baud. The firmware starts the entrance LCD, keypad, Wi-Fi dashboard,
room demo and both RFID readers. It joins `Wokwi-GUEST` and synchronizes time
with SNTP; wait for the `SNTP synchronized` serial log before presenting an
entry card. The LCD reports open/closed state, access hours, the last event and
current occupancy.

After a reboot the live inside list is volatile, so the display marks occupancy
uncertain. Physically check that the building is empty, then use the UART0 CLI
command `login 2468` followed by `confirm-empty` before demonstrating the
room-lights-off-at-zero rule.

## Demonstrate access and occupancy

1. In the entry reader panel, choose Blue Card (UID `DE:AD:BE:EF`) and tap it.
   If Kampala time is inside the configured window, the LCD shows the person's
   name, `ENTER`, time and inside count; serial reports `GRANTED`.
2. Present the same card at entry again. The LCD shows `DENIED` and count stays
   at 1 because anti-passback blocks a second entry.
3. Present the card at the exit reader. The LCD reports `LEFT` and count returns
   to 0. Present it at exit again to demonstrate exit-without-entry denial.
4. Choose Green Card (UID `11:22:33:44`) for the second demo user. Choose the
   Red Card preset to demonstrate an unknown-card denial.
5. Present a known card outside the access schedule at both readers to show
   that entry and exit are denied. A denied exit leaves occupancy unchanged.
   At close, serial logs list any remaining occupants.

The scenario `scenarios/rfid-entry-exit.yaml` passed in Wokwi: the SD card
mounted, the green demo card entered and exited during open hours, occupancy
returned to zero, and both access events were written to `sdcard/events.csv`.
The expanded `scenarios/presentation-smoke.yaml` also checks repeat-entry and
unknown-card denial. Its latest CLI attempt lost the WebSocket transport before
the serial checks, so those added assertions are not yet confirmed.

Anti-passback denial is part of the current demo. On the public keypad screen,
press `B` to mark a **simulated tailgate** and press `C` to print a read-only
evacuation roll-call to the USB serial monitor. The tailgate marker makes the
LCD show that occupancy is uncertain; it does not add a person to the inside
list. At closing time, the firmware prints an after-hours roll-call if anyone
is still recorded inside. Grace's blue feedback LED pulses after scans; it does
not unlock a door. Feature limits are in
[`LEON_ENTRANCE_SAFETY_DESIGN.md`](LEON_ENTRANCE_SAFETY_DESIGN.md).

## Change the schedule at the terminal

1. Press `A`, enter demo PIN `2468`, then press `#`. The LCD shows
   `CHECKING PIN` during verification, then displays the admin menu if the PIN
   is accepted.
2. Press `1` to set opening time or `2` to set closing time.
3. Enter four digits in `HHMM` format and press `#`. `*` clears the entry;
   `D` cancels or logs out. The LCD shows `CHECKING TIME` while it validates
   and saves the new time in ESP32 NVS.

## Change the schedule over Wi-Fi

The dashboard serves from the ESP32 on port 80 after it joins `Wokwi-GUEST`.
Run Wokwi's Private IoT Gateway, enable it in the Wokwi editor, then browse to
`http://localhost:9080/` and sign in with demo PIN `2468`. The page shows live
access state, occupancy, latest event, and opening/closing times, and lets an authorized
presenter change the schedule. Browser access to the simulated server requires
the Private IoT Gateway. The demo PIN and HTTP dashboard are for presentation
only, not production authentication.

## Trevor's existing room demo

This is Trevor's pre-existing subsystem, included in the combined firmware.
Trigger each PIR sensor and observe its matching room LED. Motion turns the
light on immediately; after motion ends, the light stays on for the configured
five-second off-delay. Move the link switch left to interrupt Trevor's logical
room/coordinator queues, then right to restore them.

The firmware forces room lights off only when the entrance count is confirmed
as zero. The room task preserves its motion state while occupancy is uncertain.
Room shutdown, link recovery, and the visual feedback LED still need a live
presentation check.

## Edward's serial menu

At the UART0 serial console type `help`. Read commands include `status`,
`inside`, `rooms`, `users`, `records`, and `schedule`. `login 2468` grants a
60-second admin session for schedule changes (`open HH:MM`, `close HH:MM`),
roster changes (`useradd NAME UIDHEX`, `userdel ID`), and `confirm-empty`.
User and CLI actions are implemented but still need a live Wokwi walkthrough.

## Scope shown in the simulation

The diagram intentionally contains one ESP32. Room 1, Room 2, entrance and exit
run as logical modules on the same chip. RFID readers and microSD share SPI;
LCD uses I2C; the entrance keypad uses a 4x4 matrix. The owner-to-folder map is
in `COMPONENT_OWNERSHIP.md`: Leon owns entrance access, Trevor owns rooms and
communications, Edward owns storage and CLI, and Grace owns scan feedback.
The SD card mounts in the combined Wokwi build and stores events. Demo RFID
values and PIN are presentation credentials; the schedule defaults to
08:00-18:00 until changed.
