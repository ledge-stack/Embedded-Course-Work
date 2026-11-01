# Implementation Plan and GitHub Issue Backlog

This file records the original coursework implementation sequence and GitHub
issue backlog. It is a planning/reference document, not a current completion
checklist; use [`COURSEWORK_COMPLETION_PLAN.md`](COURSEWORK_COMPLETION_PLAN.md)
for the implemented state and outstanding evidence.
The lecturer's current submission instruction accepts a Google Drive or GitHub
link; older backlog references to a required Wokwi URL are superseded. A
published Wokwi project can still be included as optional supporting evidence.

## Milestones

| Milestone | Target dates | Outcome |
| --- | --- | --- |
| M0 - Foundation | 23-24 Oct 2026 | Private repo and team workflow, Wokwi circuit, module skeletons, agreed shared types and message format |
| M1 - Core features | 25-28 Oct 2026 | Entrance, room control, communication, synchronization, storage and authenticated serial management implemented separately |
| M2 - Integration and release | 29 Oct-2 Nov 2026 | Integrated simulation, innovation features, report, videos, contribution statements and submission |

## GitHub issues, in dependency order

Issues #1-18 have been created in GitHub in this dependency order. Each issue body records the phase, proposed owner, acceptance criteria and dependencies. Add area labels and assign owners once their GitHub usernames are known; the issue bodies remain the source of task detail.

### M0 - Foundation

1. **Agree shared system data types and module interfaces** - Leon, Trevor and Edward. Define user, access event, occupancy, schedule/configuration and inter-terminal message types. Record ownership, units, time representation, IDs and versioning in `docs/` before feature modules depend on them.
2. **Create the terminal simulation/circuit** - Leon coordinates; all contribute. Add the entrance and room hardware, record pin assignments, and provide the project files through the lecturer-approved Google Drive or GitHub link. A working Wokwi link is optional supporting evidence.
3. **Add module skeletons and build instructions** - Leon. Establish `entrance`, `rooms`, `comms`, `storage`, `cli`, `auth` and `common` boundaries in ESP-IDF, and keep a clean PlatformIO build.
4. **Choose and verify the terminal communication link** - Trevor. Select an approach that works in Wokwi, define message types/payloads/acknowledgements, and demonstrate a message between two terminals before relying on it for integration.

### M1 - Core features

5. **Implement entrance and exit RFID access control** - Leon. Read both RFID points, validate registered tags, actuate the correct door, report outcomes and reject unknown tags.
6. **Track building occupancy and prevent invalid transitions** - Leon. Record entry/exit events, block double entry and exit without a prior entry, and keep an accurate current inside list/count.
7. **Enforce a configurable access schedule** - Leon. Provide a reliable clock source, store the allowed start/end times, enforce the schedule at both entry and exit doors, and specify/log/alert on people still inside when the period closes.
8. **Implement room occupancy and automatic lighting** - Trevor. Add one occupancy input and light output per room, apply a configurable off-delay, and publish each room's occupancy/light state.
9. **Implement terminal messaging and reconnect behavior** - Trevor. Send room/access state to shared consumers; handle lost messages, duplicate delivery and reconnection without silently corrupting state.
10. **Synchronize shared state and concurrent writes** - Trevor. Use FreeRTOS queues and mutexes/semaphores for event processing, shared occupancy/configuration and serialized storage access. Document and demonstrate a concurrent update scenario.
11. **Persist events, users and configuration on SD** - Edward. Mount the card, store access/occupancy/lighting events, registered users and configuration, serialize writes with the shared synchronization design, and handle a missing or full card without a crash.
12. **Add serial status and record commands** - Edward. Implement a UART help menu and read commands for access records, people inside/count, room status, lights, system status and schedule.
13. **Add authenticated admin commands** - Edward. Require login for schedule/user changes; store a password/PIN verifier rather than plaintext credentials, log failures, rate-limit or lock out repeated failures, and support logout/session timeout.

### M2 - Integration and release

14. **Select and demonstrate innovation features** - The repository's proposed package, **Occupancy-Aware Safety and Lighting Analytics**, combines anti-passback alerts, simulated tailgate uncertainty, evacuation roll-call and after-hours reporting, grant/deny LED feedback, building-empty light shutoff, room-link recovery, and a serial room-light usage report. The usage report shows durations only; it does not claim measured energy. A real tailgate detector needs a passage sensor. The team should record this package as its selected innovation in the issue before submission. See [`LEON_ENTRANCE_SAFETY_DESIGN.md`](LEON_ENTRANCE_SAFETY_DESIGN.md).
15. **Integrate terminals and run end-to-end Wokwi scenarios** - Leon coordinates; all contribute. Demonstrate valid/invalid RFID, entry/exit state, schedule boundary, both room lights and timeout, serial queries/admin updates, storage failure handling, reconnect and concurrent events. For Leon's entrance safety design, demonstrate denied feedback, repeat-card anti-passback alert, a clearly labelled simulated tailgate event, and an evacuation report listing tracked occupants without clearing the inside state.
16. **Write the system operation report and collect individual contributions** - Leon coordinates. Include architecture, hardware, software design, implemented features, member details, each person's contributions, the Google Drive or GitHub submission link, and report sections from all members. Add a Wokwi link if the team publishes one.
17. **Record individual code walkthrough videos and prepare presentation** - Each member. Each person explains and demonstrates the code they personally implemented; collect links and confirm all three members can present their work.
18. **Freeze, build and submit coursework** - Leon coordinates. Resolve integration issues, perform a clean firmware build and complete the report/video submission on Muele by 2 November 2026.

## Branch and review policy

The work to protect `main` is tracked in [issue #19](https://github.com/ledge-stack/Embedded-Course-Work/issues/19). The rule must be applied and verified in GitHub repository settings before treating the branch as protected.

For every issue, create a feature branch, open a pull request against `main`, reference the issue, and request a teammate review. Protect `main` with a GitHub branch ruleset or classic branch protection that:

- requires a pull request before changes can reach `main`;
- requires at least one approving review;
- dismisses stale approvals when new commits are pushed;
- requires approval of the most recent reviewable push by someone other than its author;
- blocks force pushes and branch deletion.

Apply the rules to the exact `main` branch and verify the saved settings in GitHub. If GitHub does not allow an author to approve their own pull request, keep the teammate review requirement explicit. The local PR template reminds contributors of the process, but only the remote branch rules enforce it.

## Completion checks

Each feature issue should link to its implementation PR and include the Wokwi scenario or build evidence relevant to its behavior. Integration is complete only when the full system flow is demonstrated, not just when individual components compile. Keep the report, Drive/GitHub submission link, member details, individual contributions, and videos in the final release checklist. A Wokwi URL is optional.
