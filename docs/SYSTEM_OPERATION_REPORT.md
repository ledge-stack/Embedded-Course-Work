# CoCIS Smart Control and Monitoring System

## System operation report — simplified revision, 3 November 2026

Project repository: https://github.com/ledge-stack/Embedded-Course-Work
Complete the group's final submission and individual video links with genuine evidence.

## Purpose and architecture

One ESP32 provides two lecture-room terminals and the building entrance/exit.
Two PIRs control lights, two RFID readers identify entry and exit, and an SD
card stores events. The LCD, keypad, Wi-Fi dashboard and serial management
interface are retained. Wi-Fi supplies SNTP time in Africa/Kampala.

```text
Room tasks -> light GPIOs and mutex-protected snapshots
Changed rooms + access decisions + schedule audits -> event queue -> one SD writer
RFID -> entrance policy -> inside list + separate door indicators
Keypad / dashboard / serial -> shared authenticated schedule API
Entrance count -> room confirmed-empty override
```

The simpler one-board design has no virtual UART frames, CRC, ACK/retry,
heartbeat or physical-role firmware. See INTERFACES.md for all GPIOs. Room
pins, keypad, LCD, RFID and SD wiring are preserved. The old link switch is
replaced by the entry indicator on GPIO32; the existing GPIO12 LED indicates exit.

## Access and occupancy

Enabled UIDs are checked against a 32-slot roster. Clean devices use DEADBEEF
and 11223344. Repeated entry and exit without recorded entry are denied. Both
directions require a synchronized clock and an open schedule. Ordinary and
overnight intervals include opening and exclude closing; equal endpoints fail.

Accepted entry/exit changes the live inside list. Each direction has its own
three-second LED door output, closed by an independent task rather than waiting
for SD. Denials never open an output. The bridge also closes both indicators
outside hours, at roughly 250 ms polling resolution. This is a simulation of
door operation; actual physical passage is not detected.

Presence is based on granted scans, not PIR headcount. The inside list is
volatile; after reboot occupancy is marked uncertain. Staff must check an empty
building before admin `confirm-empty`. A keypad B tailgate marker makes count
uncertain without inserting a person. C prints a read-only evacuation roll-call.
The schedule monitor reports people still inside at closing time.

## Lighting and innovation

Each room task samples every 20 ms. Motion refreshes its last-motion time and
turns its requested light on. After five seconds without motion the request
turns off; renewed motion restarts the delay. The effective light is suppressed
only when the building count is authoritative and zero. Uncertainty does not
mean empty.

Light-use counters measure effective on-time and time suppressed by the known-
empty rule since boot. `usage` shows these durations; no lamp wattage or kWh is
claimed. Together with roll-call and anti-passback, this remains a useful
innovation without a simulated network protocol.

## Management and authentication

All three administrator interfaces remain: keypad A/PIN/# and HHMM schedule
entry; browser login and schedule form through Wokwi's Private IoT Gateway;
and UART0 CLI. They use one PIN verifier and schedule mutation API. Demo PIN
2468 uses the existing PBKDF2 verifier, failed-attempt lockout and session limits.
The web form decodes encoded values and retains session/CSRF checks.

Serial read commands: help, status, inside, rooms, usage, users, records, schedule.
Admin commands: login 2468, open HH:MM, close HH:MM, useradd NAME UIDHEX,
userdel ID, confirm-empty, logout. Input survives idle/partial reads, rejects
overlong lines and validates time/arguments. Occupied users cannot be removed.

## Storage and synchronization

NVS owns schedule and registered-user roster. SD stores access, room and
schedule events plus a roster mirror. NVS wins over an older reinserted card;
legacy SD roster files are loaded only if no NVS roster exists. An SD mirror
failure does not undo a committed NVS roster.

One 16-item event queue and one writer serialize event appends. A filesystem
mutex protects concurrent file operations. Room snapshots update independently
of the queue; queue pressure or SD failure does not stop local lights. Room
records are asynchronous and may be dropped, with counters. Access/schedule
callers wait up to five seconds for their own request-ID completion. SD success
means append, flush, sync and close succeeded; failure is reported without
rolling back an access decision. Records reads about the last 2 KB under the
file lock and prints after releasing it.

The entrance mutex owns access/schedule/roster snapshots; a configuration mutex
serializes complete NVS/RAM schedule writes. Trevor's mutex protects room
snapshots, occupancy input and counters. Tasks communicate through APIs and
copied events rather than modifying another module's private state.

## Member areas

| Member | Current code area |
| --- | --- |
| Leon Nsitta Kalungi | Entrance rules, RFID, clock, LCD, retained keypad/dashboard and integration |
| Kato Trevor | Two room tasks, off-delay, usage, local snapshots, event queue, single writer and occupancy input |
| Kagimu Edward | Shared SPI, SD/NVS roster and records, UART0 management |
| Ahurira Grace | Separate entry/exit door indicators and independent timer task |

Members must describe their actual work and understanding; module labels do not
establish individual authorship. The removed protocol must not be presented as
part of the current build.

### Member identification details

| Member | Registration/student identification |
| --- | --- |
| Leon Nsitta Kalungi | Registration No. `25/U/24111`; Student No. `2500724111` |
| Kato Trevor | To be supplied by the member. |
| Kagimu Edward | To be supplied by the member. |
| Ahurira Grace | To be supplied by the member. |

The presentation guide folder contains a separate speaking outline for each
member. Each person should check their identification details and be ready to
explain their assigned code area.

## Verification and limits

Read SIMPLIFICATION.md for fresh build/test results for this revision and
PRESENTATION_DEMO.md for the live sequence. Previous RFID Wokwi evidence belongs
to an earlier firmware revision. The simplified circuit and both management
interfaces still need a fresh live run; host tests are not hardware evidence.

Remaining checks include real SD-full/removal/power-loss behavior, roster reboot
recovery, keypad and browser administration, both door outputs and room-light
suppression. The live inside list is not reconstructed after reset. LEDs do not
drive locks or mains fixtures, the B key is a simulated tailgate marker, and
usage is time rather than measured energy. Include the Wokwi demonstration link,
individual videos and complete member IDs required by the coursework; confirm
any later submission-format changes with the lecturer.
