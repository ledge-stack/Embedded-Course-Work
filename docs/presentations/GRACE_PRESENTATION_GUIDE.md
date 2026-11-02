# Ahurira Grace — Presentation Guide

## What I worked on

I implemented the scan-feedback actuator interface. It gives a visible result
after an RFID decision: a short blue LED pulse for an allowed scan and a longer
pulse for a denied scan.

## Suggested explanation

> “My part is the feedback output after an RFID scan. The entrance controller
> calls one actuator function with the result. If access is granted, the LED
> gives a short pulse. If access is denied, the pulse is longer, so a person
> can distinguish the two outcomes visually.”

> “The actuator has a small public header, so the entrance module can request a
> feedback pulse without setting the GPIO itself. The feedback LED is an
> indicator only; it is not connected to or controlling a door lock.”

## Code to show

1. `include/actuator/runtime.h` — the public actuator functions.
2. `components/actuator/runtime.c` — GPIO setup, grant/deny pulse duration, and
   pulse shutoff.
3. `components/entrance/runtime.c` — where the access outcome is passed to the
   actuator.
4. `diagram.json` and `docs/INTERFACES.md` — the LED, resistor, GPIO12, and
   external pulldown wiring.

## Suggested live demonstration

1. Scan a registered card during open hours and observe the short pulse.
2. Scan an unknown card or repeat an entry and observe the longer denial pulse.
3. Explain the serial access result alongside the visual output.

If the clock is outside the schedule, a registered card will be denied. That
still demonstrates the denial feedback path.

## Concepts to explain simply

- **Actuator interface:** a module that controls an output device.
- **GPIO output:** a digital pin set high or low to control the LED.
- **Pulse duration:** how long the LED stays on before the entrance polling
  task calls the update function to turn it off.
- **Public header:** the function declaration other modules use to request the
  LED signal.

## Limits to state accurately

The LED is visual scan feedback, not an electronic lock or door motor. The
combined diagram uses GPIO12, an ESP32 boot-strapping pin, so the diagram
includes a pulldown resistor. Physical board startup behavior should be checked
before reusing this wiring outside the simulation. Use the completion plan for
the latest status of a full visual LED demonstration.
