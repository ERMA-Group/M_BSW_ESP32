# M_BSW_ESP32/CMakeLists.txt
idf_component_register(
    SRCS "src/gpio.cpp" "src/gpioController.cpp"
    INCLUDE_DIRS "inc"
    REQUIRES driver M_BSW_ESP32_Cfg
)