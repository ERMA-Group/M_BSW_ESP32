/**
 * @file wifi.cpp
 * @brief C++ class implementation
 */

#include "wifi.hpp"

#include <cstdlib>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <string>

#include "esp_netif.h"
#include "esp_wifi_default.h"
#include "esp_mac.h"
#include "esp_random.h"
#include "esp_system.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "nvram.hpp"

namespace bsw {

EventGroupHandle_t Wifi::s_wifi_event_group;

Wifi::Wifi()
    : initialized_(false),
      got_ip_(false),
      config_{},
    ap_ip_("192.168.4.1"),
      admin_credentials_context_(nullptr),
      get_admin_credentials_fn_(nullptr),
      set_admin_credentials_fn_(nullptr),
      http_server_(nullptr),
      portal_submitted_(false)
{
}

void Wifi::set_admin_credentials_callbacks(const AdminCredentialsCallbacks& callbacks)
{
    admin_credentials_context_ = callbacks.context;
    get_admin_credentials_fn_ = callbacks.get;
    set_admin_credentials_fn_ = callbacks.set;
}

void Wifi::initialize()
{
    if (initialized_)
    {
        return;
    }

    ESP_ERROR_CHECK(Nvram::system_init());
    ESP_ERROR_CHECK(esp_netif_init());

    esp_err_t loop_err = esp_event_loop_create_default();
    if (loop_err != ESP_OK && loop_err != ESP_ERR_INVALID_STATE)
    {
        ESP_ERROR_CHECK(loop_err);
    }

    s_wifi_event_group = xEventGroupCreate();
    if (s_wifi_event_group == nullptr)
    {
        ESP_LOGE(kTag, "Failed to create Wi-Fi event group");
        return;
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Keep both default netifs available so legacy STA-only usage and provisioning AP usage
    // can coexist across products using this same BSW component.
    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, this, nullptr));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, this, nullptr));

    initialized_ = true;
}

void Wifi::event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    auto* self = static_cast<Wifi*>(arg);
    if (self == nullptr)
    {
        return;
    }

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        self->got_ip_ = false;
        xEventGroupClearBits(s_wifi_event_group, CONNECTED_BIT);
        xEventGroupSetBits(s_wifi_event_group, FAIL_BIT);
        esp_wifi_connect();
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        (void)event_data;
        self->got_ip_ = true;
        xEventGroupClearBits(s_wifi_event_group, FAIL_BIT);
        xEventGroupSetBits(s_wifi_event_group, CONNECTED_BIT);
    }
}

bool Wifi::save_config_to_nvs()
{
    return save_wifi_to_nvs(config_.wifi_ssid, config_.wifi_password);
}

bool Wifi::save_wifi_to_nvs(const std::string& ssid, const std::string& password)
{
    Nvram nvs{kNs};
    if (nvs.open() != ESP_OK)
    {
        return false;
    }

    bool ok = nvs.set_string(kKeySsid, ssid) == ESP_OK;
    ok = ok && (nvs.set_string(kKeyPass, password) == ESP_OK);
    ok = ok && (nvs.set_string(kKeyApPass, config_.ap_password) == ESP_OK);
    nvs.close();

    if (ok)
    {
        config_.wifi_ssid = ssid;
        config_.wifi_password = password;
    }
    return ok;
}

bool Wifi::load_config_from_nvs()
{
    Nvram nvs{kNs};
    if (nvs.open() != ESP_OK)
    {
        return false;
    }

    config_.wifi_ssid = nvs.get_string(kKeySsid);
    config_.wifi_password = nvs.get_string(kKeyPass);
    config_.ap_password = nvs.get_string(kKeyApPass);
    nvs.close();

    return true;
}

std::string Wifi::generate_ap_password() const
{
    static constexpr char charset[] = "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnpqrstuvwxyz23456789";
    constexpr size_t kCharsetLen = sizeof(charset) - 1;

    char pw[13] = {0};
    for (size_t i = 0; i < 12; ++i)
    {
        pw[i] = charset[esp_random() % kCharsetLen];
    }
    return std::string(pw);
}

