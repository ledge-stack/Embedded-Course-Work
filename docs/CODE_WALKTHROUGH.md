# Code walkthrough — simplified revision

Start at src/main.c. It initializes storage, rooms, doors and entrance, then starts the occupancy bridge and CLI. Follow a card from mfrc522_poll to entrance_apply_scan, actuator_runtime_signal and the SD event queue. Follow motion through tr_room_sample, the local snapshot and changed-room event. Keypad, dashboard and CLI share the schedule API.

Read [current report](SYSTEM_OPERATION_REPORT.md), [pin/API contract](INTERFACES.md),
[Trevor guide](TREVOR_GUIDE.md), and [presentation sequence](PRESENTATION_DEMO.md).
The earlier expanded guides are preserved in the pre-simplification archive.
