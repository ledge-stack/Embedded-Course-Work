# CoCIS Smart Control and Monitoring

One ESP32 runs two lecture-room controllers, entry/exit RFID, SD records and
authenticated management. **The keypad, LCD and Wi-Fi dashboard are retained.**
The 3 November simplification replaces simulated UART frames, ACKs and retries
with local snapshots and one FreeRTOS event queue. The earlier source is saved
in the verified backup outside this repository.

## Build and run

```sh
pio run -e esp32dev
```

Open `diagram.json` with Wokwi and start the simulation. This is the only build
environment; there are no separate room/coordinator firmware roles. Wait for
`SNTP synchronized` before presenting a card. The default schedule is
08:00-18:00 Kampala time, checked at both entry and exit. Outside hours, use an
authenticated interface to choose an appropriate demonstration schedule.

Default cards: `DEADBEEF` and `11223344`, unless a saved roster exists.
In Wokwi, click the left RFID reader and press **TAP** for entry; use the
right reader with the same card for exit. The local serial bridge uses port
`4001` in `wokwi.toml` so another project can use port `4000`.
If access reports `CLOCK_UNAVAILABLE`, wait for synchronization. The clock
uses two Cloudflare numeric NTP addresses to avoid delayed gateway DNS, with
automatic fallback after a receive timeout. If both servers are unreachable,
the clock keeps retrying and access stays disabled; synchronization has no
guaranteed startup time.
See [RFID verification](docs/RFID_VERIFICATION.md) for the tested results.

Default demo PIN: `2468`. An allowed entry opens the green GPIO32 door LED;
an allowed exit opens the blue GPIO12 LED. Each closes after about three
seconds independently of SD delays. Denied scans do not open either output.

## Management

- Keypad: `A`, PIN `2468`, `#`; choose `1` for opening or `2` for closing time.
  Enter four digits HHMM and `#`. `*` clears; `D` cancels/logs out.
- Public keypad `B` marks simulated tailgate uncertainty; `C` prints a roll-call.
- Web dashboard: same schedule and PIN; browser access requires Wokwi's
  Private IoT Gateway, normally at `http://localhost:9080/`.
- Serial reads: `help`, `status`, `inside`, `rooms`, `usage`, `users`, `records`,
  `schedule`. Admin: `login 2468`, `open HH:MM`, `close HH:MM`,
  `useradd NAME UIDHEX`, `userdel ID`, `confirm-empty`, `logout`.

After restart the inside list is uncertain. After physically checking an empty
building, log in and use `confirm-empty`. While the authoritative count is zero,
PIR motion is deliberately prevented from switching the lights on. Admit a
registered person before demonstrating normal room lighting.

## Code to explain

| Area | Code |
| --- | --- |
| Startup and occupancy bridge | `src/main.c` |
| Trevor: room timing and usage | `components/trevor/core.c` |
| Trevor: two room tasks, snapshots, event queue and SD worker | `components/trevor/runtime.c` |
| Leon: RFID, rules, clock, LCD, keypad and dashboard | `components/entrance/` |
| Edward: SD/NVS storage and serial menu | `components/storage/`, `components/cli/` |
| Grace: independent entry/exit door indicators | `components/actuator/runtime.c` |
| Shared PIN verifier and public types | `components/auth/`, `include/` |

Room tasks own their lights. A mutex protects local snapshots/counters. One
queue carries room, access and schedule events to one SD writer. Room logging
is nonblocking; access/schedule callers wait up to five seconds for the writer's
own completion result. Queue admission is not a durability guarantee.

NVS owns the saved schedule and registered roster; SD holds event history and a
roster mirror. Older SD files cannot override a newer NVS roster. The live inside
list and usage durations are RAM-only. Usage is time, not kWh.

## Verification and presentation

See [simplification record](docs/SIMPLIFICATION.md),
[pin/interface contract](docs/INTERFACES.md),
[report](docs/SYSTEM_OPERATION_REPORT.md),
[presentation sequence](docs/PRESENTATION_DEMO.md), and
[Trevor's guide](docs/TREVOR_GUIDE.md). Full host checks are documented in
`test/README`. Build/host results do not establish a live Wokwi demonstration.
Retain genuine evidence and finish member details, video and submission links.
