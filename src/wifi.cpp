/**
 * @file wifi.cpp
 * @brief C++ class implementation
 */

#include "wifi.hpp"

#include <cstdlib>
#include <cctype>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <string>

#include "esp_netif.h"
#include "esp_wifi_default.h"
#include "esp_mac.h"
#include "esp_random.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "nvram.hpp"

namespace bsw {

namespace {

bool normalize_pairing_pin(std::string& pin)
{
    if (pin.size() != 6)
    {
        return false;
    }

    for (char& ch : pin)
    {
        const unsigned char u = static_cast<unsigned char>(ch);
        if (std::isalnum(u) == 0)
        {
            return false;
        }
        ch = static_cast<char>(std::toupper(u));
    }

    return true;
}

} // namespace

EventGroupHandle_t Wifi::s_wifi_event_group;

Wifi::Wifi()
    : initialized_(false),
      got_ip_(false),
      config_{},
    ap_ip_("192.168.4.1"),
      http_server_(nullptr),
            portal_submitted_(false)
{
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
    if (s_wifi_event_group == nullptr)
    {
        ESP_LOGE(kTag, "Wi-Fi event group is null in event handler");
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
    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char id_buf[16] = {0};
    std::snprintf(id_buf, sizeof(id_buf), "ERMA-%02X%02X%02X", mac[3], mac[4], mac[5]);
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

bool Wifi::is_connected() const
{
    return got_ip_;
}

bool Wifi::is_ap_active() const
{
    wifi_mode_t mode = WIFI_MODE_NULL;
    if (esp_wifi_get_mode(&mode) != ESP_OK)
    {
        return false;
    }

    return mode == WIFI_MODE_AP || mode == WIFI_MODE_APSTA;
}

bool Wifi::start_local_access_ap()
{
    initialize();
    load_config_from_nvs();
    ensure_ap_password();

    got_ip_ = false;
    esp_wifi_stop();
    return start_ap();
}

const std::string& Wifi::get_ssid() const
{
    return config_.wifi_ssid;
}

const std::string& Wifi::get_password() const
{
    return config_.wifi_password;
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

void Wifi::set_pairing_pin_callback(const std::function<void(const std::string&)>& callback)
{
    pairing_pin_callback_ = callback;
}

void Wifi::set_operating_mode_callback(const std::function<void(const std::string&)>& callback)
{
    operating_mode_callback_ = callback;
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
    if (!initialized_ || s_wifi_event_group == nullptr)
    {
        ESP_LOGE(kTag, "Wi-Fi connect aborted: initialization incomplete");
        return false;
    }
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
    if (!initialized_ || s_wifi_event_group == nullptr)
    {
        ESP_LOGE(kTag, "Wi-Fi connect_from_nvram aborted: initialization incomplete");
        return false;
    }
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
    html.reserve(16000);
    html += "<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>";
    html += "<title>ERMA Group Irrigation Controller</title><style>body{font-family:Arial,sans-serif;padding:24px;max-width:540px;margin:auto;background:#f0f4f8;}";
    html += "h2{color:#f74040;}h2{margin-bottom:4px;}small{color:#475569;}label{display:block;margin-top:14px;font-weight:600;}input{width:100%;padding:10px;border-radius:8px;border:1px solid #94a3b8;}";
    html += "button{margin-top:18px;padding:11px 14px;border:0;border-radius:10px;background:#f74040;color:#fff;font-weight:700;cursor:pointer;width:100%;}";
    html += ".card{background:#fff;border-radius:14px;padding:20px;box-shadow:0 6px 24px rgba(2,6,23,0.08);} .logo{display:block;width:100%;max-width:320px;height:auto;object-fit:contain;margin:0 0 10px 0;} .hint{margin-top:10px;font-size:12px;color:#334155;}";
    html += "</style></head><body>";
    html += "<div class='card'><img class='logo' alt='ERMA Group' src='data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAMwAAABICAYAAAC6Axo8AAAAGXRFWHRTb2Z0d2FyZQBBZG9iZSBJbWFnZVJlYWR5ccllPAAAA2ZpVFh0WE1MOmNvbS5hZG9iZS54bXAAAAAAADw/eHBhY2tldCBiZWdpbj0i77u/IiBpZD0iVzVNME1wQ2VoaUh6cmVTek5UY3prYzlkIj8+IDx4OnhtcG1ldGEgeG1sbnM6eD0iYWRvYmU6bnM6bWV0YS8iIHg6eG1wdGs9IkFkb2JlIFhNUCBDb3JlIDUuMy1jMDExIDY2LjE0NTY2MSwgMjAxMi8wMi8wNi0xNDo1NjoyNyAgICAgICAgIj4gPHJkZjpSREYgeG1sbnM6cmRmPSJodHRwOi8vd3d3LnczLm9yZy8xOTk5LzAyLzIyLXJkZi1zeW50YXgtbnMjIj4gPHJkZjpEZXNjcmlwdGlvbiByZGY6YWJvdXQ9IiIgeG1sbnM6eG1wTU09Imh0dHA6Ly9ucy5hZG9iZS5jb20veGFwLzEuMC9tbS8iIHhtbG5zOnN0UmVmPSJodHRwOi8vbnMuYWRvYmUuY29tL3hhcC8xLjAvc1R5cGUvUmVzb3VyY2VSZWYjIiB4bWxuczp4bXA9Imh0dHA6Ly9ucy5hZG9iZS5jb20veGFwLzEuMC8iIHhtcE1NOk9yaWdpbmFsRG9jdW1lbnRJRD0ieG1wLmRpZDpCOTYwQTAyNzRDODFFQTExQThGN0Q2QUM3NThGMzQ1RSIgeG1wTU06RG9jdW1lbnRJRD0ieG1wLmRpZDo3Q0Q1QUU1QkM2RjgxMUVEQUFGOEVFQTZEMzc1RjFFRCIgeG1wTU06SW5zdGFuY2VJRD0ieG1wLmlpZDo3Q0Q1QUU1QUM2RjgxMUVEQUFGOEVFQTZEMzc1RjFFRCIgeG1wOkNyZWF0b3JUb29sPSJBZG9iZSBQaG90b3Nob3AgQ1M2IChXaW5kb3dzKSI+IDx4bXBNTTpEZXJpdmVkRnJvbSBzdFJlZjppbnN0YW5jZUlEPSJ4bXAuaWlkOkZEM0IwMDRCQ0I5NkVBMTE4QjIyQkUyNDQyOEU0RTNFIiBzdFJlZjpkb2N1bWVudElEPSJ4bXAuZGlkOkI5NjBBMDI3NEM4MUVBMTFBOEY3RDZBQzc1OEYzNDVFIi8+IDwvcmRmOkRlc2NyaXB0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'><h2>Water Irrigation Controller</h2><small>Configure Wi-Fi and Pairing</small>";

    html += "<form method='POST' action='/save'>";
    html += "<label>Operating Mode</label><select id='operating_mode' name='operating_mode'><option value='cloud_ha' selected>Cloud+HA (default)</option><option value='home_assistant'>Home Assistant</option><option value='local_mode'>Local Mode (offline schedule + local web)</option></select>";
    html += "<div id='wifi_fields'>";
    html += "<label>Wi-Fi SSID</label><input id='ssid_input' name='ssid' required value='" + html_escape(self->config_.wifi_ssid) + "'>";
    html += "<label>Wi-Fi Password</label><input id='password_input' name='password' type='password' required value='" + html_escape(self->config_.wifi_password) + "'>";
    html += "</div>";
    html += "<div id='pin_fields'>";
    html += "<label>Pairing PIN</label><input id='pairing_pin_input' name='pairing_pin'>";
    html += "</div>";
    html += "<button type='submit'>Save and Restart</button></form>";
    html += "<p class='hint'>Cloud+HA mode: requires Wi-Fi + pairing PIN. Home Assistant mode: requires Wi-Fi, no cloud PIN. Local Mode: no Wi-Fi required; after reboot open <b>http://192.168.4.1/local_mode</b> while connected to ECU AP.</p>";
    html += "<script>(function(){var mode=document.getElementById('operating_mode');var pin=document.getElementById('pin_fields');var wifi=document.getElementById('wifi_fields');var pairingPin=document.getElementById('pairing_pin_input');var ssid=document.getElementById('ssid_input');var pass=document.getElementById('password_input');function update(){var local=(mode.value==='local_mode');var mqtt=(mode.value==='home_assistant'||local);pin.style.display=mqtt?'none':'block';wifi.style.display=local?'none':'block';if(pairingPin){pairingPin.required=!mqtt;}if(ssid){ssid.required=!local;}if(pass){pass.required=!local;}}mode.addEventListener('change',update);update();})();</script>";
    html += "</div></body></html>";

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
    const std::string operating_mode = get_form_value(body, "operating_mode");
    std::string pairing_pin = get_form_value(body, "pairing_pin");
    const bool local_mode = (operating_mode == "local_mode");
    const bool pure_mqtt = (operating_mode == "home_assistant" || operating_mode == "pure_mqtt" || local_mode);
    if (!local_mode && ssid.empty())
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "SSID is required");
        return ESP_FAIL;
    }

    if (!local_mode && pass.empty())
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Wi-Fi password is required");
        return ESP_FAIL;
    }

    if (!pure_mqtt && pairing_pin.empty())
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Pairing PIN is required");
        return ESP_FAIL;
    }

