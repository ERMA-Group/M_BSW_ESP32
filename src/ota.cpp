#include "ota.hpp"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_ota_ops.h"

namespace bsw {

esp_err_t Ota::start_update(const char* url) {
    if (url == nullptr || url[0] == '\0') {
        ESP_LOGE("OTA", "Invalid firmware URL");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGW("OTA", "Starting Native HTTP OTA: %s", url);

    esp_http_client_config_t config = {};
    config.url = url;
    config.timeout_ms = 30000;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        return ESP_FAIL;
    }

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        esp_http_client_cleanup(client);
        return err;
    }

    int content_length = esp_http_client_fetch_headers(client);
    if (content_length <= 0) {
        ESP_LOGE("OTA", "Failed to get content length");
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    // Prepare for the flash write
    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
    if (update_partition == nullptr) {
        ESP_LOGE("OTA", "No OTA partition available");
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    esp_ota_handle_t update_handle = 0;
    err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &update_handle);
    if (err != ESP_OK) {
        ESP_LOGE("OTA", "esp_ota_begin failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return err;
    }

    char *upgrade_data_buf = (char *)malloc(1024);
    if (upgrade_data_buf == nullptr) {
        ESP_LOGE("OTA", "Failed to allocate OTA buffer");
        esp_ota_abort(update_handle);
        esp_http_client_cleanup(client);
        return ESP_ERR_NO_MEM;
    }

    int binary_size = 0;
    while (1) {
        int data_read = esp_http_client_read(client, upgrade_data_buf, 1024);
        if (data_read == 0) break; // Finished
        if (data_read < 0) {
            ESP_LOGE("OTA", "Error: SSL/HTTP connection closed");
            err = ESP_FAIL;
            break;
        }

        err = esp_ota_write(update_handle, upgrade_data_buf, data_read);
        if (err != ESP_OK) {
            ESP_LOGE("OTA", "esp_ota_write failed: %s", esp_err_to_name(err));
            break;
        }

        binary_size += data_read;
        ESP_LOGI("OTA", "Written: %d bytes", binary_size);
    }

    if (err != ESP_OK || binary_size <= 0) {
        ESP_LOGE("OTA", "OTA transfer failed, aborting update");
        esp_ota_abort(update_handle);
        free(upgrade_data_buf);
        esp_http_client_cleanup(client);
        return (err == ESP_OK) ? ESP_FAIL : err;
    }

    err = esp_ota_end(update_handle);
    if (err != ESP_OK) {
        ESP_LOGE("OTA", "esp_ota_end failed: %s", esp_err_to_name(err));
        free(upgrade_data_buf);
        esp_http_client_cleanup(client);
        return err;
    }

    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        ESP_LOGE("OTA", "esp_ota_set_boot_partition failed: %s", esp_err_to_name(err));
        free(upgrade_data_buf);
        esp_http_client_cleanup(client);
        return err;
    }
    
    ESP_LOGI("OTA", "Success! Total: %d bytes. Rebooting...", binary_size);
    free(upgrade_data_buf);
    esp_http_client_cleanup(client);
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