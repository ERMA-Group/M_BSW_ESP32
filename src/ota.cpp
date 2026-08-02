#include "ota.hpp"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_ota_ops.h"
#include <cstring>
#include <atomic>

// Reference your global OTA flag to pause background tasks
std::atomic<bool> g_ota_in_progress;

namespace bsw {

namespace {

struct OtaContext {
    esp_ota_handle_t handle;
    int bytes_written;
    esp_err_t error;
};

esp_err_t ota_http_event_handler(esp_http_client_event_t* evt)
{
    auto* ctx = static_cast<OtaContext*>(evt->user_data);
    if (ctx == nullptr) return ESP_OK;

    switch (evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGE("OTA", "HTTP_EVENT_ERROR");
            ctx->error = ESP_FAIL;
            break;

        case HTTP_EVENT_ON_DATA:
            if (evt->data_len > 0) {
                esp_err_t err = esp_ota_write(ctx->handle, evt->data, evt->data_len);
                if (err != ESP_OK) {
                    ESP_LOGE("OTA", "esp_ota_write failed: %s", esp_err_to_name(err));
                    ctx->error = err;
                    return err;
                }
                ctx->bytes_written += evt->data_len;
                if (ctx->bytes_written % 100000 < 4096) {
                    ESP_LOGW("OTA", "Downloaded: %d bytes", ctx->bytes_written);
                }
            }
            break;

        case HTTP_EVENT_ON_FINISH:
            ESP_LOGW("OTA", "HTTP download finished");
            break;

        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGW("OTA", "HTTP disconnected");
            break;

        default:
            break;
    }
    return ESP_OK;
}

} // namespace

esp_err_t Ota::start_update(const char* url) {
    if (url == nullptr || url[0] == '\0') {
        ESP_LOGE("OTA", "Invalid firmware URL");
        return ESP_ERR_INVALID_ARG;
    }

    // 1. PAUSE BACKGROUND TASKS (Prevents WebServiceApi from triggering resets)
    g_ota_in_progress = true;

    ESP_LOGW("OTA", "Starting Native HTTP OTA: %s", url);

    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
    if (update_partition == nullptr) {
        ESP_LOGE("OTA", "No OTA partition available");
        g_ota_in_progress = false;
        return ESP_FAIL;
    }

    // 2. ERASE FLASH *BEFORE* OPENING THE HTTP CONNECTION
    // This uses 1027472 bytes (from your curl check) or full partition size.
    // By erasing BEFORE opening HTTP, OpenResty's 10s timer won't run during erase!
    esp_ota_handle_t update_handle = 0;
    esp_err_t err = esp_ota_begin(update_partition, 1027472, &update_handle);
    if (err != ESP_OK) {
        ESP_LOGE("OTA", "esp_ota_begin failed: %s", esp_err_to_name(err));
        g_ota_in_progress = false;
        return err;
    }

    OtaContext ctx = {};
    ctx.handle = update_handle;
    ctx.bytes_written = 0;
    ctx.error = ESP_OK;

    esp_http_client_config_t config = {};
    config.url = url;
    config.timeout_ms = 30000;
    config.event_handler = ota_http_event_handler;
    config.user_data = &ctx;
    config.buffer_size = 4096;
    config.buffer_size_tx = 1024;
    
    // Enable TCP Keep-Alive to satisfy OpenResty
    config.keep_alive_enable = true;
    config.keep_alive_idle = 5;
    config.keep_alive_interval = 2;
    config.keep_alive_count = 3;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE("OTA", "Failed to init HTTP client");
        esp_ota_abort(update_handle);
        g_ota_in_progress = false;
        return ESP_FAIL;
    }

    // Matching headers specifically for OpenResty
    esp_http_client_set_header(client, "User-Agent", "Mozilla/5.0 (ESP32) OTA Client");
    esp_http_client_set_header(client, "Accept", "*/*");
    esp_http_client_set_header(client, "Connection", "keep-alive");
    esp_http_client_set_header(client, "Host", "fwarchive.erma.sk");

    // 3. START HTTP TRANSFER
    err = esp_http_client_perform(client);

    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (err != ESP_OK || ctx.error != ESP_OK) {
        ESP_LOGE("OTA", "HTTP transfer failed: %s (Status: %d)", esp_err_to_name(err != ESP_OK ? err : ctx.error), status);
        esp_ota_abort(update_handle);
        g_ota_in_progress = false;
        return (err != ESP_OK) ? err : ctx.error;
    }

    if (status != 200) {
        ESP_LOGE("OTA", "HTTP status %d", status);
        esp_ota_abort(update_handle);
        g_ota_in_progress = false;
        return ESP_FAIL;
    }

    if (ctx.bytes_written <= 0) {
        ESP_LOGE("OTA", "No data received");
        esp_ota_abort(update_handle);
        g_ota_in_progress = false;
        return ESP_FAIL;
    }

    err = esp_ota_end(update_handle);
    if (err != ESP_OK) {
        ESP_LOGE("OTA", "esp_ota_end failed: %s", esp_err_to_name(err));
        g_ota_in_progress = false;
        return err;
    }

    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        ESP_LOGE("OTA", "esp_ota_set_boot_partition failed: %s", esp_err_to_name(err));
        g_ota_in_progress = false;
        return err;
    }

    ESP_LOGW("OTA", "Success! Total: %d bytes. Rebooting...", ctx.bytes_written);
    esp_restart();
    return ESP_OK;
}

void Ota::cancel_rollback() {
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
            esp_ota_mark_app_valid_cancel_rollback();
        }
    }
}

} // namespace bsw