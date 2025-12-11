# M_BSW_ESP32/CMakeLists.txt
idf_component_register(
    SRCS "src/gpio.cpp" "src/gpioController.cpp" "src/core_task.cpp" "src/scheduler.cpp"
    INCLUDE_DIRS "inc"
    REQUIRES driver freertos esp_timer
)