std::string Wifi::generate_ap_ssid() const
{
    if (admin_credentials_context_ != nullptr && get_admin_credentials_fn_ != nullptr)
    {
        AdminCredentials creds{};
        if (get_admin_credentials_fn_(admin_credentials_context_, creds) && !creds.device_id.empty())
        {
            return "Irrigation-" + creds.device_id;
        }
    }

    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char id_buf[16] = {0};
    std::snprintf(id_buf, sizeof(id_buf), "ESP-%02X%02X%02X", mac[3], mac[4], mac[5]);
    return "Irrigation-" + std::string(id_buf);
}

bool Wifi::ensure_ap_password()
{
    bool changed = false;
    if (config_.ap_password.size() < 8)
    {
        config_.ap_password = generate_ap_password();
        changed = true;
    }
    if (changed)
    {
        return save_wifi_to_nvs(config_.wifi_ssid, config_.wifi_password);
    }
    return true;
}

bool Wifi::get_ap_password(std::string& out_password)
{
    initialize();
    if (!load_config_from_nvs())
    {
        return false;
    }
    if (!ensure_ap_password())
    {
        return false;
    }
    out_password = config_.ap_password;
    return true;
}

bool Wifi::reset_ap_password(std::string& out_password)
{
    initialize();
    if (!load_config_from_nvs())
    {
        return false;
    }
    config_.ap_password = generate_ap_password();

    if (!save_wifi_to_nvs(config_.wifi_ssid, config_.wifi_password))
    {
        return false;
    }

    out_password = config_.ap_password;
    return true;
}

bool Wifi::has_wifi_credentials()
{
    initialize();
    load_config_from_nvs();
    ensure_ap_password();
    return !config_.wifi_ssid.empty();
}

void Wifi::clear_wifi_credentials()
{
    initialize();
    load_config_from_nvs();
    ensure_ap_password();

    config_.wifi_ssid.clear();
    config_.wifi_password.clear();

    if (!save_wifi_to_nvs("", ""))
    {
        ESP_LOGE(kTag, "Failed to clear Wi-Fi credentials in NVS");
    }
}

bool Wifi::start_sta()
{
    esp_err_t err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK)
    {
        ESP_LOGE(kTag, "Failed to set STA mode: %s", esp_err_to_name(err));
        return false;
    }

    wifi_config_t wifi_config = {};
    std::strncpy(reinterpret_cast<char*>(wifi_config.sta.ssid),
                 config_.wifi_ssid.c_str(),
                 sizeof(wifi_config.sta.ssid) - 1);
    std::strncpy(reinterpret_cast<char*>(wifi_config.sta.password),
                 config_.wifi_password.c_str(),
                 sizeof(wifi_config.sta.password) - 1);

    err = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (err != ESP_OK)
    {
        ESP_LOGE(kTag, "Failed to set STA config: %s", esp_err_to_name(err));
        return false;
    }

    err = esp_wifi_start();
    if (err != ESP_OK && err != ESP_ERR_WIFI_CONN)
    {
        ESP_LOGE(kTag, "Failed to start Wi-Fi STA: %s", esp_err_to_name(err));
        return false;
    }

    return true;
}

