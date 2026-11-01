idf_component_register(SRCS "runtime.c" INCLUDE_DIRS "../../include"
    REQUIRES common trevor esp_driver_gpio esp_driver_spi fatfs sdmmc nvs_flash)
