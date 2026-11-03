# Shared system contracts

Shared users, access events and schedules are defined in
`include/common/system.h`. Keep one owner for each live state value: Leon owns
building occupancy and access schedule; Trevor owns room state; Edward owns
persistent files and the registered roster; Grace owns the separate door indicators.
