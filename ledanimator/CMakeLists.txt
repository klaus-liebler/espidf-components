idf_component_register(SRCS "led_animator.cc"
    INCLUDE_DIRS "."
    REQUIRES "esp_driver_gpio" "common" "esp_timer"
    )