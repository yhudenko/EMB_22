set(EAF_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../submodules/eaf")

idf_component_register(
    SRCS
        "${EAF_ROOT}/src/esp_adapter/esp_interrupt.cpp"
        "${EAF_ROOT}/src/esp_adapter/esp_logger.cpp"
        "${EAF_ROOT}/src/esp_adapter/esp_pin.cpp"
        "${EAF_ROOT}/src/esp_adapter/esp_task.cpp"
        "${EAF_ROOT}/src/esp_adapter/esp_time.cpp"
        "${EAF_ROOT}/src/esp_adapter/esp_timer.cpp"
        "${EAF_ROOT}/src/esp_adapter/esp_timer_builder.cpp"
        "${EAF_ROOT}/src/esp_adapter/esp_utils.cpp"
    INCLUDE_DIRS
        "${EAF_ROOT}/include"
    PRIV_REQUIRES
        esp_driver_gpio
        esp_driver_gptimer
        freertos
        log
)