    if (!pure_mqtt && !normalize_pairing_pin(pairing_pin))
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Pairing PIN must be 6 letters/digits");
        return ESP_FAIL;
    }

    self->config_.wifi_ssid = local_mode ? "" : ssid;
    self->config_.wifi_password = local_mode ? "" : pass;

    if (!self->save_config_to_nvs())
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to save config");
        return ESP_FAIL;
    }

    if (self->operating_mode_callback_)
    {
        if (local_mode)
        {
            self->operating_mode_callback_("local_mode");
        }
        else
        {
            self->operating_mode_callback_(pure_mqtt ? "home_assistant" : "cloud_ha");
        }
    }

    if (self->pairing_pin_callback_)
    {
        self->pairing_pin_callback_(pure_mqtt ? "" : pairing_pin);
    }

    self->portal_submitted_.store(true);
    const char* response = local_mode
        ? "Saved. Device will restart to Local Mode. Reconnect to ECU AP and open http://192.168.4.1/local_mode"
        : "Saved. Device will restart and apply selected mode.";
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

    portal_submitted_.store(false);
    ESP_LOGI(kTag, "Provisioning portal running on SoftAP");

    const uint64_t start_ms = static_cast<uint64_t>(esp_timer_get_time() / 1000ULL);
    while (!portal_submitted_.load())
    {
        const uint64_t now_ms = static_cast<uint64_t>(esp_timer_get_time() / 1000ULL);
        if ((now_ms - start_ms) >= kPortalTimeoutMs)
        {
            ESP_LOGW(kTag, "Provisioning portal timeout after %lu ms", static_cast<unsigned long>(kPortalTimeoutMs));
            stop_http_server();
            return;
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    stop_http_server();
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
}

} // namespace bsw
