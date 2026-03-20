/**
 * @file wifi.hpp
 * @brief C++ class definition for embedded module.
 *
 * This header defines the internal C++ class used by the module.
 * It is not exposed to C projects directly — only via the C facade.
 */

#pragma once
#include <cstdint>
#include <string>
#include <functional>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "freertos/event_groups.h"

#include "esp_http_server.h"

extern "C" {

}

namespace bsw {

class Wifi {
public:
    Wifi();
    ~Wifi() = default;

    void initialize();
    bool connect(const char* ssid, const char* password, uint8_t max_attempts = 3);
    bool connect_from_nvram(uint8_t max_attempts = 3);

    bool has_wifi_credentials();
    bool is_connected() const;
    void clear_wifi_credentials();
    void set_pairing_pin_callback(const std::function<void(const std::string&)>& callback);
    void start_provisioning_portal_blocking();
    bool get_ap_password(std::string& out_password);
    bool reset_ap_password(std::string& out_password);

private:
    struct Config {
        std::string wifi_ssid;
        std::string wifi_password;
        std::string ap_password;
    };

    static EventGroupHandle_t s_wifi_event_group;
    static constexpr int CONNECTED_BIT = BIT0;
    static constexpr int FAIL_BIT = BIT1;

    static constexpr const char* kNs = "wifi_cfg";
    static constexpr const char* kKeySsid = "ssid";
    static constexpr const char* kKeyPass = "pass";
    static constexpr const char* kKeyApPass = "ap_pass";

    static constexpr const char* kTag = "WIFI";
    static constexpr uint32_t kConnectTimeoutMs = 15000;

    bool initialized_;
    bool got_ip_;
    Config config_;
    std::string ap_ip_;
    httpd_handle_t http_server_;
    bool portal_submitted_;
    std::function<void(const std::string&)> pairing_pin_callback_;

    static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
    static esp_err_t root_get_handler(httpd_req_t* req);
    static esp_err_t save_post_handler(httpd_req_t* req);
    static esp_err_t captive_redirect_handler(httpd_req_t* req);

    static std::string url_decode(const std::string& in);
    static std::string get_form_value(const std::string& body, const char* key);
    static std::string html_escape(const std::string& in);
    static bool get_query_value(httpd_req_t* req, const char* key, std::string& out);

    bool load_config_from_nvs();
    bool save_config_to_nvs();
    bool save_wifi_to_nvs(const std::string& ssid, const std::string& password);
    bool ensure_ap_password();
    std::string generate_ap_password() const;
    std::string generate_ap_ssid() const;

    bool start_sta();
    bool start_ap();
    void stop_http_server();

};

} // namespace bsw