# Leon's entrance safety feature design

**Status: partly implemented.** The entrance core rejects repeat entry by a user
who is recorded inside and exit by a user who is not recorded inside. The
firmware now prints a read-only evacuation roll-call on keypad `C`, prints an
after-hours report when the schedule closes with people inside, and marks
occupancy uncertain when keypad `B` injects a simulated tailgate. Grace's
actuator feedback and buzzer are a separate integration task and are not
claimed here. This feature is for the one-ESP32 presentation simulation and
remains separate from Trevor's room-control code.

## Goals

1. Make denied access easy to notice with a short buzzer sound.
2. Alert staff when anti-passback blocks a repeat entry or an invalid exit.
3. Detect or clearly simulate a person passing without a matching access grant.
4. Print a read-only report of the registered people the entrance currently
   records as inside, without clearing or changing that state.

These are three related safety features, not a replacement for RFID, the
schedule, occupancy ownership, or production door hardware.

## Keep the design small

Add one small entrance alert module:

| Planned module | One responsibility |
| --- | --- |
| `components/entrance/actuator.c` and `include/entrance/actuator.h` | Grace owns door and buzzer outputs. Leon's runtime sends each access outcome to this module after its decision. It does not decide whether a card is valid. |
| `components/entrance/runtime.c` | Send alert requests after the existing access rules return a result. Keep the inside list and schedule as the only authoritative state. |
| `components/entrance/keypad.c` | `B` marks a Wokwi-only simulated tailgate and `C` prints a read-only evacuation report when the public screen is showing. Neither action changes a person's identity or bypasses access rules. |
| `components/entrance/runtime.c` report helper | Take one snapshot under the entrance state lock and print the report to the USB serial monitor. The schedule watcher uses the same report for an after-hours alert. |
| `components/entrance/dashboard.c` | No new dashboard action is required for the first version. The existing dashboard continues to show current status. |

Do not put alert patterns in `mfrc522.c`; that file should only read cards.
Do not add a second occupancy list or schedule for the alerts.

## Denied-access feedback

For every denied card result, log the existing reason and send the outcome to
Grace's actuator module when it is integrated. Include unknown card, schedule
closed, clock unavailable, anti-passback, and exit-without-entry. The LCD
continues to show the person's name when known and `DENIED`; the serial line
records the exact result.

Use a second, clearly different pattern for a suspected tailgate (for example,
three short pulses). Keep the sound pattern in one alert module so the access
rules do not contain pin toggles or delay loops. Queue the alert request to a
small worker task so the RFID polling loop does not pause while the buzzer
sounds. If the alert queue is full, log the missed alert; never block a room
light task or hold the entrance state mutex while making sound.

### Proposed simulation pin

Reserve **GPIO2** for a passive piezo buzzer in the combined `TR_ROLE=0`
simulation. It is unused by the current room, RFID, LCD, and keypad assignments.
The buzzer is an output only after boot; do not add a pull-up that drives GPIO2
high during reset. GPIO2 is an ESP32 strapping pin, so this is a Wokwi
presentation choice only. For a physical board, select a reviewed non-strapping
pin or an output expander before wiring a buzzer. See Espressif's [ESP32 boot
strapping guidance](https://docs.espressif.com/projects/esp-hardware-design-guidelines/en/latest/esp32/schematic-checklist.html#strapping-pins).

Grace owns the actuator pin map and Wokwi parts. Until that module and circuit
change are integrated, do not claim that the buzzer or door output is wired or
working.

## Anti-passback and tailgating are different

**Anti-passback** can be decided from the current card and inside list. If the
same registered card is presented at entry while its user is already inside,
keep the existing denial, add a clear `ANTI-PASSBACK` alert log, and request the
normal denied buzzer. An exit card that is not recorded inside is also denied
and alerted. Do not change the occupancy list after either denial.

**Tailgating** means more people pass through a controlled doorway than were
authorized. An RFID reader alone cannot count people who follow the cardholder.
A real deployment needs a passage counter, such as a pair of directional
break-beams or a turnstile counter, and a defined time window after an access
grant. Count one authorized passage per grant. If the counter reports an extra
passage without another grant, raise a suspected-tailgate alert.

The current one-chip Wokwi diagram has no door or passage counter. For a
repeatable presentation, pressing keypad `B` on the public screen may inject a
**simulated tailgate alert**. Label it as a test input; do not describe it as
automatic tailgate detection. It uses no new GPIO, so it does not overlap the
keypad, RFID, LCD, room sensors, or Trevor's logical link.

Do not assign an unrecognized passage to the last cardholder or invent a named
person in the inside list. Mark occupancy as **uncertain** after a tailgate
signal and include that warning in the evacuation report. A staff member must
reconcile the count with the physical doorway before treating the list as
complete.

## Evacuation report

Press keypad `C` on the public screen to produce a report. It is read-only and
does not require an admin login: it displays registered names already held by
the entrance roster, but cannot unlock a door or change access settings. Edward's future
serial command can call the same report helper; it must not build a separate
occupancy list.

Take one consistent snapshot of the entrance users and `inside[]` state, then
print:

- report time in Kampala time, or `clock not synchronized` if SNTP is not ready;
- the number of registered users currently recorded inside;
- each recorded inside user's display name (never print card UIDs);
- whether the count is `CONFIRMED` or `UNCERTAIN` because of a suspected
  untracked passage.

Example:

```text
EVACUATION REPORT
Time: 15:42 EAT
Recorded inside: 2
- Demo User 1
- Demo User 2
Occupancy: UNCERTAIN - suspected untracked passage
```

When nobody is recorded inside, print `Recorded inside: 0` and `- none`. Do not
clear the inside list after printing. The list reflects RFID events since this
boot; until storage/recovery is implemented, a reset loses that live state. If
the clock is unavailable, still print the report and state that its time is
unknown. If occupancy is uncertain, never label the report as a complete
roll-call.

## Acceptance scenarios for Leon

1. Present an unknown card: see `DENIED`, the reason in serial, and one short
   buzzer pulse.
2. Present a valid card at entry, then present it at entry again: the second
   scan leaves occupancy unchanged, logs `ANTI-PASSBACK`, and requests the
   denied pulse.
3. Present an exit card for a user not recorded inside: occupancy stays the
   same and the denied pulse is requested.
4. Press `B` on the public keypad screen: see a clearly labelled simulated
   tailgate alert; occupancy becomes uncertain but no fictional user is added.
   Grace's buzzer feedback is verified after her actuator module is integrated.
5. Press `C`: the report lists all names still recorded inside, shows the
   current count and uncertainty, and leaves state unchanged. Repeat with no
   users inside and with SNTP unavailable.
6. Trigger room motion while Grace's actuator output is active: Trevor's room
   light and off-delay continue normally. Confirm this in the one-ESP32
   `esp32dev` simulation after actuator integration.

## Ownership and implementation order

1. Team records the selected Leon innovation in the issue before claiming it in
   the report. Anti-passback already exists; the added alert/report behavior
   needs implementation and demonstration.
2. Add the alert module and buzzer to the diagram/pin map; verify boot and room
   behavior in the Wokwi simulation.
3. Add tailgate alert injection on `B`, clearly marked as simulation-only.
4. Add the snapshot-based report on `C` and document that it lists recorded
   users, not unidentified people.
5. Leave real passage-counter hardware and physical buzzer pin selection as
   future physical-design work. Do not imply the keypad test proves automatic
   tailgating detection.
