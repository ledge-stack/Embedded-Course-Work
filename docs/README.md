# Documentation guide

Use this index to find the current coursework and presentation material. The
root [README](../README.md) has the project summary, build command, demo PIN,
and main feature list.

## Main presentation and submission documents

- [System operation report](SYSTEM_OPERATION_REPORT.md): the coursework report
  covering purpose, architecture, hardware, operation, contributions, and
  evidence limits.
- [Presentation demo](PRESENTATION_DEMO.md): a step-by-step demonstration
  sequence for the one-ESP32 Wokwi circuit.
- [Individual presentation guides](presentations/README.md): speaking
  outlines for Leon, Trevor, Edward, and Grace.
- [Coursework completion plan](COURSEWORK_COMPLETION_PLAN.md): current
  implementation status, evidence collected, and remaining checks.

## Guides for understanding the repository and code

- [Repository structure guide](REPOSITORY_STRUCTURE_GUIDE.md): explains the
  purpose of the folders, including `components/`, `include/`, `lib/`, and
  `scenarios/`.
- [Code walkthrough](CODE_WALKTHROUGH.md): follows startup, RFID scans, room
  lighting, administration, and storage through the source files.
- [System principles guide](SYSTEM_PRINCIPLES_GUIDE.md): explains FreeRTOS
  tasks, queues, mutexes, buses, persistence, authentication, and timing.
- [Module integration guide](MODULE_INTEGRATION_GUIDE.md): explains which
  module owns each piece of state and how the modules exchange information.
- [Interfaces and pin contract](INTERFACES.md): records public APIs, shared
  data, GPIO assignments, and bus wiring.
- [Component ownership](COMPONENT_OWNERSHIP.md): maps member contributions to
  code areas and their evidence boundaries.

## Member code references

- [Leon implementation guide](LEON_IMPLEMENTATION_GUIDE.md): entrance access,
  schedule, RFID, display, keypad, dashboard, and integration.
- [Trevor implementation guide](TREVOR_GUIDE.md): room control, protocol,
  FreeRTOS synchronization, storage handoff, and diagnostics.

Source folders also contain short README files describing each module.
Portable host-check instructions are in [`../test/README`](../test/README).

The lecturer accepts a GitHub or Google Drive link for the submission; a
published Wokwi link is optional supporting evidence. Confirm each member's
identification details and review the remaining checks in the completion plan
before submitting.
