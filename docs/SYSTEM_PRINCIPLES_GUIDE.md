# System principles — simplified revision

FreeRTOS tasks separate room sensing, reader polling, clock, LCD, keypad, door timing and storage. Queues copy events to one SD writer. Mutexes protect shared state and files. Notifications return an important write result. SPI connects RFID/SD, I2C connects the LCD, UART0 provides management, and Wi-Fi supplies SNTP/HTTP. No custom room wire protocol remains.

Read [current report](SYSTEM_OPERATION_REPORT.md), [pin/API contract](INTERFACES.md),
[Trevor guide](TREVOR_GUIDE.md), and [presentation sequence](PRESENTATION_DEMO.md).
The earlier expanded guides are preserved in the pre-simplification archive.
