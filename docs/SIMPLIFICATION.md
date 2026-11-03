# Simplification record — 3 November 2026

Changes were saved directly in `Embedded-Course-Work`. The keypad, web dashboard,
LCD, PIN verifier, RFID driver, access rules and earlier bug fixes are retained.

## Verified backup

Before edits, the complete working source/documents were saved to:

`C:\Users\Trevor\Documents\Codex\2026-10-31\th\outputs\Embedded-Course-Work-before-simplification-2026-11-03.zip`

The ZIP was read back, CRC-checked and compared byte-for-byte with all 112
source/document files. It includes the earlier uncommitted fixes. Generated
`.pio`/`.qa` files and `.git` were excluded; Git history remains in this repository.

## What became simpler

- One `esp32dev` environment replaces four role-specific firmware environments.
- Two room tasks update local snapshots directly. The simulated wire frames,
  CRC, parser, ACKs, retries, boot-session state, heartbeat, coordinator task,
  link switch and background protocol diagnostics are removed.
- One typed event queue feeds the existing single SD writer. Room lighting and
  snapshots continue under queue pressure or failed storage. Access/schedule
  requests retain their bounded, request-ID-specific completion notifications.
- Existing team component boundaries stay, so the retained keypad/dashboard
  still share the entrance schedule API and authentication.
- Separate entry GPIO32 and exit GPIO12 LEDs simulate the appropriate door.
  Only a grant opens the relevant output. An independent task closes each
  about three seconds later; the bridge closes both outside access hours.
  The GPIO12 pulldown and all keypad/LCD/reader/room wiring stay.
- Current documentation and speaking guides describe this local event flow.
  Member identification details were preserved. Old documentation/source is
  available in the backup; historical verification is marked as such.

The keypad, dashboard, LCD-driver, PIN-verifier and entrance-policy source files
were checked byte-for-byte against the backup and are unchanged. Their startup
paths remain connected. The diagram retains keypad, LCD, SD and both readers;
only the switch/door-output area and explanatory labels changed.

## Actual line counts

Counts include comments and blank lines in application `.c` files, excluding
headers, tests and generated/SDK code. Both revisions use the same counting rule.

| Code | Before | After | Removed |
| --- | ---: | ---: | ---: |
| All 13 application C files | 3,142 | 2,733 | 409 (13.0%) |
| Trevor's two C files | 671 | 240 | 431 (64.2%) |
| Trevor core.c | 174 | 36 | 138 |
| Trevor runtime.c | 497 | 204 | 293 |

The new independent door implementation adds code, so total reduction is smaller
than the room-module reduction. Retaining the keypad and web dashboard also
retains their user-interface code; this is not the earlier 328-line project.

## Fresh proof

- Direct `pio run -e esp32dev`: PASS, exit code 0, about 4 minutes 24 seconds.
  PlatformIO Espressif32 7.0.1, ESP-IDF 6.0.1, Xtensa GCC 15.2.0.
- Reported static RAM 39,376 / 327,680 bytes (12.0%); application flash
  969,565 / 1,048,576 bytes (92.5%). Runtime heap/task stacks are additional.
  Existing SDK Kconfig notes did not prevent compilation or linking.
- `scripts/test-all.ps1`: PASS. Room timing/usage, threaded runtime, entrance
  policy, independent actuator suite, plus all 13 management/storage regressions.
- Runtime coverage now includes direct snapshots without the old GPIO32 link,
  simultaneous rooms, delayed off, confirmed-empty suppression and usage,
  concurrent producers, queue pressure and SD failure, actual host task
  notifications, and ignoring a timed-out request's late completion.
- Door tests cover closed startup, denial, separate entry/exit outputs,
  independent three-second deadlines without a reader update call, and close-all.
- New snapshot/door-startup expectations failed against the old implementation
  before the new code passed them. Existing removed protocol/outage tests are
  preserved in the archive rather than claimed as current coverage.
- Circuit JSON parsed with 25 parts and 64 connections, valid component-ID
  references and retained keypad/LCD/reader/SD parts. This is structural proof,
  not a live electrical simulation.
- Independent read-only source review: no blocking findings. Retained UI source
  and startup, local state/queue behavior, door timing and pins were inspected.

The tracked SDK configuration was restored exactly to its pre-build bytes;
toolchain normalization was not retained as an unrelated source change.
No commit, push or public publication was performed.

## Live limits

The subsequent RFID fix was tested live in Wokwi on 3 November: clock
synchronization, granted entry, duplicate-entry denial and granted exit all
passed. See RFID_VERIFICATION.md. This is partial end-to-end evidence. Perform
PRESENTATION_DEMO.md before claiming the full simulation works. In particular,
verify the retained keypad and browser
forms, all three management interfaces, both door LEDs, real SD rows and roster/
schedule recovery. Host substitutes do not prove physical card-full/removal or
power-loss behavior. Prior Wokwi results apply to prior firmware.

The inside list remains RAM-only and uncertain after reset. Usage is duration,
not kWh. Door LEDs are simulated actuators, not locks. The B-key tailgate alert
is simulated. There is no longer a three-board UART or link-loss demonstration.

## What Trevor should explain

Begin with the 36-line `components/trevor/core.c`, then the 204-line runtime:
room task, mutex snapshot, event queue, one SD writer and completion notification.
Show the bridge in `src/main.c`. Explain why room motion differs from the
building's RFID count and why a known-zero count suppresses lights. Present
only the contributions and live results you actually understand and performed.
