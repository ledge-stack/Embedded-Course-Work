# Module integration — simplified revision

Entrance owns access/inside/schedule state. Trevor owns room light tasks and snapshots. Room, access and schedule events share one queue and writer; Edward implements the actual SD append and CLI. Grace owns the separate door indicators. The occupancy bridge shares the authoritative count and closes doors outside hours. Keep private state behind public APIs.

Read [current report](SYSTEM_OPERATION_REPORT.md), [pin/API contract](INTERFACES.md),
[Trevor guide](TREVOR_GUIDE.md), and [presentation sequence](PRESENTATION_DEMO.md).
The earlier expanded guides are preserved in the pre-simplification archive.
