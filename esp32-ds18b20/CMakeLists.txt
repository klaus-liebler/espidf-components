idf_component_register(SRCS "ds18b20.c"
                       INCLUDE_DIRS "include"
                       REQUIRES driver esp_timer esp32-owb
                       )


