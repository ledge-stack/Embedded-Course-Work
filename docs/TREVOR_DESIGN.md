# Trevor's room terminals, communication and synchronization

Approved scope: issues 4, 8, 9 and 10 in IMPLEMENTATION_PLAN.md. This design supplies the room side and coordinator integration interfaces, without implementing Leon's RFID or Edward's SD/authentication work.

Two independently controlled PIR inputs drive two lights. Motion turns a light on immediately; the configurable off-delay begins at the last active sample. Absence clears the raw occupancy signal immediately, while the light stays on until the delay expires. PIR is a presence estimate, not a people counter.

Physical deployment uses a coordinator with two dedicated 115200-baud UART links, one per room, with crossed TX/RX and common ground. A versioned, fixed-size, CRC-protected frame carries room ID, boot session, sequence, uptime, occupancy, light and delay. Stop-and-wait retries preserve the latest unacknowledged state, followed by a fresh snapshot. Periodic snapshots recover after coordinator restart. Persisted boot counters distinguish terminal restarts; sequence/session comparisons reject stale snapshots. Link loss marks data stale without switching off room lights.

Wokwi currently supports only one microcontroller per simulation (https://docs.wokwi.com/faq). The default demo therefore runs three logical terminals on one ESP32, passing the actual encoded frames through FreeRTOS queues. Hardware role builds use the same portable room/protocol/reliability code with UART transport. The single-chip demo is not evidence of physical multi-ESP32 UART operation.

One task owns the coordinator receiver state. A mutex protects shared snapshots and configuration. A bounded queue feeds one storage-consumer task, which calls Edward's SD event writer. Queue saturation and write failures appear in diagnostics and cannot block local light control. Access events use a bounded payload and share the storage queue. Each room task also reports light-on time and time suppressed by the confirmed-empty rule to the serial `usage` command; these duration counters reset on reboot and are not energy measurements. Shared access/schedule authority stays with Leon and Edward.

Retry refinement: after five transmissions at 500 ms intervals, expire the unacknowledged snapshot and publish a fresh one. This prevents an ACK-only outage from permanently hiding newer room activity. Count timeouts explicitly. Room-state synchronization coalesces intermediate events during an outage; it is not a lossless event journal.

Verification: compile and execute portable C and threaded runtime-adapter tests on Windows; compile demo, coordinator and both room firmware roles; validate Wokwi wiring JSON. Supply manual simulation and real-hardware scenarios with unperformed checks explicitly labelled.
