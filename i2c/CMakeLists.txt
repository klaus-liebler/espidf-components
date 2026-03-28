idf_component_register(SRCS "i2c.cc"
                       INCLUDE_DIRS "include"
                       REQUIRES "esp_driver_i2c" "errorcodes" "common")