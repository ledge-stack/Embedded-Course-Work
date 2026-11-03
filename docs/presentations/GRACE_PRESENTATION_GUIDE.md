# Grace's presentation guide — door indicators

The actuator module now provides two separate simulated doors: entry GPIO32
and exit GPIO12. The entrance calls actuator_runtime_signal(point, granted).
Only a grant opens the selected output. A separate task checks both deadlines
every 20 ms and closes each about three seconds later, independently of SD.
The occupancy bridge closes both outside the access window.

Show a registered entry with its green LED, then exit with its blue LED. Show
that unknown or duplicate scans do not open a door. Explain GPIO, deadline,
FreeRTOS task and mutex. The GPIO12 pulldown remains. These are indicators, not
real locks. The previous short/long grant/deny feedback pulse is no longer used.
