# Firmware component ownership

| Folder | Owner | What it contains |
| --- | --- | --- |
| `entrance/` | **Leon Nsitta** | RFID decisions, occupancy, schedule, display, keypad, dashboard, entrance integration |
| `trevor/` | **Kato Trevor** | Room sensing, lighting delays, local snapshots, event queue and synchronization |
| `storage/`, `cli/` | **Kagimu Edward** | SD event log, saved user roster, serial status and administration commands |
| `actuator/` | **Ahurira Grace** | Separate entry/exit door indicators and independent timer task |
| `common/`, `auth/` | Shared | Types and the demo administrator PIN verifier |
| `rooms/`, `comms/` | Trevor interface aliases | Public boundaries backed by Trevor's implementation |

See [`../docs/COMPONENT_OWNERSHIP.md`](../docs/COMPONENT_OWNERSHIP.md) for the
full map, responsibilities, and remaining end-to-end evidence.
