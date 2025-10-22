#include "OTAWebServer.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_netif.h"
#include <string.h>


static const char *TAG = "OTA_WEB";
OTAWebServer* OTAWebServer::instance_ = nullptr;

OTAWebServer::OTAWebServer(OTAMode mode,
                           const std::string& wifi_ssid,
                           const std::string& wifi_pass,
                           const std::string& ap_ssid,
                           const std::string& ap_pass,
                           const std::string& fw_url)
    : mode_(mode), wifi_ssid_(wifi_ssid), wifi_pass_(wifi_pass),
      ap_ssid_(ap_ssid), ap_pass_(ap_pass), fw_url_(fw_url),
      server_(nullptr), ota_in_progress_(false)
{
    instance_ = this;
}

void OTAWebServer::begin() {
    // NVS برای WiFi
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    initWiFi();
    startWebServer();
}

// --- WiFi ---
void OTAWebServer::initWiFi() {
    esp_netif_init();
    esp_event_loop_create_default();

    if (mode_ == OTAMode::ROUTER_STA) {
        esp_netif_create_default_wifi_sta();
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        esp_wifi_init(&cfg);
        esp_wifi_set_mode(WIFI_MODE_STA);

        wifi_config_t wifi_config = {};
        strcpy((char*)wifi_config.sta.ssid, wifi_ssid_.c_str());
        strcpy((char*)wifi_config.sta.password, wifi_pass_.c_str());

        esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
        esp_wifi_start();
        esp_wifi_connect();
        ESP_LOGI(TAG,"Connecting to router STA...");
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    } else {
        // DIRECT_AP: AP برای موبایل + STA برای اینترنت گوشی
        esp_netif_create_default_wifi_sta();
        esp_netif_create_default_wifi_ap();

        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        esp_wifi_init(&cfg);
        esp_wifi_set_mode(WIFI_MODE_APSTA);

        // STA اینترنت
        wifi_config_t wifi_sta_config = {};
        strcpy((char*)wifi_sta_config.sta.ssid, wifi_ssid_.c_str());
        strcpy((char*)wifi_sta_config.sta.password, wifi_pass_.c_str());
        esp_wifi_set_config(WIFI_IF_STA, &wifi_sta_config);

        // AP برای موبایل
        wifi_config_t wifi_ap_config = {};
        strcpy((char*)wifi_ap_config.ap.ssid, ap_ssid_.c_str());
        strcpy((char*)wifi_ap_config.ap.password, ap_pass_.c_str());
        wifi_ap_config.ap.max_connection = 4;
        wifi_ap_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
        esp_wifi_set_config(WIFI_IF_AP, &wifi_ap_config);

        esp_wifi_start();
        esp_wifi_connect();
        ESP_LOGI(TAG,"AP+STA mode started");
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}


void OTAWebServer::otaTask(void* param) {
    OTAWebServer* self = static_cast<OTAWebServer*>(param);

    // ساخت config برای http client
    esp_http_client_config_t http_cfg = {};
    http_cfg.url = self->fw_url_.c_str();
    http_cfg.cert_pem = nullptr; // چون HTTP هست

    // config اصلی OTA
    esp_https_ota_config_t ota_config = {};
    ota_config.http_config = &http_cfg;  // <- حتماً به آدرس struct اشاره بده

    ESP_LOGI(TAG,"Starting OTA...");
    esp_err_t ret = esp_https_ota(&ota_config);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG,"OTA success, restarting...");
        esp_restart();
    } else {
        ESP_LOGE(TAG,"OTA failed");
    }
    self->ota_in_progress_ = false;
    vTaskDelete(NULL);
}


// --- Web Server ---
void OTAWebServer::startWebServer() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    if (httpd_start(&server_, &config) == ESP_OK) {
        httpd_uri_t root_uri = {"/", HTTP_GET, rootHandler, nullptr};
        httpd_register_uri_handler(server_, &root_uri);

        httpd_uri_t update_uri = {"/update", HTTP_GET, updateHandler, nullptr};
        httpd_register_uri_handler(server_, &update_uri);

        httpd_uri_t status_uri = {"/status", HTTP_GET, statusHandler, nullptr};
        httpd_register_uri_handler(server_, &status_uri);
    }
}

// --- Handlers ---
esp_err_t OTAWebServer::rootHandler(httpd_req_t *req) {
    const char html[] =
        "<!DOCTYPE html>"
        "<html>"
        "<head><title>ESP32 OTA</title></head>"
        "<body>"
        "<h2>ESP32 Firmware Update</h2>"
        "<form action=\"/update\" method=\"get\">"
        "<button type=\"submit\">Update Firmware</button>"
        "</form>"
        "<div id=\"status\"></div>"
        "<script>"
        "function checkStatus(){"
        "fetch('/status').then(r=>r.text()).then(t=>{"
        "document.getElementById('status').innerHTML=t;"
        "if(t.includes('Updating')) setTimeout(checkStatus,1000);"
        "});"
        "}"
        "checkStatus();"
        "</script>"
        "</body></html>";

    httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t OTAWebServer::updateHandler(httpd_req_t *req) {
    if (!instance_->ota_in_progress_) {
        instance_->ota_in_progress_ = true;
        xTaskCreate(&OTAWebServer::otaTask, "ota_task", 8192, instance_, 5, nullptr);
    }
    const char resp[] = "Updating firmware...";
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t OTAWebServer::statusHandler(httpd_req_t *req) {
    const char* status = instance_->ota_in_progress_ ? "Updating..." : "Idle";
    httpd_resp_send(req, status, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}
