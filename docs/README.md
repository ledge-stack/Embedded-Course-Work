# Documentation guide

Start with the root [README](../README.md) for the project summary, build
command, demo credentials, and current high-level scope. This page points to
the detailed material by task.

## Build, run, and explain the system

- [Presentation demo](PRESENTATION_DEMO.md): how to start the one-board Wokwi
  demo and steps for showing RFID, schedule, room, CLI, and storage features.
- [System presentation guide](SYSTEM_PRESENTATION_GUIDE.md): end-to-end system
  explanation, folder purposes, GPIO wiring, FreeRTOS tasks, queues, mutexes,
  logs, and module interactions.
- [System operation report](SYSTEM_OPERATION_REPORT.md): coursework report with
  architecture, hardware, operation, implementation areas, member details,
  and verification limits.
- [Interface and pin contract](INTERFACES.md): shared data ownership, APIs,
  access rules, GPIO assignments, and SPI/I2C/UART connections.
- [Coursework completion plan](COURSEWORK_COMPLETION_PLAN.md): implemented
  work, evidence collected, known limits, and checks still required.

## Ownership and individual contributions

- [Component ownership](COMPONENT_OWNERSHIP.md): member-to-folder mapping and
  implementation/evidence boundaries.
- [Leon implementation guide](LEON_IMPLEMENTATION_GUIDE.md): entrance code,
  schedule, RFID, display, keypad, dashboard, and integration.
- [Leon work summary](LEON_WORK.md): concise contribution record.
- [Leon report section](LEON_REPORT_SECTION.md): draft text for the coursework
  report; the author should review it before submission.
- [Entrance safety design](LEON_ENTRANCE_SAFETY_DESIGN.md): anti-passback,
  uncertainty, simulated tailgate marker, roll-call, and limits.
- [Trevor implementation guide](TREVOR_GUIDE.md): room control, protocol,
  FreeRTOS ownership, retries, and verification steps.
- [Trevor design](TREVOR_DESIGN.md): architecture and one-chip simulation
  boundary.
- [Original implementation plan](IMPLEMENTATION_PLAN.md): historical issue
  order and branch/review expectations; not the current status checklist.
- [Trevor verification record](TREVOR_VERIFICATION.md): historical evidence
  from the original subsystem checks.

Short README files beside source folders identify the responsible member and
the purpose of each module. The old Trevor implementation checklist was
removed because its completed steps duplicated the source tree and current
completion record.

## Verification and remaining work

The completion plan is the current status source. The Trevor verification
record documents earlier subsystem-specific checks; it is historical evidence,
not a replacement for current end-to-end acceptance. A local build or host test
does not prove a public Wokwi run, physical multi-board wiring, reboot recovery,
or submission completeness.

Portable C test commands and the Windows adapter-test command are in
[`../test/README`](../test/README). The test runner uses Trevor's current source
path under `components/trevor/`.

The lecturer accepts a Google Drive or GitHub link for submission; a published
Wokwi link is optional supporting evidence. Before submission, complete the
remaining checks listed in the completion plan, review each member's
contribution statement, and include the report, videos, and project files in
the Drive/GitHub destination.
