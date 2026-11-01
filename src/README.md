# Shared application entry point

`main.c` performs startup in this order: NVS, SD storage, Trevor room runtime,
Grace feedback output, Leon's entrance runtime, the occupancy bridge, and
Edward's UART0 command task. This startup composition is shared integration
work; member modules stay in their owner-labelled component folders.
