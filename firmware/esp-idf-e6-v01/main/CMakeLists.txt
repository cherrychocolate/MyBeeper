idf_component_register(SRCS "main.c" "e6_hal.c" INCLUDE_DIRS "."
    REQUIRES esp-epaper-v01 esp_driver_spi esp_driver_gpio freertos)
