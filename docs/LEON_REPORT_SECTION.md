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
schedule can be changed through the keypad, Wi-Fi dashboard, or authenticated
serial command and is saved in NVS. All three interfaces call the same schedule
API.

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

Trevor's room lights and communications are in `components/trevor/`. The
one-board `esp32dev` build runs Trevor's logical room demo alongside Leon's
entrance terminal. It does not represent three physical ESP32 boards. The room
runtime receives Leon's count and forces room lights off only when the count is
known to be zero. After reboot or an uncertainty alert, an administrator must
physically check the building and use `confirm-empty` before the system trusts
a zero count.

The demo roster starts with two sample users. Edward's storage module saves
roster changes to the SD card with an NVS backup and routes them through
`entrance_runtime_set_users()`. The API keeps stable user IDs and refuses to
replace a person recorded inside.

Grace's module drives a blue simulation feedback LED with a short grant pulse
and a longer denial pulse. It does not move a door or sound a buzzer. The
project is not published, so there is no shared simulator URL. The lecturer
accepts a Google Drive or GitHub link for submission; a Wokwi URL is optional.
An earlier Wokwi run detected the green card at both readers and denied access after
closing. A later Wokwi RFID scenario mounted the SD card, granted the demo
card's entry and exit during open hours, returned occupancy to zero, and wrote
both access events to `sdcard/events.csv`. CLI roster/schedule changes, room
shutdown at a confirmed zero count, and the LED pulse still need separate
interactive checks.

## Verification to attach before submission

- **Build:** all four PlatformIO environments passed on 1 November 2026 after
  the storage, CLI, feedback and occupancy integration. The role configs now
  match the board's 4 MB flash size.
- **Wokwi reader probe:** passed for both readers; green UID `11:22:33:44` was
  detected. This does not demonstrate a granted access event.
- **Wokwi access flow:** entry, exit, occupancy reset, SD mount and two stored
  access records passed in the CLI scenario. Add a shared project URL only
  after the project is published.
- **PIN authentication:** the PBKDF2 verifier and serial CLI compile; successful
  keypad, dashboard and serial login still need interactive Wokwi checks.
- **Host tests:** entrance and Trevor portable core tests passed on 1 November.
  They do not test the keypad, CLI, LED, room shutdown or all card failure cases.

**Submission link:** add the team's Google Drive or GitHub link.

**Optional Wokwi URL:** add only if the team publishes and verifies the project.

## Contribution statement

Leon implemented and integrated the entrance RFID decisions, occupancy and
anti-passback rules, Kampala schedule, keypad and LCD interface, Wi-Fi schedule
dashboard, roll-call and after-hours report, and the registered-user API.
Trevor owns room control and communication. Edward owns storage, roster and
serial administration. Grace owns the simulation feedback LED. The modules
remain in their owner-labelled folders and are composed by `src/main.c`.
