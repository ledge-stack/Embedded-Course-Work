# Repository structure — simplified revision

src/main.c starts the system. components contains implementations; include contains public headers. test contains host logic and integration checks; scripts/test-all.ps1 runs them. diagram.json defines the circuit, wokwi.toml maps firmware/SD, and platformio.ini defines the sole esp32dev target. .pio and .qa are generated outputs.

Read [current report](SYSTEM_OPERATION_REPORT.md), [pin/API contract](INTERFACES.md),
[Trevor guide](TREVOR_GUIDE.md), and [presentation sequence](PRESENTATION_DEMO.md).
The earlier expanded guides are preserved in the pre-simplification archive.