bool Wifi::start_ap()
{
    esp_err_t err = esp_wifi_set_mode(WIFI_MODE_AP);
    if (err != ESP_OK)
    {
        ESP_LOGE(kTag, "Failed to set AP mode: %s", esp_err_to_name(err));
        return false;
    }

    std::string ap_ssid = generate_ap_ssid();
    if (ap_ssid.size() > 31)
    {
        ap_ssid.resize(31);
    }

    std::string ap_password = config_.ap_password;
    if (ap_password.size() < 8)
    {
        ap_password = generate_ap_password();
        config_.ap_password = ap_password;
        save_config_to_nvs();
    }

    wifi_config_t ap_config = {};
    std::strncpy(reinterpret_cast<char*>(ap_config.ap.ssid), ap_ssid.c_str(), sizeof(ap_config.ap.ssid) - 1);
    ap_config.ap.ssid_len = ap_ssid.size();
    std::strncpy(reinterpret_cast<char*>(ap_config.ap.password), ap_password.c_str(), sizeof(ap_config.ap.password) - 1);
    ap_config.ap.max_connection = 4;
    ap_config.ap.channel = 1;
    ap_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;

    err = esp_wifi_set_config(WIFI_IF_AP, &ap_config);
    if (err != ESP_OK)
    {
        ESP_LOGE(kTag, "Failed to set AP config: %s", esp_err_to_name(err));
        return false;
    }

    err = esp_wifi_start();
    if (err != ESP_OK)
    {
        ESP_LOGE(kTag, "Failed to start Wi-Fi AP: %s", esp_err_to_name(err));
        return false;
    }

    esp_netif_t* ap_netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (ap_netif != nullptr)
    {
        esp_netif_ip_info_t ip_info{};
        if (esp_netif_get_ip_info(ap_netif, &ip_info) == ESP_OK)
        {
            char ip_buf[16] = {0};
            std::snprintf(ip_buf,
                          sizeof(ip_buf),
                          "%" PRIu8 ".%" PRIu8 ".%" PRIu8 ".%" PRIu8,
                          IP2STR(&ip_info.ip));
            ap_ip_ = ip_buf;
        }
    }

    ESP_LOGI(kTag, "SoftAP started: %s", ap_ssid.c_str());
    ESP_LOGI(kTag, "Portal URL: http://%s/", ap_ip_.c_str());
    ESP_LOGI(kTag, "AP Password: %s", config_.ap_password.c_str());
    return true;
}

bool Wifi::connect(const char* ssid, const char* password, uint8_t max_attempts)
{
    initialize();
    load_config_from_nvs();
    ensure_ap_password();

    config_.wifi_ssid = ssid != nullptr ? ssid : "";
    config_.wifi_password = password != nullptr ? password : "";

    if (config_.wifi_ssid.empty())
    {
        ESP_LOGW(kTag, "Wi-Fi SSID is empty");
        return false;
    }

    for (uint8_t attempt = 1; attempt <= max_attempts; ++attempt)
    {
        got_ip_ = false;
        xEventGroupClearBits(s_wifi_event_group, CONNECTED_BIT | FAIL_BIT);

        if (!start_sta())
        {
            continue;
        }

        esp_err_t conn_err = esp_wifi_connect();
        if (conn_err != ESP_OK)
        {
            ESP_LOGW(kTag, "esp_wifi_connect failed on attempt %u: %s", attempt, esp_err_to_name(conn_err));
            esp_wifi_stop();
            continue;
        }

        EventBits_t bits = xEventGroupWaitBits(
            s_wifi_event_group,
            CONNECTED_BIT,
            pdTRUE,
            pdTRUE,
            pdMS_TO_TICKS(kConnectTimeoutMs));

        if (bits & CONNECTED_BIT)
        {
            ESP_LOGI(kTag, "Connected to %s", config_.wifi_ssid.c_str());
            return true;
        }

        ESP_LOGW(kTag, "Wi-Fi connection attempt %u/%u failed", attempt, max_attempts);
        esp_wifi_stop();
        vTaskDelay(pdMS_TO_TICKS(300));
    }

    return false;
}

bool Wifi::connect_from_nvram(uint8_t max_attempts)
{
    initialize();
    load_config_from_nvs();
    ensure_ap_password();

    if (config_.wifi_ssid.empty())
    {
        ESP_LOGW(kTag, "No Wi-Fi credentials in NVS");
        return false;
    }

    const std::string ssid = config_.wifi_ssid;
    const std::string password = config_.wifi_password;
    return connect(ssid.c_str(), password.c_str(), max_attempts);
}

