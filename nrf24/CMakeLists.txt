idf_component_register(SRCS "nrf24.cc"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_driver_spi esp_driver_gpio errorcodes
                       )