# Leon Nsitta — entrance and access control

This folder is Leon's implementation. `runtime.c` reads both RFID readers,
checks access rules, owns the inside list and schedule, records events, and
updates the display. `core.c` contains simple testable access rules. The
display, keypad, dashboard, and MFRC522 driver are supporting pieces.

The runtime calls Grace's feedback module after each scan. The entrance runtime
remains the only owner of building occupancy and the access schedule.
