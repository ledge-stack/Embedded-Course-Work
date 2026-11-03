# Trevor's presentation guide — simplified version

Read core.c (room timing/usage) then runtime.c (tasks, snapshots and event queue).

Suggested explanation:

> Each room has its own FreeRTOS task. It reads the PIR every 20 ms and controls
> its own light. Motion turns the requested light on; a five-second delay turns
> it off after motion stops. Renewed motion restarts the delay.
>
> The task copies its latest state into a mutex-protected snapshot and sends
> changed states to one event queue. One worker writes all SD events. A full
> queue or failed write is counted without stopping local lights. Access and
> schedule requests can wait for their own write-completion notification.
>
> The entrance count is a separate input. Known zero suppresses lights; an
> uncertain count does not. The usage command reports on-time and suppressed
> time since boot, not kWh.

Show both PIRs, renewed motion, delayed off, rooms/usage and the confirmed-empty
override. Explain task, queue, mutex and task notification. The former CRC/ACK,
retry, UART roles and link switch are removed. Use your own description of the
code you actually understand and the demonstrations you personally performed.
