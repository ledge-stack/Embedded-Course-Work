# Kato Trevor — Presentation Guide

## What I worked on

I implemented the room sensing and lighting logic, the room/coordinator
communication rules, retry and synchronization tracking, and the FreeRTOS
queues used to hand events to the storage worker. I also connected the
building-empty safety input and the room-light usage counters.

## Suggested explanation

> “My part controls two lecture-room light channels. Each room task reads its
> own PIR sensor. Motion turns the light on; when motion stops, an off-delay
> keeps it on briefly. If motion returns, the delay starts over. A PIR tells us
> that there is motion; it does not count how many people are in the room.”

> “The room task publishes a state message with the room number, boot session,
> sequence number, motion, light, and delay. The coordinator checks the frame,
> detects duplicates or old messages, acknowledges new messages, and the sender
> retries if an acknowledgement is missing. In the one-board presentation
> build, FreeRTOS queues simulate this link; the other roles can use UART.”

> “The entrance count is supplied as a safety input. When the count is known to
> be zero, the room controller forces lights off. If the count is uncertain, it
> does not assume the building is empty. Room events use a bounded storage
> queue and one writer task, so room loops do not write the SD file directly.
> Access and schedule-change events share that queue and wait for the storage
> result so the entrance can report whether its row was saved.”

## Code to show

1. `components/trevor/core.c` — small room off-delay and message encode/decode,
   sequence, retry, and receive rules.
2. `components/trevor/runtime.c` — room tasks, effective-light rule, logical
   queue/UART transport, coordinator, storage worker, and diagnostics.
3. `include/trevor/core.h` — frame size and timing constants.
4. `include/trevor/runtime.h` — room snapshots, occupancy input, event handoff,
   and storage-event interfaces.
5. `src/main.c` — where the entrance count is passed to the room runtime.

## Suggested live demonstration

1. Activate Room 1's PIR in the Wokwi diagram and show its light output turn on.
2. Stop motion and show the five-second default off-delay.
3. If time permits, operate the link switch and point to retry/stale status in
   the serial output. Local room control should continue.
4. Show the `usage` command and explain that it reports durations since boot,
   not energy consumption.
5. Explain that the room lights turn off when the building count is known to be
   zero; do not claim an uncertain count means empty.

## Concepts to explain simply

- **Task:** an independently scheduled piece of firmware work.
- **Queue:** a bounded first-in, first-out mailbox between tasks.
- **Acknowledgement:** a message that confirms the receiver got a frame.
- **Sequence number:** lets the receiver distinguish new data from repeats or
  old data.
- **Retry and timeout:** resend when a confirmation does not arrive, then
  report a timeout if communication remains unavailable.
- **Mutex:** a lock around shared snapshots and counters.

## Limits to state accurately

The single Wokwi board uses logical room links rather than three physical ESP32
boards. PIR sensors detect movement, not people. Light-use reports contain
time measurements, not kWh. Link-loss behavior and some room recovery scenarios
still need full end-to-end demonstration; use the completion plan for the latest
evidence.
