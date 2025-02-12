idf_component_register(SRCS "led_animator.cc"
    INCLUDE_DIRS "."
    REQUIRES "driver" "common" "esp_timer"
    )