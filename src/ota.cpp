#include "ota.hpp"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_ota_ops.h"

namespace bsw {

esp_err_t Ota::start_update(const char* url) {
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
    esp_ota_handle_t update_handle = 0;
    esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &update_handle);

    char *upgrade_data_buf = (char *)malloc(1024);
    int binary_size = 0;
    while (1) {
        int data_read = esp_http_client_read(client, upgrade_data_buf, 1024);
        if (data_read == 0) break; // Finished
        if (data_read < 0) {
            ESP_LOGE("OTA", "Error: SSL/HTTP connection closed");
            break;
        }
        esp_ota_write(update_handle, upgrade_data_buf, data_read);
        binary_size += data_read;
        ESP_LOGI("OTA", "Written: %d bytes", binary_size);
    }

    esp_ota_end(update_handle);
    esp_ota_set_boot_partition(update_partition);
    
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