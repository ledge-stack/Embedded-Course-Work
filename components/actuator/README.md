# Ahurira Grace — entrance feedback

`runtime.c` drives the combined-demo blue status LED after an RFID scan. A
granted scan gives a short pulse; a denied scan gives a longer pulse. The output
is simulation feedback only; it does not operate or unlock a real door.

GPIO12 is used because the combined board has no other free output. It is an
ESP32 boot-strapping pin, so the diagram keeps it low with an external pulldown.
The blue LED uses a 330 Ohm series resistor and GPIO12 also has a 10 kOhm
pulldown resistor to ground.
The board resistor and boot behavior must be checked again if the wiring changes.
