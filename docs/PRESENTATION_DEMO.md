# One-board Wokwi presentation demo

## Start

From this project directory, build the firmware:

```sh
pio run -e esp32dev
```

Open diagram.json in Wokwi and start the simulator. The USB serial monitor uses
115200 baud. The firmware starts the room demo and both RFID readers. It joins
Wokwi-GUEST and synchronizes time with SNTP; wait for the
"SNTP synchronized" serial log before presenting an entry card.

## Demonstrate access and occupancy

1. In the entry reader control panel, choose the Blue Card (UID DE:AD:BE:EF)
   and tap it. If Kampala time is between 08:00 and 18:00, the entry grant LED
   flashes green and the log shows GRANTED with inside=1.
2. Present the same card at entry again. It is denied as already inside and
   the entry denial LED flashes red; the count remains 1.
3. Present the Blue Card at the exit reader. The exit grant LED flashes green
   and the count returns to 0. Present it at exit again to demonstrate
   exit-without-entry denial.
4. Select the Green Card (UID 11:22:33:44) for the second registered demo user.
   Select the Red Card preset for an unknown-card denial.
5. Present a known card at entry outside the schedule to demonstrate schedule
   denial. Exits for a known person already inside remain allowed after closing.
   When the access window closes, serial logs list any remaining occupants.

## Demonstrate room control and link state

Trigger each PIR sensor and observe its matching room LED. Motion turns the
light on immediately; after motion ends, the light stays on for the configured
five-second off-delay. Move the link switch left to interrupt Trevor's logical
room/coordinator queues, then right to restore them.

## Scope shown in the simulation

This diagram intentionally contains one ESP32. Room 1, Room 2, entrance and
exit run as logical modules on the same chip. The two RFID readers share SPI
with separate chip-selects; the microSD hardware is present on the same bus.
The current firmware does not mount or write the microSD card. The schedule
setter is an API for Edward's future authenticated CLI integration; there is
no serial admin command yet. The registered UIDs and schedule are demo values,
and the schedule resets to 08:00-18:00 after reboot.