std::string Wifi::url_decode(const std::string& in)
{
    std::string out;
    out.reserve(in.size());

    for (size_t i = 0; i < in.size(); ++i)
    {
        if (in[i] == '+')
        {
            out.push_back(' ');
        }
        else if (in[i] == '%' && i + 2 < in.size())
        {
            char hex[3] = {in[i + 1], in[i + 2], '\0'};
            char* end = nullptr;
            long val = std::strtol(hex, &end, 16);
            if (end != nullptr && *end == '\0')
            {
                out.push_back(static_cast<char>(val));
                i += 2;
            }
            else
            {
                out.push_back(in[i]);
            }
        }
        else
        {
            out.push_back(in[i]);
        }
    }

    return out;
}

std::string Wifi::get_form_value(const std::string& body, const char* key)
{
    const std::string token = std::string(key) + "=";
    size_t start = body.find(token);
    if (start == std::string::npos)
    {
        return "";
    }

    start += token.size();
    size_t end = body.find('&', start);
    if (end == std::string::npos)
    {
        end = body.size();
    }

    return url_decode(body.substr(start, end - start));
}

std::string Wifi::html_escape(const std::string& in)
{
    std::string out;
    out.reserve(in.size());
    for (char c : in)
    {
        if (c == '&') out += "&amp;";
        else if (c == '<') out += "&lt;";
        else if (c == '>') out += "&gt;";
        else if (c == '\"') out += "&quot;";
        else out.push_back(c);
    }
    return out;
}

bool Wifi::get_query_value(httpd_req_t* req, const char* key, std::string& out)
{
    out.clear();
    const size_t query_len = httpd_req_get_url_query_len(req);
    if (query_len == 0 || query_len > 255)
    {
        return false;
    }

    char query[256] = {0};
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK)
    {
        return false;
    }

    char value[128] = {0};
    if (httpd_query_key_value(query, key, value, sizeof(value)) != ESP_OK)
    {
        return false;
    }

    out = url_decode(value);
    return true;
}

esp_err_t Wifi::root_get_handler(httpd_req_t* req)
{
    auto* self = static_cast<Wifi*>(req->user_ctx);
    if (self == nullptr)
    {
        return ESP_FAIL;
    }

    std::string html;
    html.reserve(2000);
    html += "<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>";
    html += "<title>ESP32 WiFi Setup</title><style>body{font-family:Arial,sans-serif;padding:24px;max-width:540px;margin:auto;background:#f0f4f8;}";
    html += "h2{margin-bottom:4px;}small{color:#475569;}label{display:block;margin-top:14px;font-weight:600;}input{width:100%;padding:10px;border-radius:8px;border:1px solid #94a3b8;}";
    html += "button{margin-top:18px;padding:11px 14px;border:0;border-radius:10px;background:#0f766e;color:#fff;font-weight:700;cursor:pointer;width:100%;}";
    html += ".card{background:#fff;border-radius:14px;padding:20px;box-shadow:0 6px 24px rgba(2,6,23,0.08);} .hint{margin-top:10px;font-size:12px;color:#334155;}";
    html += "</style></head><body>";
    html += "<div class='card'><h1>ERMA Group</h1><h2>Water Irrigation Controller</h2><small>Configure Wi-Fi and device admin identity</small>";
    html += "<form method='POST' action='/save'>";
    html += "<label>Wi-Fi SSID</label><input name='ssid' required value='" + html_escape(self->config_.wifi_ssid) + "'>";
    html += "<label>Wi-Fi Password</label><input name='password' type='password' value='" + html_escape(self->config_.wifi_password) + "'>";
    html += "<button type='submit'>Save and Restart</button></form>";
    html += "<p class='hint'>Only Wi-Fi credentials can be changed from this page.</p></div></body></html>";

    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, html.c_str(), html.size());
}

