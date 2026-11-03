# Leon implementation guide — simplified revision

Entrance core.c contains UID, anti-passback and schedule rules. runtime.c owns the live state, SNTP, reader/display tasks, schedule persistence and event handoff. Keypad and dashboard are retained. Granted scans call the direction-aware actuator API before waiting for SD; the door task closes independently. See PRESENTATION_DEMO.md for the current workflow.

Read [current report](SYSTEM_OPERATION_REPORT.md), [pin/API contract](INTERFACES.md),
[Trevor guide](TREVOR_GUIDE.md), and [presentation sequence](PRESENTATION_DEMO.md).
The earlier expanded guides are preserved in the pre-simplification archive.
