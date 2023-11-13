idf_component_register(
    SRCS "common-esp32.cc" "common.cc"
    INCLUDE_DIRS "include"
    REQUIRES driver nvs_flash esp_timer
    )

