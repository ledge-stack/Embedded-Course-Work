# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

idf_component_register(SRCS "main.c" INCLUDE_DIRS "../include"
    REQUIRES entrance rooms comms storage cli auth common trevor actuator nvs_flash)
