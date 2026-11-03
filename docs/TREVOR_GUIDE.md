# Trevor: room control, events and synchronization

The room subsystem now uses two local tasks on one ESP32. Read `core.c` first,
then `runtime.c`; their headers declare the public data and functions.

## Pure room rules

`tr_room_sample()` reads a supplied motion value and timestamp. A high sample
turns the requested light on and refreshes `last_motion_ms`. Low samples keep
the light on until the off-delay expires. Renewed motion restarts that delay.
Default delay is five seconds; the API accepts zero to one hour.

`tr_usage_sample()` adds elapsed milliseconds to the light-on or suppressed
total according to the previous interval's state. The `usage` serial command
reports durations since boot, not electricity consumption.

## Runtime flow

```text
PIR27 -> Room 1 task -> GPIO26 light --+
PIR33 -> Room 2 task -> GPIO25 light --+-> changed room events
RFID decisions ----------------------+-> event queue -> single SD worker
Schedule audit requests -------------+
```

Each room task samples every 20 ms and controls only its own GPIO. It copies
the latest state and usage totals into a mutex-protected snapshot. `rooms`
reads that snapshot directly: there is no virtual wire protocol or link switch.
Only state changes produce room log events. A full queue increments the drop
counter; it does not prevent the local light or snapshot from updating.

The entrance module supplies building count and certainty every 250 ms through
`src/main.c`. Effective light = local request AND (count unknown OR count > 0).
Thus a confirmed-empty building overrides motion; uncertainty is not treated as
proof of an empty building. PIR motion does not measure room headcount.

## Synchronization and storage

The shared mutex protects snapshots, delays, occupancy input and counters.
GPIO control and SD work do not hold this mutex. A 16-event queue lets room,
access and schedule producers hand work to one writer task.

Room events are asynchronous. Important access/schedule requests include a
request ID and task handle; the writer notifies the waiting task after the SD
append returns. The caller waits at most five seconds and ignores a late reply
for an older request. SD failures and queue drops are counted; failed room rows
are not a persistent retry journal.

The former CRC, frame parser, sequence/session ordering, ACK, retry, heartbeat,
coordinator task and separate-board UART modes have been removed. The archived
source retains that older design. Do not present those features as active.

## Demonstration

1. Wait for SNTP and check schedule/roster.
2. With the building count unknown or a user admitted, trigger each PIR.
3. Show immediate on, delayed off and renewal of motion.
4. Show `rooms` and `usage`.
5. Exit all recorded users, physically check empty, then `login 2468` and
   `confirm-empty`. Motion stays visible but lights remain off at known zero.
6. Show SD rows and storage counters. Explain that host tests use simulated
   GPIO and queues; real SD faults still need a live demonstration.
