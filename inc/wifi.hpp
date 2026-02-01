/**
 * @file wifi.hpp
 * @brief C++ class definition for embedded module.
 *
 * This header defines the internal C++ class used by the module.
 * It is not exposed to C projects directly — only via the C facade.
 */

#pragma once
#include <cstdint>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "freertos/event_groups.h"

extern "C" {

}

namespace bsw {

class Wifi {
public:
    Wifi();
    ~Wifi() = default;

    void connect(const char* ssid, const char* password);

private:
    static EventGroupHandle_t s_wifi_event_group;
    static const int CONNECTED_BIT = BIT0;

    static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
    {
        if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
        {
            esp_wifi_connect();
        }
        else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
        {
            esp_wifi_connect(); // Auto-reconnect
            xEventGroupClearBits(s_wifi_event_group, CONNECTED_BIT);
        }
        else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
        {
            xEventGroupSetBits(s_wifi_event_group, CONNECTED_BIT);
        }
    }

};

} // namespace bsw