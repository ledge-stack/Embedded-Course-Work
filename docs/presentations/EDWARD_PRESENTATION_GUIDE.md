# Kagimu Edward — Presentation Guide

## What I worked on

I implemented the SD-card storage interface and UART0 serial management menu.
My part stores room/access events, saves the registered-user roster with an
NVS backup, reports storage availability, and provides read-only status and
authenticated administration commands.

## Suggested explanation

> “My storage module initializes the shared SPI bus and attempts to mount the
> microSD card. Access and room events are appended to `events.csv`, and
> schedule changes add an audit row with the old and new hours. The event
> producers do not all write to the file themselves. Trevor's single storage
> task receives events from a bounded queue and calls my append function, which
> protects file access with a mutex. Access and schedule changes wait for the
> file to be flushed, synced, and closed before the caller reports success.”

> “The registered-user list is saved as a roster file on the SD card and backed
> up in NVS. The serial command menu runs on the USB console. It can report
> status, people inside, rooms, light-use durations, users, records, and the
> schedule. After admin login, it can change the schedule or user list. User
> changes go through the entrance module's validation API, so the CLI does not
> bypass access rules.”

## Code to show

1. `components/storage/runtime.c` — shared SPI setup, SD mount, roster
   load/save, mutex, and CSV append function.
2. `components/trevor/runtime.c` — the single queue consumer that sends events
   to the storage module.
3. `components/cli/runtime.c` — command parsing, status/read commands,
   authentication, schedule changes, and roster management.
4. `include/storage/runtime.h` and `include/cli/runtime.h` — the public service
   functions.
5. `docs/INTERFACES.md` — shared ownership and event-handoff contract.

## Useful commands to demonstrate

Read-only commands:

```text
help
status
inside
rooms
usage
users
records
schedule
```

Authenticated commands:

```text
login 2468
close 18:00
open 08:00
useradd NAME UIDHEX
userdel ID
confirm-empty
logout
```

The CLI and ESP-IDF log output share UART0, so command responses and diagnostic
messages can appear together. `records` can only show events when the SD card
is available. A queued event is not yet a durable record; storage counters
report queue drops and write failures.

## Concepts to explain simply

- **SD card:** removable persistent storage for larger event records.
- **NVS:** small ESP32 non-volatile storage used for settings and the roster
  backup.
- **Queue:** passes work to a single writer so concurrent tasks do not write a
  file at once.
- **Mutex:** a lock that protects file reads/writes from overlapping.
- **Authenticated session:** a successful PIN login opens a time-limited window
  for changes.

## Limits to state accurately

When the SD card is unavailable, local access and room logic continue, but new
events may not persist. Full-card, removal/recovery, and roster-after-reboot
behavior still need the demonstrations listed in the completion plan. The
system's live inside list is held in RAM and does not come back from the SD
roster after reboot.
