# Trevor terminals implementation plan

Goal: implement the approved room, communication and synchronization scope in ESP-IDF C.

Architecture: portable deterministic room/protocol/link logic under src/trevor; an ESP-IDF adapter owns GPIO, UART, queues, mutexes and tasks. Default firmware demonstrates three logical terminals; role builds select physical UART deployment.

1. [x] Add test/test_trevor.c and a Windows PowerShell test runner. Verify failure before implementation. Cover light delay boundaries and renewed motion, CRC corruption, stream resynchronization, malformed input, ACK matching, retries, duplicate/stale state, restart and sequence rollover.
2. [x] Implement include/trevor/core.h and src/trevor/core.c. Run the portable tests until green with strict compiler warnings.
3. [x] Add include/trevor/runtime.h and src/trevor/runtime.c. Initialize GPIO, queues and mutex before tasks. Keep UART decoding and per-room control in their owner tasks. Serialize storage callbacks through one consumer, expose bounded enqueue/snapshot/configuration APIs, and count overflow/unavailable writes. Build all four roles.
4. [x] Add diagram.json, role build flags and physical-role build configurations. Supply reproducible demo instructions for simultaneous motion, delayed light-off, link interruption, recovery and stale status.
5. [x] Document wire format, pins, APIs, ownership, test evidence, limitations, Trevor's report contribution and video outline. Review the diff, rerun affected verification, and copy only implementation files to the requested repository, preserving existing SDK/editor edits.

Commands: scripts/test-trevor.ps1 -Compiler <tcc-or-clang-path>; pio run -e esp32dev -e coordinator -e room1 -e room2; git diff --check.
