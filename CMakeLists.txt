# M_BSW_ESP32/CMakeLists.txt
idf_component_register(
    SRCS "src/gpio.cpp" "src/gpio_controller.cpp" "src/core_task.cpp" "src/scheduler.cpp"
         "src/i2c.cpp" "src/spi.cpp" "src/uart.cpp"
    INCLUDE_DIRS "inc"
    REQUIRES driver freertos esp_timer
)