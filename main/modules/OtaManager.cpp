#include "OtaManager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

const char* OtaManager::TAG = "OTA_MGR";

void OtaManager::startUpdate(const std::string& url) {
    if (_isUpdating.load()) {
        ESP_LOGW(TAG, "Update is already running...");
        return;
    }
    
    _updateUrl = url;
    _progress = 0;
    _lastError = "";
    
    // ایجاد تسک برای جلوگیری از بلاک شدن وب‌سرور
    xTaskCreate(otaTask, "ota_worker_task", 8192, this, 5, NULL);
}

void OtaManager::otaTask(void* pvParameter) {
    OtaManager* self = static_cast<OtaManager*>(pvParameter);
    self->_isUpdating.store(true);

    esp_http_client_config_t http_config = {};
    http_config.url = self->_updateUrl.c_str();
    http_config.crt_bundle_attach = esp_crt_bundle_attach; // استفاده از گواهی‌های استاندارد
    http_config.keep_alive_enable = true;
    http_config.timeout_ms = 10000; // ده ثانیه تایم‌اوت

    esp_https_ota_config_t ota_config = {};
    ota_config.http_config = &http_config;

    ESP_LOGI(TAG, "Connecting to: %s", http_config.url);

    esp_https_ota_handle_t https_ota_handle = NULL;
    esp_err_t err = esp_https_ota_begin(&ota_config, &https_ota_handle);
    
    if (err != ESP_OK) {
        self->_lastError = "Connection Failed";
        self->_isUpdating.store(false);
        vTaskDelete(NULL);
        return;
    }

    // دریافت حجم فایل برای محاسبه درصد
    int total_size = esp_https_ota_get_image_size(https_ota_handle);
    
    while (true) {
        err = esp_https_ota_perform(https_ota_handle);
        if (err != ESP_ERR_HTTPS_OTA_IN_PROGRESS) break;

        int read_len = esp_https_ota_get_image_len_read(https_ota_handle);
        if (total_size > 0) {
            int p = (read_len * 100) / total_size;
            self->_progress.store(p);
            ESP_LOGD(TAG, "Progress: %d%%", p);
        }
    }

    if (esp_https_ota_is_complete_data_received(https_ota_handle)) {
        err = esp_https_ota_finish(https_ota_handle);
        if (err == ESP_OK) {
            self->_progress.store(100);
            ESP_LOGI(TAG, "OTA Success! Restarting in 2s...");
            vTaskDelay(pdMS_TO_TICKS(2000));
            esp_restart();
        } else {
            self->_lastError = "OTA Finish Error";
        }
    } else {
        self->_lastError = "Download Incomplete";
        esp_https_ota_abort(https_ota_handle);
    }

    self->_isUpdating.store(false);
    vTaskDelete(NULL);
}
