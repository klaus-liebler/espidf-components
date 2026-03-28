idf_component_register(SRCS "FUSB302_UFP.c" "PD_UFP_Protocol.c" "PD_UFP.cpp"
                       INCLUDE_DIRS "."
                       REQUIRES i2c esp_timer esp_driver_i2c esp_driver_gpio
                       )