esp_err_t Wifi::save_post_handler(httpd_req_t* req)
{
    auto* self = static_cast<Wifi*>(req->user_ctx);
    if (self == nullptr)
    {
        return ESP_FAIL;
    }

    const int total_len = req->content_len;
    if (total_len <= 0 || total_len > 1024)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid body");
        return ESP_FAIL;
    }

    std::string body;
    body.resize(total_len);
    int received = 0;
    while (received < total_len)
    {
        int r = httpd_req_recv(req, body.data() + received, total_len - received);
        if (r <= 0)
        {
            if (r == HTTPD_SOCK_ERR_TIMEOUT)
            {
                continue;
            }
            return ESP_FAIL;
        }
        received += r;
    }

    const std::string ssid = get_form_value(body, "ssid");
    const std::string pass = get_form_value(body, "password");

    if (ssid.empty())
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "SSID is required");
        return ESP_FAIL;
    }

    self->config_.wifi_ssid = ssid;
    self->config_.wifi_password = pass;

    if (!self->save_config_to_nvs())
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to save config");
        return ESP_FAIL;
    }

    self->portal_submitted_ = true;
    const char* response = "Saved. Device will restart and connect to configured Wi-Fi.";
    httpd_resp_set_type(req, "text/plain");
    return httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
}

esp_err_t Wifi::captive_redirect_handler(httpd_req_t* req)
{
    auto* self = static_cast<Wifi*>(req->user_ctx);
    if (self == nullptr)
    {
        return ESP_FAIL;
    }

    const std::string location = "http://" + self->ap_ip_ + "/";
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", location.c_str());
    httpd_resp_set_type(req, "text/plain");
    return httpd_resp_send(req, "Redirecting to setup portal", HTTPD_RESP_USE_STRLEN);
}

void Wifi::stop_http_server()
{
    if (http_server_ != nullptr)
    {
        httpd_stop(http_server_);
        http_server_ = nullptr;
    }
}

void Wifi::start_provisioning_portal_blocking()
{
    initialize();
    load_config_from_nvs();
    ensure_ap_password();

    esp_wifi_stop();
    if (!start_ap())
    {
        ESP_LOGE(kTag, "Failed to start provisioning AP");
        return;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.uri_match_fn = httpd_uri_match_wildcard;

    esp_err_t err = httpd_start(&http_server_, &config);
    if (err != ESP_OK)
    {
        ESP_LOGE(kTag, "Failed to start HTTP server: %s", esp_err_to_name(err));
        return;
    }

    httpd_uri_t root_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = &root_get_handler,
        .user_ctx = this,
    };

    httpd_uri_t save_uri = {
        .uri = "/save",
        .method = HTTP_POST,
        .handler = &save_post_handler,
        .user_ctx = this,
    };

    httpd_uri_t captive_android_204 = {
        .uri = "/generate_204",
        .method = HTTP_GET,
        .handler = &captive_redirect_handler,
        .user_ctx = this,
    };

    httpd_uri_t captive_android_gen204 = {
        .uri = "/gen_204",
        .method = HTTP_GET,
        .handler = &captive_redirect_handler,
        .user_ctx = this,
    };

    httpd_uri_t captive_apple = {
        .uri = "/hotspot-detect.html",
        .method = HTTP_GET,
        .handler = &captive_redirect_handler,
        .user_ctx = this,
    };

    httpd_uri_t captive_windows = {
        .uri = "/connecttest.txt",
        .method = HTTP_GET,
        .handler = &captive_redirect_handler,
        .user_ctx = this,
    };

    httpd_uri_t captive_msft = {
        .uri = "/ncsi.txt",
        .method = HTTP_GET,
        .handler = &captive_redirect_handler,
        .user_ctx = this,
    };

    httpd_uri_t wildcard_uri = {
        .uri = "/*",
        .method = HTTP_GET,
        .handler = &captive_redirect_handler,
        .user_ctx = this,
    };

    httpd_register_uri_handler(http_server_, &root_uri);
    httpd_register_uri_handler(http_server_, &save_uri);
    httpd_register_uri_handler(http_server_, &captive_android_204);
    httpd_register_uri_handler(http_server_, &captive_android_gen204);
    httpd_register_uri_handler(http_server_, &captive_apple);
    httpd_register_uri_handler(http_server_, &captive_windows);
    httpd_register_uri_handler(http_server_, &captive_msft);
    httpd_register_uri_handler(http_server_, &wildcard_uri);

    portal_submitted_ = false;
    ESP_LOGI(kTag, "Provisioning portal running on SoftAP");

    while (!portal_submitted_)
    {
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    stop_http_server();
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
}

} // namespace bsw