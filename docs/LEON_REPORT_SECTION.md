# Leon Nsitta: Entrance terminal and access schedule

**Draft date:** 1 November 2026

## What this part does

The entrance terminal uses one RFID reader for entry and one for exit. The
entrance runtime checks each card against its registered-user list, checks the
schedule for entry, and updates one inside list. A person who is already inside
cannot enter again. A person who is not inside cannot exit. A known person can
still leave after closing time.

The schedule uses Kampala time from SNTP. Entry is denied when the clock is
unavailable, because the system cannot confirm that entry is allowed. The
schedule can be changed through the keypad or the Wi-Fi dashboard and is saved
in NVS. Both interfaces call the same schedule API.

The LCD shows whether the entrance is open, the hours, the last card result,
and the number of people recorded inside. It also shows an after-hours alert
when the schedule is closed and people remain inside. The serial report lists
their registered names and says whether occupancy is confirmed or uncertain.

## Safety feature

The entrance rejects repeat entry and exit-without-entry. These decisions are
labelled in the serial log. Pressing keypad `B` injects a clearly labelled,
simulation-only tailgate event. It marks occupancy uncertain; it does not
invent a person or change the inside list. Pressing `C` prints a read-only
evacuation report. At closing time, the runtime prints the same report as an
after-hours alert if anyone is still recorded inside.

This report describes RFID-tracked users. A simulated tailgate can mean the
list is incomplete, so staff must check the doorway before treating the list
as a complete headcount. The report does not clear the inside list.

## Integration and limits

Trevor's room lights and communications remain in `components/trevor/`. The
one-board `esp32dev` build runs both Trevor's logical room demo and Leon's
entrance terminal. It does not represent three physical ESP32 boards.

The demo roster currently contains two sample users. Edward's persistent user
store is not in this checkout; it should load a fixed 32-entry roster and pass
it to `entrance_runtime_set_users()`. The API keeps stable user IDs and refuses
to replace a person who is recorded inside.

Grace's actuator and buzzer module is not in this checkout. The LCD and serial
log report access outcomes, but this draft does not claim that a door moves or
that a buzzer sounds. The project is not published, so there is no verified
simulator URL. A Wokwi CLI reader probe detected the registered green card at
both readers. The entry-flow run happened after the configured 08:00-18:00
Kampala window, so the card was correctly denied. A granted
entry-and-exit run is still needed.

## Verification to attach before submission

- **Build:** all four PlatformIO environments (`esp32dev`, `coordinator`,
  `room1`, and `room2`) passed on 31 October 2026 after the PIN-verifier fix.
  The three separate role builds show a 2 MB versus 4 MB flash-size warning.
- **Wokwi reader probe:** passed for both readers; green UID `11:22:33:44` was
  detected. This does not demonstrate a granted access event.
- **Wokwi access flow:** pending a granted entry and exit during open hours.
  Add the public project URL only after the shared project is published.
- **PIN authentication:** the PBKDF2 input-order correction builds, but its
  successful `2468` keypad login has not yet been checked in Wokwi.
- **Host test:** the entrance-core test passed on 31 October for entry,
  anti-passback, exit after closing, invalid exit, closed hours, and unavailable
  clock. It does not test Wokwi hardware behavior or the roll-call task.

**Wokwi project URL:** pending team publication and verification.

## Contribution statement

Leon implemented and integrated the entrance RFID decisions, occupancy and
anti-passback rules, Kampala schedule, keypad and LCD interface, Wi-Fi schedule
dashboard, roll-call and after-hours report, and the registered-user API
handoff. Trevor's room runtime and Edward's storage and user-store modules are
separate contributions. Grace owns the door actuator, indicators and buzzer.
