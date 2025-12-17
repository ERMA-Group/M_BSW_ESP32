# M_BSW_ESP32/CMakeLists.txt
idf_component_register(
    SRCS 
    "src/core_task.cpp"
    "src/gpio_controller.cpp"
    "src/gpio.cpp"
    "src/i2c.cpp"
    "src/scheduler_task.cpp"
    "src/scheduler.cpp"
    "src/spi.cpp"
    "src/uart.cpp"
    "src/watchdog.cpp"
    INCLUDE_DIRS "inc"
    REQUIRES driver freertos esp_timer
)