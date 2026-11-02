# Leon Nsitta Kalungi — Presentation Guide

## What I worked on

I implemented the entrance access-control module and connected it to the rest
of the one-board demonstration. My part covers the two RFID readers, access
rules, building inside list, access schedule, clock handling, LCD, keypad,
dashboard, and event handoff.

## Suggested explanation

> “My part is the entrance controller. The two RFID readers share the SPI bus
> but have separate chip-select pins, so the ESP32 can tell whether a card was
> scanned at entry or exit. The reader driver returns a UID. The entrance rules
> check whether the UID belongs to an enabled person, whether that person is
> already inside, whether the clock is ready, and whether the configured
> Kampala access window is open. The inside count changes only when the scan
> is granted.”

> “The schedule applies to both doors. The LCD shows current status and the
> keypad provides a local administrator screen. The Wi-Fi page and serial menu
> use the same schedule API, so there is one schedule used by both readers.
> After an access decision, the module reports it to the display and logs, calls
> Grace's feedback LED, and passes the event to Trevor's queue for storage.”

## Code to show

1. `components/entrance/core.c` — default schedule, schedule validation,
   open/closed calculation, UID lookup, anti-passback, and inside-list updates.
2. `components/entrance/mfrc522.c` — reading the RFID UID over SPI.
3. `components/entrance/runtime.c` — connects the reader, clock, rules, display,
   event logging, schedule, and FreeRTOS tasks.
4. `components/entrance/keypad.c` — keypad states and administrator schedule
   flow. The new schedule is typed as four digits `HHMM` and confirmed with `#`.
5. `include/entrance/runtime.h` — public APIs used by storage, CLI, and the
   system integration code.
6. `src/main.c` — the occupancy bridge that passes the entrance count to room
   control every 250 ms.

## Suggested live demonstration

1. Show the LCD's current time, schedule, and inside count.
2. Scan a registered card at entry and show the count increase.
3. Scan the same card at entry again and explain the anti-passback denial.
4. Scan the card at exit and show the count decrease.
5. If time allows, use `A`, PIN `2468`, `#`, then `2` to set the closing time.
   Type four digits such as `1800`, press `#`, and explain the saved schedule.

If the current time is outside the configured window, a valid card should be
denied. That is expected schedule behavior; do not describe it as a reader
failure.

## Concepts to explain simply

- **UID:** the identifier bytes returned by an RFID card.
- **Anti-passback:** a person already on the inside list cannot enter again;
  someone not on the list cannot exit.
- **Fail closed:** if the system cannot confirm the time, it denies access.
- **Schedule API:** one shared function updates the hours used by both doors.
- **Occupancy uncertainty:** because the inside list is in RAM, after reboot
  staff check the building before confirming an empty state.

## Limits to state accurately

The one-board Wokwi design represents two rooms and the coordinator as logical
modules. The tailgate alert on keypad `B` is simulated because the circuit has no
passage sensor. The inside list is not restored after reboot. The LEDs are
indicators and do not unlock a real door.
