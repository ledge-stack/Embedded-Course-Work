# Completion checklist — simplified revision, 3 November 2026

The keypad, LCD and dashboard remain. Use only pio run -e esp32dev.
SIMPLIFICATION.md records this revision's build and host tests. Earlier Wokwi
RFID results and FIX_VERIFICATION.md describe prior revisions, not a passing
live run of the new circuit.

Before presenting, record genuine checks for:

1. Startup, SD mount, both readers and SNTP readiness.
2. Keypad login/schedule, dashboard login/schedule through the gateway and
   serial login/schedule; confirm each sees the same hours.
3. Registered entry/exit, duplicate entry/exit, unknown card, both-door late
   denial and clock-unavailable denial, with unchanged state after denials.
4. Separate GPIO32 entry and GPIO12 exit indicators and independent closure.
5. Both PIR/light channels, renewed motion, delayed off, rooms and usage.
6. Confirm-empty only after a physical check; demonstrate known-zero light
   suppression and uncertain-count behavior.
7. SD records, roster changes, occupied-user deletion refusal and reboot
   recovery of roster/schedule. Presence remains volatile.
8. Missing/full SD behavior and recovery; host substitutes are not physical
   card-failure evidence.
9. Member details, actual contribution statements, individual video links,
   report and required simulation/submission links.

There is no longer a link-loss/ACK/retry demonstration or three-board UART
deployment. Do not claim those from the archived version as current features.
