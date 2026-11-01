# Entrance safety behavior and limits

This note describes the safety-related behavior in the current one-ESP32 demo.
The detailed entrance implementation is in
[`LEON_IMPLEMENTATION_GUIDE.md`](LEON_IMPLEMENTATION_GUIDE.md); current test
evidence is in [`COURSEWORK_COMPLETION_PLAN.md`](COURSEWORK_COMPLETION_PLAN.md).

## Implemented behavior

- The entrance core rejects a second entry by a registered user already marked
  inside and rejects exit when that user is not marked inside. Denied events do
  not change the inside list.
- Keypad `B` raises a clearly labeled, simulated tailgate alert and marks the
  occupancy count uncertain. It does not add a person or alter the inside list.
- Keypad `C` prints a read-only roll-call of names already recorded inside.
  Schedule close prints an after-hours report if people remain inside.
- The entrance runtime owns the authoritative schedule and inside list.
  Other modules receive status through its APIs and snapshots.
- Grace's blue LED gives different pulse lengths for allowed and denied scans.
  It is visual feedback only; there is no door lock or buzzer in the circuit.

## Safety limits

An RFID reader cannot detect someone following a cardholder through the door.
The keypad tailgate action is a simulation input, not automatic detection. A
real installation needs a passage counter, such as paired directional
break-beams or a turnstile counter, and a reviewed door controller.

The live inside list is held in RAM and is lost on reboot. At startup the
firmware treats occupancy as uncertain. An administrator may confirm the
building is empty only after a physical check. The roll-call lists registered
names associated with accepted RFID events; it is not an independently verified
headcount and it does not clear state.

The schedule blocks both entry and exit outside configured Kampala hours, as
required by the coursework brief. A denied exit does not remove the person from
the inside list. Unknown cards and exit-without-entry remain denied at all
times.

## Presentation checks

1. During open hours, scan a valid card at entry, then repeat entry and confirm
   the second scan is denied without increasing occupancy.
2. Scan that card at exit and confirm occupancy returns to zero. Repeat exit
   and confirm it is denied.
3. Scan an unknown card and observe the denied result and LED pulse.
4. Press keypad `B` and verify the uncertainty warning without a new person in
   the list. Press `C` and verify the report leaves the state unchanged.
5. Check that both entry and exit are denied after closing, and that a denied
   exit leaves the person's inside status unchanged.
6. Observe the grant/deny LED pulses. Do not describe the LED as a lock or a
   buzzer.

The repository's proposed innovation package is **Occupancy-Aware Safety and
Lighting Analytics**. It combines this safety feature bundle, feedback LED
behavior, empty-building light shutoff, room-link recovery, and a room-light
usage report. The report gives runtime durations only, not measured kWh. The
team should record this package as its selection in the project issue.
