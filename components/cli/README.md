# Kagimu Edward — serial CLI

`runtime.c` reads simple commands from the UART0 console. Read commands show
system health, people inside, users, room state, light-use totals, schedule,
and saved event records. The read-only `usage` command reports light-on time
and light-on time suppressed by the known-empty rule since boot; it does not
estimate kWh. `login PIN` opens a 60-second administrator session for schedule edits,
user add/delete, and `confirm-empty`. User changes go through Leon's roster API
and are committed to NVS with an optional SD mirror. A person currently inside cannot be
deleted. `confirm-empty` requires a human building check after reboot or a
tailgate uncertainty alert.

The default presentation PIN is `2468`; this is a coursework demo verifier,
not production authentication. Run the combined `esp32dev` firmware to use the
CLI. UART0 is shared with ESP-IDF log output.
Input survives temporary empty reads, handles backspace, and discards an entire
overlong line. Times must use HH:MM; extra command arguments are rejected.
