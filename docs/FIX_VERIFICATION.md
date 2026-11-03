# Direct project fixes — 2 November 2026

Historical record: superseded for architecture/line counts by SIMPLIFICATION.md.

These changes were applied to this repository's working files, not only to a
separate review copy. They have not been committed or pushed by this workflow.

## Corrected behavior

- Keypad PIN masking fits all four digits without writing beyond its buffer.
- The dashboard decodes URL-encoded form values, including `08%3A00`, checks
  exact field names and rejects duplicate or malformed values. Authentication
  and CSRF requirements remain in place.
- The serial task survives temporary empty reads, retains partial lines,
  handles backspace/CRLF, and discards a complete overlong line. Command times
  use strict `HH:MM`, and extra/missing command arguments are rejected.
- NVS is the authoritative registered-user roster. SD is an optional mirror;
  an older reinserted SD card cannot override a newer NVS roster. A legacy
  version-1 SD file remains usable when no NVS roster exists. An SD mirror
  failure is reported as a warning after a successful NVS commit, so the CLI
  does not roll back the live roster while the saved one has already changed.
- The saved roster is loaded before the RFID scan tasks start.
- A separate configuration mutex serializes the complete NVS/RAM schedule
  update and its audit request. Concurrent administrators cannot interleave
  the opening/closing writes into a mismatched saved schedule.
- `records` reads approximately the last 2 KB of the SD log and releases the
  filesystem mutex before slow UART output.

Documentation and the storage API comment describe the NVS-first recovery rule.
The tracked SDK configuration was preserved; the installed toolchain's automatic
configuration normalization was not retained as an unrelated source change.

## Fresh verification

| Check | Result |
| --- | --- |
| Direct `pio run -e esp32dev` in this project | PASS, exit code 0 |
| Trevor portable room/protocol tests | PASS |
| Threaded runtime integration tests | PASS |
| Entrance authorization/schedule tests | PASS |
| New regression suite | PASS, 13 tests |

The direct firmware build used PlatformIO Espressif32 7.0.1, ESP-IDF 6.0.1 and
Xtensa GCC 15.2.0. It took approximately 3 minutes 47 seconds. Reported static
RAM was 39,544 / 327,680 bytes (12.1%); application flash was
971,881 / 1,048,576 bytes (92.7%). Static RAM figures do not include all runtime
heap and task-stack allocations. The SDK emitted its existing Kconfig notes;
they did not prevent compilation or linking.

The regression suite uses actual affected C function bodies with host substitutes
for HTTP requests, NVS, input and synchronization. It checks PIN buffer guards,
encoded and reordered dashboard fields, malformed inputs, CLI overflow and
partial/idle reads, overlapping schedule writes, recent-record output without
the SD lock, legacy roster reads, NVS failures, and stale/missing/failed SD
mirrors. The reviewed defects were reproduced against the original functions
before the corresponding corrected checks passed. The test instructions are
in `test/README`.

## Remaining live demonstration

The corrected firmware has not been run in Wokwi in this fix session. Rehearse
keypad login and schedule saving, a browser schedule change through the gateway,
CLI input after an idle boot, SD records, roster changes/reboot, both RFID readers,
room off-delay, confirmed-empty suppression and link recovery. Host tests do not
establish physical SD power-loss behavior or real hardware operation.

The live inside list remains volatile by design, with occupancy marked uncertain
after restart. The feedback LED remains an indicator; this patch does not add a
door actuator, persistent inside list or physical multi-board network.
