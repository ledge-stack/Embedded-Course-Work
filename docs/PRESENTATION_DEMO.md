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

The live inside list starts empty, so the display begins with confirmed zero
occupancy. If a simulated tailgate alert is triggered, use the UART0 CLI command
`login 2468` followed by `confirm-empty` before demonstrating the
room-lights-off-at-zero rule.

## Demonstrate access and occupancy

1. In the entry reader panel, choose Blue Card (UID `DE:AD:BE:EF`) for Leon
   Nsitta Kalungi and tap it. If Kampala time is inside the configured window,
   the LCD says `Welcome Leon` and shows the time and inside count; serial
   reports `GRANTED`.
2. Present the same card at entry again. The LCD shows `DENIED` and count stays
   at 1 because anti-passback blocks a second entry.
3. Present the card at the exit reader. The LCD says `Goodbye Leon` and the
   count returns to 0. Present it at exit again to demonstrate exit-without-entry
   denial.
4. The remaining member cards are Green (`11:22:33:44`), Kato Trevor; Yellow
   (`55:66:77:88`), Kagimu Edward; and Red (`AA:BB:CC:DD`), Ahurira Grace.
   Their LCD call names are Trevor, Edward, and Grace. Use the Key Fob preset
   (`C0:FF:EE:99`) or NFC Tag to demonstrate an unknown-card denial.
5. Present a known card outside the access schedule at both readers to show
   that entry and exit are denied. A denied exit leaves occupancy unchanged.
   At close, serial logs list any remaining occupants.

The scenario `scenarios/rfid-entry-exit.yaml` passed in Wokwi: the SD card
mounted, the green demo card entered and exited during open hours, occupancy
returned to zero, and both access events were written to `sdcard/events.csv`.
Current firmware writes readable access rows with a Kampala timestamp and
waits for the storage worker to flush and sync the row. Use the serial `records`
command to show the event history. After changing the schedule, `records` also
shows a `SCHEDULE` row with the old and new hours. Reboot persistence is
implemented through the SD card and NVS but still needs a fresh live
power-cycle demonstration.
To check it, set the closing time to `23:00`, wait for `SCHEDULE SAVED`, and
run `records` on UART0. Reset the ESP32 without replacing or formatting the SD
card, wait for SNTP, then run `schedule` and `records` again. The schedule
should still end at `23:00`, and the access/schedule rows should remain. This
live power-cycle check is not yet recorded as passed.
The after-hours scenario `scenarios/rfid-after-hours.yaml` also passed in Wokwi.
It read the registered card UID `11223344` and denied entry because the
08:00-18:00 schedule was closed, then read the unregistered Key Fob UID
`C0FFEE99` and denied it. Run `rfid-entry-exit.yaml` during open hours to
demonstrate a grant.
The expanded `scenarios/presentation-smoke.yaml` has not yet passed its full
set of automated assertions.

Anti-passback denial is part of the current demo. On the public keypad screen,
press `B` to mark a **simulated tailgate** and press `C` to print a read-only
evacuation roll-call to the USB serial monitor. The tailgate marker makes the
LCD show that occupancy is uncertain; it does not add a person to the inside
list. At closing time, the firmware prints an after-hours roll-call if anyone
is still recorded inside. Grace's blue feedback LED pulses after scans; it does
not unlock a door. Feature limits are in
[`SYSTEM_OPERATION_REPORT.md`](SYSTEM_OPERATION_REPORT.md).

## Change the schedule at the terminal

1. Press `A`, enter demo PIN `2468`, then press `#`. The LCD shows
   `CHECKING PIN` during verification, then displays the admin menu if the PIN
   is accepted.
2. Press `1` to set opening time or `2` to set closing time.
3. Enter four digits in `HHMM` format and press `#`. `*` clears the entry;
   `D` cancels or logs out. The LCD shows `CHECKING TIME` while it validates
   and saves the new time in ESP32 NVS.
4. From the admin menu, press `3` to page through all registered people and
   their current state (`IN`, `OUT`, or `OUT?` when occupancy needs checking).
   Press `A`/`B` to page forward/back and `D` to return to the menu.
5. Press `4` to page through recent access attempts, newest first. It shows a
   welcome, goodbye, or denial with the member's call name and entry/exit time.
   The keypad and dashboard show up to eight scans from the current boot; the
   full saved history is available with the serial `records` command.
6. Press `5` after physically checking that the building is empty to clear an
   occupancy-uncertain state. The keypad confirms the action only when nobody
   is recorded inside.

## Change the schedule over Wi-Fi

The dashboard serves from the ESP32 on port 80 after it joins `Wokwi-GUEST`.
Run Wokwi's Private IoT Gateway, enable it in the Wokwi editor, then browse to
`http://localhost:9080/` and sign in with demo PIN `2468`. The page shows live
access state, occupancy, the roster with each person's current state, recent
entry/exit attempts, and opening/closing times. An authorized presenter can
change the schedule there. The dashboard list is protected by sign-in. Recent
attempts shown there are from the current simulation run; older event history
remains on the SD card. The demo PIN and HTTP dashboard are for presentation
only, not production authentication.

## Trevor's existing room demo

This is Trevor's pre-existing subsystem, included in the combined firmware.
Trigger each PIR sensor and observe its matching room LED. Motion turns the
light on immediately; after motion ends, the light stays on for the configured
two-minute off-delay. Move the link switch left to interrupt Trevor's logical
room/coordinator queues, then right to restore them.

The firmware forces room lights off only when the entrance count is confirmed
as zero. The room task preserves its motion state while occupancy is uncertain.
Room shutdown, link recovery, and the visual feedback LED still need a live
presentation check.

## Edward's serial menu

At the UART0 serial console type `help`. Read commands include `status`,
`inside`, `rooms`, `usage`, `users`, `records`, and `schedule`. The `usage`
command compares room light-on time with requested light-on time suppressed
while the building is confirmed empty. Values reset at reboot and are not kWh.
`login 2468` grants a
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
