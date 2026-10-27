# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

idf_component_register(
    SRCS "main.c" "trevor/core.c" "trevor/runtime.c"
    INCLUDE_DIRS "../include"
    REQUIRES esp_driver_gpio esp_driver_uart esp_timer nvs_flash
)
