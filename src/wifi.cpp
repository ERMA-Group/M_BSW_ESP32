/**
 * @file wifi.cpp
 * @brief C++ class implementation
 */

#include "wifi.hpp"
#include "esp_netif.h"
#include "esp_wifi_default.h"
#include "nvs_flash.h"

namespace bsw {

EventGroupHandle_t Wifi::s_wifi_event_group;

Wifi::Wifi()
{
}

void Wifi::connect(const char* ssid, const char* password)
{
    nvs_flash_init();
    s_wifi_event_group = xEventGroupCreate();
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, NULL);

    wifi_config_t wifi_config = {};
    strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char*)wifi_config.sta.password, password, sizeof(wifi_config.sta.password));

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();

    // Wait for connection
    xEventGroupWaitBits(s_wifi_event_group, CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
    ESP_LOGI("WIFI", "Connected to %s", ssid);
}

} // namespace bsw