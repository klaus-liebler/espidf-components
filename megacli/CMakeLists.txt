idf_component_register(SRCS "sshd.cc" "megacli.cc"
                    INCLUDE_DIRS "include"
                    REQUIRES "libssh" "console" "esp_hw_support" "app_update")
