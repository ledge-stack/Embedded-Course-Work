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
5. Present a known card outside the access schedule to demonstrate schedule
   denial. A known person already inside can exit after closing. At close,
   serial logs list any remaining occupants.

## Change the schedule at the terminal

1. Press `A`, enter demo PIN `2468`, then press `#`.
2. Press `1` to set opening time or `2` to set closing time.
3. Enter four digits in `HHMM` format and press `#`. `*` clears the entry;
   `D` cancels or logs out. The schedule is saved in ESP32 NVS.

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

## Scope shown in the simulation

The diagram intentionally contains one ESP32. Room 1, Room 2, entrance and exit
run as logical modules on the same chip. RFID readers and microSD share SPI;
LCD uses I2C; the entrance keypad uses a 4x4 matrix. The entrance terminal,
occupancy, schedule, dashboard, shared-type, and diagram work in this change is
Leon Nsitta's. Room-control and logical-link behavior remains Trevor's. The
current firmware does not mount or write the microSD card. Demo RFID values and
PIN are presentation credentials; the schedule defaults to 08:00-18:00 until
changed.
