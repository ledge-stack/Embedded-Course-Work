idf_component_register(
    SRCS "core.c" "mfrc522.c" "runtime.c"
    INCLUDE_DIRS "../../include"
    REQUIRES common trevor esp_driver_gpio esp_driver_spi esp_event esp_netif
             esp_timer esp_wifi lwip
)
