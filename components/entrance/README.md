# Leon Nsitta — entrance and access control

This folder is Leon's implementation. `runtime.c` reads both RFID readers,
checks access rules, owns the inside list and schedule, records events, and
updates the display. `core.c` contains simple testable access rules. The
display, keypad, dashboard, and MFRC522 driver are supporting pieces.

The runtime calls Grace's feedback module after each scan. The entrance runtime
remains the only owner of building occupancy and the access schedule.

## Member cards and administrator views

When there is no saved roster, the four Wokwi cards are initialized as follows:

| Card | Saved roster name | Greeting name | UID |
| --- | --- | --- | --- |
| Blue | Leon Nsitta Kalungi | Leon | `DE:AD:BE:EF` |
| Green | Kato Trevor | Trevor | `11:22:33:44` |
| Yellow | Kagimu Edward | Edward | `55:66:77:88` |
| Red | Ahurira Grace | Grace | `AA:BB:CC:DD` |

The LCD briefly shows `Welcome <name>` after a granted entry or
`Goodbye <name>` after a granted exit. A denied scan is also shown briefly.
After signing in with the keypad, menu option `3` pages through each registered
person and their current `IN`, `OUT`, or uncertain `OUT?` state. Option `4`
pages through up to eight latest scans. The signed-in Wi-Fi dashboard shows
the roster and the same recent activity. This short activity list lives in RAM
and resets on reboot; the longer event history is saved on SD and can be read
with the serial `records` command.
