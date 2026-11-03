# Presentation sequence — simplified project

1. Build `pio run -e esp32dev` and open `diagram.json` in Wokwi.
2. Wait for SD/reader startup and `SNTP synchronized`. Check the Kampala access
   window; the default is 08:00-18:00. A registered card outside hours is denied.
3. Show the retained LCD and use keypad A -> 2468 -> #. Choose 1 or 2, type
   HHMM and # to save opening/closing time. D logs out; * clears input.
4. If the gateway is available, sign into the retained dashboard at the local
   forwarded address and save a schedule. It uses the same backend as keypad
   and serial. Restore the intended hours after the demonstration.
5. Scan Blue DEADBEEF or Green 11223344 at entry. Show name/count, the green
   entry LED and its independent three-second closure. Repeat entry is denied.
6. Scan the same card at exit. Show count decrement and the blue exit LED.
   Repeat exit and an unknown Red card are denied without opening outputs.
7. While the count is nonzero, trigger both PIRs. Show independent lights,
   renewed-motion behavior and five-second delayed off.
8. On serial show `status`, `inside`, `rooms`, `usage`, `users`, `schedule`,
   and `records`. Records prints approximately the last 2 KB after releasing
   the SD lock. Usage reports time, not kWh.
9. Log in with `login 2468`; demonstrate schedule/roster changes and logout.
   An occupied user cannot be deleted. NVS is the authoritative roster copy.
10. After everyone has exited and staff physically check the building, use
    `confirm-empty`. PIR motion no longer lights an empty building. After a
    reset the count is uncertain until a fresh check; do not claim recovery of
    previous inside flags.
11. Public keypad B marks simulated tailgate uncertainty; C prints roll-call.

The circuit is one board. There is no link-loss switch, remote room UART mode,
frame/CRC/ACK demonstration or automatic passage sensor. The two door LEDs are
simulation outputs, not a physical lock. Do not claim a live check passed solely
because compilation or a host test passed. Save genuine console, SD and video
evidence for this revision. Existing automated RFID scenarios require open
hours for grant assertions and do not cover all keypad/dashboard flows.
