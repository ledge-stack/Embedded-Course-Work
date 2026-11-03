# Component ownership — current single-board build

| Member/area | Source |
| --- | --- |
| Leon: entrance rules, RFID, clock, LCD, keypad, dashboard | components/entrance |
| Trevor: room tasks, snapshots, timing/usage and event worker | components/trevor |
| Edward: SD/NVS and serial management | components/storage, components/cli |
| Grace: entry and exit indicators | components/actuator |
| Shared verifier, types and startup | components/auth, include/common, src/main.c |

rooms/comms are compatibility build aliases over Trevor's implementation, not
additional controllers. No physical-role builds, CRC/ACK or link switch remain.
Keypad and dashboard are retained. Presentation claims must reflect actual
individual work and observed results. See SYSTEM_OPERATION_REPORT.md and the
individual speaking guides; SIMPLIFICATION.md records this revision's proof.
