idf_component_register(SRCS "buzzer.cc"
                       INCLUDE_DIRS "."
                       REQUIRES "esp_driver_ledc" "esp_driver_gpio" "esp_timer"
                       )

