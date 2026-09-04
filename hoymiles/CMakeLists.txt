if(NOT DEFINED GENERATED_DIR)
    message(ERROR "NOT DEFINED GENERATED_DIR!!!")
endif()

idf_component_register(
    SRCS "hoymiles_protocol.cc" "hoymiles_manager.cc"
    INCLUDE_DIRS "include" "${GENERATED_DIR}"
    REQUIRES nrf24 espidf-component-webmanager esp_driver_gpio esp_timer errorcodes
)
