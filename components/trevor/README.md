# Kato Trevor — room sensing and communications

This folder is Trevor's implementation. `core.c` contains the small room and
message rules. `runtime.c` samples the two room PIR inputs, controls lights,
tracks off-delay timers, handles logical-room or UART messages, and owns the
bounded event queue plus its single storage worker.

In the combined demo, the entrance coordinator supplies the authoritative
building inside count. Room lights are forced off when that known count is zero.
An unknown count does not mean the building is empty.

Each room task counts light-on time and requested light-on time suppressed by
the known-empty rule. Edward's serial `usage` command reports these totals since
boot. They are duration measurements, not a kWh estimate.
