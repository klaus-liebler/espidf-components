idf_component_register(SRCS "vl53l0x.cc"
                    INCLUDE_DIRS "include"
                    REQUIRES "i2c" "esp_timer" "i2c_sensor")
