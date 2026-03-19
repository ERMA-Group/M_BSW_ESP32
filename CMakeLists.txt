# M_BSW_ESP32/CMakeLists.txt
idf_component_register(
    SRCS 
    "src/core_task.cpp"
    "src/gpio_controller.cpp"
    "src/gpio.cpp"
    "src/i2c.cpp"
    "src/nvram.cpp"
    "src/ota.cpp"
    "src/scheduler_task.cpp"
    "src/scheduler.cpp"
    "src/spi.cpp"
    "src/uart.cpp"
    "src/watchdog.cpp"
    "src/wifi.cpp"
    INCLUDE_DIRS "inc"
    REQUIRES driver 
    freertos 
    esp_timer 
    esp_wifi 
    esp_event 
    esp_netif
    nvs_flash 
    app_update
    esp_http_server
    esp_http_client
    #EMBED_TXTFILES "erma_pubkey.pem"
)