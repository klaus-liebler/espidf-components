idf_component_register(SRCS "megacli.cc" "sshd.cc"
                    INCLUDE_DIRS "include"
                    REQUIRES  "console" "esp_hw_support" "app_update" "i2c" "libssh")
