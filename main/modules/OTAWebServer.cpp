#include "OTAWebServer.h"
#include "ADC_AD7191.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_mac.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "mdns.h"
#include <algorithm>

static const char *TAG = "OTA_WEB";


// تعریف کلیدهای NVS
const char* NVS_NAMESPACE = "ota_config";
const char* KEY_WIFI_SSID = "wifi_ssid";
const char* KEY_WIFI_PASS = "wifi_pass";
const char* KEY_AP_SSID = "ap_ssid";
const char* KEY_AP_PASS = "ap_pass";
const char* KEY_FW_URL = "fw_url";
const char* KEY_USERNAME = "username";
const char* KEY_PASSWORD = "password";
const char* KEY_MODE = "mode";

OTAWebServer* OTAWebServer::instance_ = nullptr;

OTAWebServer::OTAWebServer(OTAMode mode,
                           const std::string& wifi_ssid,
                           const std::string& wifi_pass,
                           const std::string& ap_ssid,
                           const std::string& ap_pass,
                           const std::string& fw_url,
                           const std::string& username,
                           const std::string& password)
    : mode_(mode),
      wifi_ssid_(wifi_ssid), wifi_pass_(wifi_pass),
      ap_ssid_(ap_ssid), ap_pass_(ap_pass), fw_url_(fw_url),
      username_(username), password_(password),
      server_(nullptr), ota_in_progress_(false), is_logged_in_(false),
      adc_(nullptr), wifi_connected_(false),
      hostname_("jahat")
{
    instance_ = this;
    loadSettings(); // بارگذاری تنظیمات ذخیره شده (جایگزین مقادیر پیش‌فرض می‌شود اگر وجود داشته باشند)
}

void OTAWebServer::loadSettings() {
    ESP_LOGI(TAG, "========== [LOAD SETTINGS] STARTED ==========");
    
    nvs_handle_t nvs_handle;
    // تلاش برای باز کردن NVS در حالت فقط خواندن
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "[LOAD] Failed to open NVS namespace '%s'. Error: %s", NVS_NAMESPACE, esp_err_to_name(err));
        ESP_LOGE(TAG, "[LOAD] This is normal on first boot. Using default constructor values.");
        return;
    }
    ESP_LOGI(TAG, "[LOAD] NVS Opened successfully.");

    size_t required_size = 0;
    
    // --- خواندن WIFI SSID ---
    err = nvs_get_str(nvs_handle, KEY_WIFI_SSID, NULL, &required_size);
    if (err == ESP_OK && required_size > 0) {
        char* buf = new char[required_size];
        nvs_get_str(nvs_handle, KEY_WIFI_SSID, buf, &required_size);
        wifi_ssid_ = std::string(buf);
        ESP_LOGI(TAG, "[LOAD] Loaded WiFi SSID: '%s'", buf);
        delete[] buf;
    } else {
        ESP_LOGW(TAG, "[LOAD] WiFi SSID not found in NVS (Err: %s). Keeping default.", esp_err_to_name(err));
    }

    // --- خواندن WIFI PASS ---
    required_size = 0;
    err = nvs_get_str(nvs_handle, KEY_WIFI_PASS, NULL, &required_size);
    if (err == ESP_OK && required_size > 0) {
        char* buf = new char[required_size];
        nvs_get_str(nvs_handle, KEY_WIFI_PASS, buf, &required_size);
        wifi_pass_ = std::string(buf);
        ESP_LOGI(TAG, "[LOAD] Loaded WiFi PASS: '%s' (Length: %d)", buf, strlen(buf)); // پسورد لاگ می‌شود، مراقب باشید
        delete[] buf;
    } else {
        ESP_LOGW(TAG, "[LOAD] WiFi PASS not found in NVS. Keeping default.");
    }

    // --- خواندن AP SSID ---
    required_size = 0;
    err = nvs_get_str(nvs_handle, KEY_AP_SSID, NULL, &required_size);
    if (err == ESP_OK && required_size > 0) {
        char* buf = new char[required_size];
        nvs_get_str(nvs_handle, KEY_AP_SSID, buf, &required_size);
        ap_ssid_ = std::string(buf);
        ESP_LOGI(TAG, "[LOAD] Loaded AP SSID: '%s'", buf);
        delete[] buf;
    } else {
        ESP_LOGW(TAG, "[LOAD] AP SSID not found in NVS. Keeping default.");
    }

    // --- خواندن AP PASS ---
    required_size = 0;
    err = nvs_get_str(nvs_handle, KEY_AP_PASS, NULL, &required_size);
    if (err == ESP_OK && required_size > 0) {
        char* buf = new char[required_size];
        nvs_get_str(nvs_handle, KEY_AP_PASS, buf, &required_size);
        ap_pass_ = std::string(buf); // اصلاح شد: قبلاً اشتباهاً در wifi_pass_ ذخیره می‌شد
        ESP_LOGI(TAG, "[LOAD] Loaded AP PASS: '%s'", buf);
        delete[] buf;
    } else {
        ESP_LOGW(TAG, "[LOAD] AP PASS not found in NVS. Keeping default.");
    }

    // --- خواندن FW URL ---
    required_size = 0;
    err = nvs_get_str(nvs_handle, KEY_FW_URL, NULL, &required_size);
    if (err == ESP_OK && required_size > 0) {
        char* buf = new char[required_size];
        nvs_get_str(nvs_handle, KEY_FW_URL, buf, &required_size);
        fw_url_ = std::string(buf);
        ESP_LOGI(TAG, "[LOAD] Loaded FW URL: '%s'", buf);
        delete[] buf;
    } else {
        ESP_LOGW(TAG, "[LOAD] FW URL not found in NVS. Keeping default.");
    }

    // --- خواندن USERNAME ---
    required_size = 0;
    err = nvs_get_str(nvs_handle, KEY_USERNAME, NULL, &required_size);
    if (err == ESP_OK && required_size > 0) {
        char* buf = new char[required_size];
        nvs_get_str(nvs_handle, KEY_USERNAME, buf, &required_size);
        username_ = std::string(buf);
        ESP_LOGI(TAG, "[LOAD] Loaded Username: '%s'", buf);
        delete[] buf;
    } else {
        ESP_LOGW(TAG, "[LOAD] Username not found in NVS. Keeping default.");
    }

    // --- خواندن PASSWORD ---
    required_size = 0;
    err = nvs_get_str(nvs_handle, KEY_PASSWORD, NULL, &required_size);
    if (err == ESP_OK && required_size > 0) {
        char* buf = new char[required_size];
        nvs_get_str(nvs_handle, KEY_PASSWORD, buf, &required_size);
        password_ = std::string(buf);
        ESP_LOGI(TAG, "[LOAD] Loaded Password: '%s'", buf);
        delete[] buf;
    } else {
        ESP_LOGW(TAG, "[LOAD] Password not found in NVS. Keeping default.");
    }

    // --- خواندن MODE ---
    int8_t mode_val = 0;
    err = nvs_get_i8(nvs_handle, KEY_MODE, &mode_val);
    if (err == ESP_OK) {
        mode_ = static_cast<OTAMode>(mode_val);
        ESP_LOGI(TAG, "[LOAD] Loaded Mode: %d", mode_val);
    } else {
        ESP_LOGW(TAG, "[LOAD] Mode not found in NVS. Keeping default.");
    }

    nvs_close(nvs_handle);
    ESP_LOGI(TAG, "========== [LOAD SETTINGS] FINISHED ==========");
}

void OTAWebServer::saveSettings() {
    ESP_LOGI(TAG, "========== [SAVE SETTINGS] STARTED ==========");
    
    nvs_handle_t nvs_handle;
    // باز کردن NVS در حالت خواندن/نوشتن
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "[SAVE] Failed to open NVS namespace '%s'. Error: %s", NVS_NAMESPACE, esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG, "[SAVE] NVS Opened successfully for writing.");

    // --- ذخیره WIFI SSID ---
    err = nvs_set_str(nvs_handle, KEY_WIFI_SSID, wifi_ssid_.c_str());
    if (err != ESP_OK) ESP_LOGE(TAG, "[SAVE] Error saving WiFi SSID: %s", esp_err_to_name(err));
    else ESP_LOGI(TAG, "[SAVE] WiFi SSID saved: '%s'", wifi_ssid_.c_str());

    // --- ذخیره WIFI PASS ---
    err = nvs_set_str(nvs_handle, KEY_WIFI_PASS, wifi_pass_.c_str());
    if (err != ESP_OK) ESP_LOGE(TAG, "[SAVE] Error saving WiFi PASS: %s", esp_err_to_name(err));
    else ESP_LOGI(TAG, "[SAVE] WiFi PASS saved.");

    // --- ذخیره AP SSID ---
    err = nvs_set_str(nvs_handle, KEY_AP_SSID, ap_ssid_.c_str());
    if (err != ESP_OK) ESP_LOGE(TAG, "[SAVE] Error saving AP SSID: %s", esp_err_to_name(err));
    else ESP_LOGI(TAG, "[SAVE] AP SSID saved: '%s'", ap_ssid_.c_str());

    // --- ذخیره AP PASS ---
    err = nvs_set_str(nvs_handle, KEY_AP_PASS, ap_pass_.c_str());
    if (err != ESP_OK) ESP_LOGE(TAG, "[SAVE] Error saving AP PASS: %s", esp_err_to_name(err));
    else ESP_LOGI(TAG, "[SAVE] AP PASS saved.");

    // --- ذخیره FW URL ---
    err = nvs_set_str(nvs_handle, KEY_FW_URL, fw_url_.c_str());
    if (err != ESP_OK) ESP_LOGE(TAG, "[SAVE] Error saving FW URL: %s", esp_err_to_name(err));
    else ESP_LOGI(TAG, "[SAVE] FW URL saved: '%s'", fw_url_.c_str());

    // --- ذخیره USERNAME ---
    err = nvs_set_str(nvs_handle, KEY_USERNAME, username_.c_str());
    if (err != ESP_OK) ESP_LOGE(TAG, "[SAVE] Error saving Username: %s", esp_err_to_name(err));
    else ESP_LOGI(TAG, "[SAVE] Username saved: '%s'", username_.c_str());

    // --- ذخیره PASSWORD ---
    err = nvs_set_str(nvs_handle, KEY_PASSWORD, password_.c_str());
    if (err != ESP_OK) ESP_LOGE(TAG, "[SAVE] Error saving Password: %s", esp_err_to_name(err));
    else ESP_LOGI(TAG, "[SAVE] Password saved.");

    // --- ذخیره MODE ---
    err = nvs_set_i8(nvs_handle, KEY_MODE, static_cast<int8_t>(mode_));
    if (err != ESP_OK) ESP_LOGE(TAG, "[SAVE] Error saving Mode: %s", esp_err_to_name(err));
    else ESP_LOGI(TAG, "[SAVE] Mode saved: %d", (int)mode_);
    
    // --- کامیت کردن تغییرات ---
    ESP_LOGI(TAG, "[SAVE] Committing changes to flash...");
    err = nvs_commit(nvs_handle);
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "[SAVE] FAILED to commit NVS! Error: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "[SAVE] NVS Commit successful.");
    }
    
    nvs_close(nvs_handle);
    ESP_LOGI(TAG, "========== [SAVE SETTINGS] FINISHED ==========");
}





void OTAWebServer::setWiFiCredentials(const std::string& ssid, const std::string& pass) {
    wifi_ssid_ = ssid;
    wifi_pass_ = pass;
    ESP_LOGI(TAG, "WiFi Credentials updated via Serial.");
}

void OTAWebServer::setWebCredentials(const std::string& username, const std::string& password) {
    username_ = username;
    password_ = password;
    ESP_LOGI(TAG, "Web Credentials updated via Serial.");
}

void OTAWebServer::saveAndRestart() {
    ESP_LOGI(TAG, "Saving settings and restarting...");
    
    // ذخیره تنظیمات فعلی در NVS
    saveSettings();
    
    // تاخیر کوتاه برای اطمینان از تکمیل نوشتن
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    esp_restart();
}

void OTAWebServer::startMDNS() {
    // راه‌اندازی سرویس mDNS
    esp_err_t ret = mdns_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "mDNS Init failed: %s", esp_err_to_name(ret));
        return;
    }
    
    // تبدیل نام هاست به حروف کوچک
    std::string host = hostname_;
    for (char &c : host) {
        c = tolower(c);
    }
    
    ESP_ERROR_CHECK(mdns_hostname_set(host.c_str()));
    ESP_ERROR_CHECK(mdns_instance_name_set("Jahat POS Scale"));
    
    // اضافه کردن سرویس HTTP
    mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);

    // --- بخش مهم: فعال‌سازی روی همه اینترفیس‌ها ---
    // این دستور باعث می‌شود mDNS هم روی AP و هم روی STA کار کند
    mdns_hostname_set(host.c_str()); 
    
    ESP_LOGI(TAG, "mDNS started: http://%s.local", host.c_str());
}

void OTAWebServer::setAdcInstance(ADC_AD7191* adc) {
    adc_ = adc;
}

void OTAWebServer::begin() {
    initWiFi();
    startMDNS();
    startWebServer();
}

void OTAWebServer::wifi_event_handler(void* arg, esp_event_base_t event_base,
                                      int32_t event_id, void* event_data)
{
    OTAWebServer* self = static_cast<OTAWebServer*>(arg);
    
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "WiFi Disconnected.");
        self->wifi_connected_ = false;
        
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
        ESP_LOGI(TAG, "WiFi Connected");
        self->wifi_connected_ = true;
        
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ESP_LOGI(TAG, "WiFi Got IP");
    }
}

void OTAWebServer::initWiFi() {
    esp_netif_init();
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    // ثبت هندلرهای رویداد
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, this));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, this));

    // ساختار تنظیمات اولیه WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    switch (mode_) {
        case OTAMode::ROUTER_STA:
        {
            // --- حالت فقط اتصال به مودم (STA) ---
            esp_netif_create_default_wifi_sta();
            esp_wifi_set_mode(WIFI_MODE_STA);

            wifi_config_t wifi_config = {};
            strcpy((char*)wifi_config.sta.ssid, wifi_ssid_.c_str());
            strcpy((char*)wifi_config.sta.password, wifi_pass_.c_str());
            esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
            
            esp_wifi_start();
            esp_wifi_connect();
            ESP_LOGI(TAG, "در حال اتصال به مودم (حالت STA)...");
            break;
        }

        case OTAMode::DIRECT_AP:
        {
            // --- حالت فقط اکسس پوینت (AP) ---
            esp_netif_create_default_wifi_ap();
            esp_wifi_set_mode(WIFI_MODE_AP);

            wifi_config_t wifi_config = {};
            strcpy((char*)wifi_config.ap.ssid, ap_ssid_.c_str());
            strcpy((char*)wifi_config.ap.password, ap_pass_.c_str());
            wifi_config.ap.max_connection = 4;
            wifi_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
            esp_wifi_set_config(WIFI_IF_AP, &wifi_config);

            esp_wifi_start();
            ESP_LOGI(TAG, "اکسس پوینت شروع شد (حالت AP). SSID: %s", ap_ssid_.c_str());
            break;
        }

        case OTAMode::HYBRID:
        default:
        {
            // --- حالت ترکیبی (AP + STA) ---
            esp_netif_create_default_wifi_sta();
            esp_netif_create_default_wifi_ap();
            esp_wifi_set_mode(WIFI_MODE_APSTA);

            // تنظیمات STA (اتصال به مودم)
            wifi_config_t wifi_sta_config = {};
            strcpy((char*)wifi_sta_config.sta.ssid, wifi_ssid_.c_str());
            strcpy((char*)wifi_sta_config.sta.password, wifi_pass_.c_str());
            esp_wifi_set_config(WIFI_IF_STA, &wifi_sta_config);

            // تنظیمات AP (هات‌اسپات)
            wifi_config_t wifi_ap_config = {};
            strcpy((char*)wifi_ap_config.ap.ssid, ap_ssid_.c_str());
            strcpy((char*)wifi_ap_config.ap.password, ap_pass_.c_str());
            wifi_ap_config.ap.max_connection = 4;
            wifi_ap_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
            esp_wifi_set_config(WIFI_IF_AP, &wifi_ap_config);

            esp_wifi_start();
            esp_wifi_connect();
            ESP_LOGI(TAG, "حالت ترکیبی شروع شد (AP + STA)");
            break;
        }
    }

    vTaskDelay(2000 / portTICK_PERIOD_MS);
}

void OTAWebServer::otaTask(void* param) {
    OTAWebServer* self = static_cast<OTAWebServer*>(param);
    esp_http_client_config_t http_cfg = {};
    http_cfg.url = self->fw_url_.c_str();
    http_cfg.cert_pem = nullptr;
    esp_https_ota_config_t ota_config = {};
    ota_config.http_config = &http_cfg;
    ESP_LOGI(TAG,"شروع فرآیند آپدیت...");
    esp_err_t ret = esp_https_ota(&ota_config);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG,"آپدیت موفقیت آمیز بود");
        esp_restart();
    } else {
        ESP_LOGE(TAG,"آپدیت شکست خورد: %s", esp_err_to_name(ret));
        self->ota_in_progress_ = false;
    }
    vTaskDelete(NULL);
}

bool OTAWebServer::isAuthenticated(httpd_req_t *req) {
    return instance_->is_logged_in_;
}

void OTAWebServer::showLoginPage(httpd_req_t *req, bool hasError) {
    const char* errorHtml = hasError ? "<div class='error'>نام کاربری یا رمز عبور اشتباه است.</div>" : "";
    static char html[4096];
    int len = snprintf(html, sizeof(html),
        "<!DOCTYPE html><html lang='fa' dir='rtl'>"
        "<head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
        "<title>ورود</title>"
        "<style>body{font-family: Tahoma; background: #f4f4f4; display: flex; justify-content: center; align-items: center; height: 100vh;}"
        ".box{background: white; padding: 40px; border-radius: 10px; box-shadow: 0 0 10px rgba(0,0,0,0.1); text-align: center;}"
        "input{width: 100%%; padding: 10px; margin: 10px 0; border: 1px solid #ddd; border-radius: 5px; box-sizing: border-box;}"
        "button{width: 100%%; padding: 10px; background: #007bff; color: white; border: none; border-radius: 5px; cursor: pointer;}"
        ".error{color: red; margin-bottom: 10px;}</style></head>"
        "<body><div class='box'><h2>ورود به سیستم</h2>"
        "%s"
        "<form action='/login' method='post'>"
        "<input type='text' name='username' placeholder='نام کاربری' required>"
        "<input type='password' name='password' placeholder='رمز عبور' required>"
        "<button type='submit'>ورود</button></form></div></body></html>",
        errorHtml
    );
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
    httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
}

void OTAWebServer::startWebServer() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 32;
    config.max_resp_headers = 16;
    config.lru_purge_enable = true;
    config.stack_size = 16384;
    
    if (httpd_start(&server_, &config) == ESP_OK) {
        ESP_LOGI(TAG, "Registering URI handlers...");
        httpd_uri_t root_uri = { .uri = "/", .method = HTTP_GET, .handler = rootHandler, .user_ctx = NULL };
        httpd_register_uri_handler(server_, &root_uri);
        httpd_uri_t login_page_uri = { .uri = "/login.html", .method = HTTP_GET, .handler = loginPageHandler, .user_ctx = NULL };
        httpd_register_uri_handler(server_, &login_page_uri);
        httpd_uri_t login_uri = { .uri = "/login", .method = HTTP_POST, .handler = loginHandler, .user_ctx = NULL };
        httpd_register_uri_handler(server_, &login_uri);
        httpd_uri_t logout_uri = { .uri = "/logout", .method = HTTP_GET, .handler = logoutHandler, .user_ctx = NULL };
        httpd_register_uri_handler(server_, &logout_uri);
        httpd_uri_t update_uri = { .uri = "/update", .method = HTTP_GET, .handler = updateHandler, .user_ctx = NULL };
        httpd_register_uri_handler(server_, &update_uri);
        httpd_uri_t status_uri = { .uri = "/status", .method = HTTP_GET, .handler = statusHandler, .user_ctx = NULL };
        httpd_register_uri_handler(server_, &status_uri);
        httpd_uri_t calib_page_uri = { .uri = "/calib", .method = HTTP_GET, .handler = calibPageHandler, .user_ctx = NULL };
        httpd_register_uri_handler(server_, &calib_page_uri);
        httpd_uri_t calib_read_uri = { .uri = "/calib_read", .method = HTTP_GET, .handler = calibReadHandler, .user_ctx = NULL };
        httpd_register_uri_handler(server_, &calib_read_uri);
        httpd_uri_t calib_load_uri = { .uri = "/calib_load", .method = HTTP_GET, .handler = calibLoadHandler, .user_ctx = NULL };
        httpd_register_uri_handler(server_, &calib_load_uri);
        httpd_uri_t calib_save_uri = { .uri = "/calib_save", .method = HTTP_POST, .handler = calibSaveHandler, .user_ctx = NULL };
        httpd_register_uri_handler(server_, &calib_save_uri);
        httpd_uri_t calib_zero_uri = { .uri = "/calib_zero", .method = HTTP_POST, .handler = calibZeroHandler, .user_ctx = NULL };
        httpd_register_uri_handler(server_, &calib_zero_uri);
        httpd_uri_t calib_span_uri = { .uri = "/calib_span", .method = HTTP_POST, .handler = calibSpanHandler, .user_ctx = NULL };
        httpd_register_uri_handler(server_, &calib_span_uri);
        httpd_uri_t calib_finetune_uri = { .uri = "/calib_finetune", .method = HTTP_POST, .handler = calibFineTuneHandler, .user_ctx = NULL };
        httpd_register_uri_handler(server_, &calib_finetune_uri);
        httpd_uri_t calib_analyze_start_uri = { .uri = "/calib_analyze_start", .method = HTTP_GET, .handler = calibAnalyzeHandler, .user_ctx = NULL };
        httpd_register_uri_handler(server_, &calib_analyze_start_uri);
        httpd_uri_t calib_analyze_result_uri = { .uri = "/calib_analyze_result", .method = HTTP_GET, .handler = calibAnalyzeResultHandler, .user_ctx = NULL };
        httpd_register_uri_handler(server_, &calib_analyze_result_uri);
        httpd_uri_t manage_port_page_uri = { .uri = "/manage_port", .method = HTTP_GET, .handler = managePortPageHandler, .user_ctx = NULL };
        httpd_register_uri_handler(server_, &manage_port_page_uri);
        ESP_LOGI(TAG, "Web server started successfully");
    } else {
        ESP_LOGE(TAG, "Failed to start web server");
    }
}

esp_err_t OTAWebServer::rootHandler(httpd_req_t *req) {
    if (!instance_->isAuthenticated(req)) {
        httpd_resp_set_status(req, "302 Found");
        httpd_resp_set_hdr(req, "Location", "/login.html");
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }
    const char html[] =
        "<!DOCTYPE html><html lang='fa' dir='rtl'>"
        "<head>"
        "<meta charset='UTF-8'>"
        "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
        "<title>مدیریت ترازو</title>"
        "<style>"
        "body{font-family: Tahoma, sans-serif; background-color: #f0f2f5; display: flex; justify-content: center; align-items: center; height: 100vh; margin: 0;}"
        ".card{background: white; padding: 30px; border-radius: 15px; box-shadow: 0 4px 15px rgba(0,0,0,0.1); text-align: center; width: 300px;}"
        "h2{color: #333; margin-bottom: 20px;}"
        ".btn{background-color: #007bff; color: white; border: none; padding: 12px 24px; border-radius: 8px; font-size: 16px; cursor: pointer; width: 100%; margin-bottom: 10px; transition: background 0.3s;}"
        ".btn:hover{background-color: #0056b3;}"
        ".btn-green{background-color: #28a745;}"
        ".btn-green:hover{background-color: #218838;}"
        ".btn-orange{background-color: #fd7e14;}"
        ".btn-orange:hover{background-color: #e36b09;}"
        "</style>"
        "</head>"
        "<body>"
        "<div class='card'>"
        "<h2>پنل مدیریت</h2>"
        "<button class='btn' onclick='location.href=\"/calib\"'>کالیبراسیون ترازو</button>"
        "<button class='btn btn-green' onclick='startUpdate()'>آپدیت نرم‌افزار</button>"
        "<button class='btn btn-orange' onclick='location.href=\"/manage_port\"'>مدیریت پورت سریال</button>"
        "</div>"
        "<script>"
        "function startUpdate(){"
        "fetch('/update').then(r=>r.text()).then(()=>alert('آپدیت شروع شد'));"
        "}"
        "</script>"
        "</body></html>";
    httpd_resp_set_hdr(req, "Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
    httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t OTAWebServer::loginPageHandler(httpd_req_t *req) {
    instance_->showLoginPage(req, false);
    return ESP_OK;
}

esp_err_t OTAWebServer::loginHandler(httpd_req_t *req) {
    static char buf[512];
    static char user[32];
    static char pass[32];
    memset(buf, 0, sizeof(buf));
    memset(user, 0, sizeof(user));
    memset(pass, 0, sizeof(pass));
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret > 0) {
        buf[ret] = 0;
        httpd_query_key_value(buf, "username", user, sizeof(user));
        httpd_query_key_value(buf, "password", pass, sizeof(pass));
        if (instance_ != nullptr &&
            strcmp(user, instance_->username_.c_str()) == 0 &&
            strcmp(pass, instance_->password_.c_str()) == 0) {
            instance_->is_logged_in_ = true;
            httpd_resp_set_status(req, "302 Found");
            httpd_resp_set_hdr(req, "Location", "/");
            httpd_resp_send(req, NULL, 0);
            return ESP_OK;
        }
    }
    instance_->showLoginPage(req, true);
    return ESP_OK;
}

esp_err_t OTAWebServer::updateHandler(httpd_req_t *req) {
    if (!instance_->isAuthenticated(req)) {
        httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
        return ESP_FAIL;
    }
    if (!instance_->ota_in_progress_) {
        instance_->ota_in_progress_ = true;
        xTaskCreate(&OTAWebServer::otaTask, "ota_task", 8192, instance_, 5, nullptr);
    }
    const char resp[] = "Update started";
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t OTAWebServer::statusHandler(httpd_req_t *req) {
    const char* status = instance_->ota_in_progress_ ? "Updating..." : "Idle";
    httpd_resp_send(req, status, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t OTAWebServer::logoutHandler(httpd_req_t *req) {
    instance_->is_logged_in_ = false;
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/login.html");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

esp_err_t OTAWebServer::managePortPageHandler(httpd_req_t *req) {
    if (!instance_->isAuthenticated(req)) {
        httpd_resp_set_status(req, "302 Found");
        httpd_resp_set_hdr(req, "Location", "/login.html");
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }
    const char html[] =
        "<!DOCTYPE html><html lang='fa' dir='rtl'>"
        "<head>"
        "<meta charset='UTF-8'>"
        "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
        "<title>تنظیمات پورت خروجی</title>"
        "<style>"
        "body{font-family: Tahoma, sans-serif; background-color: #eef2f3; display: flex; justify-content: center; align-items: center; min-height: 100vh; margin: 0; padding: 20px;}"
        ".container{background: white; padding: 30px; border-radius: 15px; box-shadow: 0 5px 20px rgba(0,0,0,0.1); width: 100%%; max-width: 600px;}"
        "h2{text-align: center; color: #333; margin-bottom: 25px;}"
        ".section{border: 1px solid #ddd; border-radius: 8px; padding: 15px; margin-bottom: 20px; background: #fafafa;}"
        ".section-title{font-weight: bold; color: #007bff; margin-bottom: 10px; display: block;}"
        ".form-group{margin-bottom: 15px;}"
        "label{display: block; margin-bottom: 5px; color: #555; font-size: 14px;}"
        "select, input[type=number]{width: 100%%; padding: 10px; border: 1px solid #ccc; border-radius: 5px; box-sizing: border-box; font-family: Tahoma; background-color: #fff; font-size: 14px;}"
        ".button-container{display: flex; gap: 10px; margin-top: 20px; padding: 0 10px;}"
        ".btn-save, .btn-back{color: white; width: 100%%; padding: 14px; border: none; border-radius: 8px; font-size: 16px; cursor: pointer; text-align: center; text-decoration: none; transition: transform 0.1s ease; flex: 1; white-space: nowrap;}"
        ".btn-save{background-color: #28a745;}"
        ".btn-save:hover{background-color: #218838;}"
        ".btn-save:active{transform: scale(0.98);}"
        ".btn-back{background-color: #6c757d;}"
        ".btn-back:hover{background-color: #5a6268;}"
        ".btn-back:active{transform: scale(0.98);}"
        "@media (max-width: 480px) { .button-container{flex-direction: column;} }"
        "</style>"
        "</head>"
        "<body>"
        "<div class='container'>"
        "<h2>تنظیمات پورت سریال خروجی</h2>"
        "<div class='section'>"
        "<span class='section-title'>تنظیمات نرم‌افزاری (پروتکل)</span>"
        "<div class='form-group'>"
        "<label>نوع پروتکل خروجی (شرکت):</label>"
        "<select id='protocolType'>"
        "<option value='0'>پند</option>"
        "<option value='1'>تکین آرکا ۱</option>"
        "<option value='2'>تکین آرکا ۲</option>"
        "<option value='3'>توزین صدر</option>"
        "<option value='4'>رادین</option>"
        "<option value='5'>محک</option>"
        "<option value='6'>کارا توزین</option>"
        "<option value='7'>سروینا</option>"
        "</select>"
        "</div>"
        "</div>"
        "<div class='section'>"
        "<span class='section-title'>تنظیمات سخت‌افزاری پورت</span>"
        "<div class='form-group'>"
        "<label>نرخ انتقال داده (Baud Rate):</label>"
        "<select id='baudRate'>"
        "<option value='2400'>2400</option>"
        "<option value='4800'>4800</option>"
        "<option value='9600' selected>9600</option>"
        "<option value='19200'>19200</option>"
        "<option value='38400'>38400</option>"
        "<option value='57600'>57600</option>"
        "<option value='115200'>115200</option>"
        "</select>"
        "</div>"
        "<div class='form-group'>"
        "<label>تعداد بیت داده (Data Bits):</label>"
        "<select id='dataBits'>"
        "<option value='5'>5</option>"
        "<option value='6'>6</option>"
        "<option value='7'>7</option>"
        "<option value='8' selected>8</option>"
        "</select>"
        "</div>"
        "<div class='form-group'>"
        "<label>بیت توقف (Stop Bits):</label>"
        "<select id='stopBits'>"
        "<option value='1' selected>1</option>"
        "<option value='1.5'>1.5</option>"
        "<option value='2'>2</option>"
        "</select>"
        "</div>"
        "<div class='form-group'>"
        "<label>بیت توازن (Parity):</label>"
        "<select id='parity'>"
        "<option value='0' selected>None (بدون)</option>"
        "<option value='1'>Odd (فرد)</option>"
        "<option value='2'>Even (زوج)</option>"
        "</select>"
        "</div>"
        "</div>"
        "<div class='button-container'>"
        "<button class='btn-save' onclick='saveSettings()'>ذخیره تنظیمات</button>"
        "<button class='btn-back' onclick='location.href=\"/\"'>بازگشت به منو</button>"
        "</div>"
        "</div>"
        "<script>"
        "function saveSettings(){"
        "var params = new URLSearchParams();"
        "params.append('protocol', document.getElementById('protocolType').value);"
        "params.append('baud', document.getElementById('baudRate').value);"
        "params.append('dataBits', document.getElementById('dataBits').value);"
        "params.append('stopBits', document.getElementById('stopBits').value);"
        "params.append('parity', document.getElementById('parity').value);"
        "fetch('/serial_save', {method:'POST', body: params})"
        ".then(response => {"
        "if(response.ok) alert('تنظیمات با موفقیت ذخیره شد');"
        "else alert('خطا در ذخیره تنظیمات');"
        "})"
        ".catch(error => { console.error('Error:', error); alert('خطا در ارتباط'); });"
        "}"
        "</script>"
        "</body></html>";
    httpd_resp_set_hdr(req, "Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
    httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t OTAWebServer::calibPageHandler(httpd_req_t *req) {
    if (!instance_->isAuthenticated(req)) {
        httpd_resp_set_status(req, "302 Found");
        httpd_resp_set_hdr(req, "Location", "/login.html");
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }
    const char html[] =
        "<!DOCTYPE html><html lang='fa' dir='rtl'>"
        "<head>"
        "<meta charset='UTF-8'>"
        "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
        "<title>تنظیمات ترازو</title>"
        "<style>"
        "body{font-family: Tahoma, sans-serif; background-color: #eef2f3; display: flex; justify-content: center; align-items: center; min-height: 100vh; margin: 0; padding: 20px;}"
        ".container{background: white; padding: 30px; border-radius: 15px; box-shadow: 0 5px 20px rgba(0,0,0,0.1); width: 100%%; max-width: 600px;}"
        "h2{text-align: center; color: #333; margin-bottom: 25px;}"
        ".section{border: 1px solid #ddd; border-radius: 8px; padding: 20px; margin-bottom: 20px; background: #fafafa;}"
        ".section-title{font-weight: bold; color: #007bff; margin-bottom: 15px; display: block;}"
        ".weight-display{font-size: 48px; font-weight: bold; color: #28a745; text-align: center; margin: 20px 0; padding: 15px; background: #fff; border: 2px solid #28a745; border-radius: 10px; direction: ltr;}"
        ".form-group{margin-bottom: 15px;}"
        "label{display: block; margin-bottom: 8px; color: #555; font-size: 14px;}"
        "input[type=number], select{width: 100%%; padding: 10px; border: 1px solid #ccc; border-radius: 5px; box-sizing: border-box; font-family: Tahoma; background-color: #fff; font-size: 14px;}"
        ".btn-row{display: flex; gap: 10px; justify-content: center; margin-top: 15px;}"
        ".btn{flex: 1; padding: 12px; border: none; border-radius: 8px; color: white; font-size: 16px; cursor: pointer; text-align: center;}"
        ".btn:active{transform: scale(0.98);}"
        ".btn-zero{background-color: #dc3545;}"
        ".btn-zero:hover{background-color: #c82333;}"
        ".btn-span{background-color: #007bff;}"
        ".btn-span:hover{background-color: #0069d9;}"
        ".btn-up{background-color: #ffc107; color: #000; font-weight: bold;}"
        ".btn-up:hover{background-color: #e0a800;}"
        ".btn-down{background-color: #17a2b8; color: #fff; font-weight: bold;}"
        ".btn-down:hover{background-color: #138496;}"
        ".btn-save{background-color: #28a745; margin-top: 0;}"
        ".btn-save:hover{background-color: #218838;}"
        ".btn-back{background-color: #6c757d; color: white; border: none; border-radius: 8px; font-size: 16px; cursor: pointer; margin-top: 0; text-align: center; text-decoration: none;}"
        ".btn-back:hover{background-color: #5a6268;}"
        "</style>"
        "</head>"
        "<body>"
        "<div class='container'>"
        "<h2>تنظیمات و کالیبراسیون</h2>"
        "<div class='weight-display' id='weightVal'>---</div>"
        "<div class='section'>"
        "<span class='section-title'>عملیات کالیبراسیون</span>"
        "<div class='form-group'>"
        "<label>وزن نمونه برای ضریب (کیلوگرم):</label>"
        "<input type='number' id='knownWeight' placeholder='مثلا 15.000' step='0.001'>"
        "</div>"
        "<div class='btn-row'>"
        "<button class='btn btn-zero' onclick='doZero()'>تنظیم صفر</button>"
        "<button class='btn btn-span' onclick='doSpan()'>تنظیم ضریب</button>"
        "</div>"
        "<div class='btn-row'>"
        "<button class='btn btn-up' onclick='fineTune(1)'>▲ افزایش ضریب</button>"
        "<button class='btn btn-down' onclick='fineTune(-1)'>▼ کاهش ضریب</button>"
        "</div>"
        "</div>"
        "<div class='btn-row' style='margin-bottom: 20px;'>"
        "<button class='btn btn-save' style='background-color: #6f42c1;' onclick='startAnalysis()'>محاسبه خودکار دقت</button>"
        "</div>"
        "<div class='section'>"
        "<span class='section-title'>تنظیمات ظرفیت و دقت</span>"
        "<div class='form-group'>"
        "<label>ظرفیت ماکزیمم (کیلوگرم):</label>"
        "<input type='number' id='maxCapacity' step='0.1'>"
        "</div>"
        "<div class='form-group'>"
        "<label>نقطه شکست (کیلوگرم):</label>"
        "<input type='number' id='breakPoint' step='0.1'>"
        "</div>"
        "<div class='form-group'>"
        "<label>دقت بازه اول (گرم):</label>"
        "<input type='number' id='resLow' step='1'>"
        "</div>"
        "<div class='form-group'>"
        "<label>دقت بازه دوم (گرم):</label>"
        "<input type='number' id='resHigh' step='1'>"
        "</div>"
        "</div>"
        "<div class='section'>"
        "<span class='section-title'>تنظیمات فیلتر نرم‌افزاری</span>"
        "<div class='form-group'>"
        "<label>نوع فیلتر:</label>"
        "<select id='firType'>"
        "<option value='0'>میانگین‌گیر</option>"
        "<option value='1'>سینک ۳</option>"
        "<option value='2'>سینک ۴</option>"
        "</select>"
        "</div>"
        "<div class='form-group'>"
        "<label>درجه فیلتر (تعداد ضریب):</label>"
        "<input type='number' id='firOrder' step='1' min='1' max='50'>"
        "</div>"
        "</div>"
        "<div class='section'>"
        "<span class='section-title'>تنظیمات حساسیت و ترشولدها</span>"
        "<div class='form-group'>"
        "<label>آستانه پایداری (گرم):</label>"
        "<input type='number' id='stabTh' step='1'>"
        "</div>"
        "<div class='form-group'>"
        "<label>تعداد دورهای پایدار:</label>"
        "<input type='number' id='stabCnt' step='1'>"
        "</div>"
        "<div class='form-group'>"
        "<label>آستانه وزن کم (گرم):</label>"
        "<input type='number' id='uwTh' step='1'>"
        "</div>"
        "</div>"
        "<div class='section'>"
        "<span class='section-title'>تنظیم صفرخودکار</span>"
        "<div class='form-group'>"
        "<label>فعال/غیرفعال:</label>"
        "<select id='ztEn'>"
        "<option value='1'>فعال</option>"
        "<option value='0'>غیرفعال</option>"
        "</select>"
        "</div>"
        "<div class='form-group'>"
        "<label>محدوده صفرخودکار (گرم):</label>"
        "<input type='number' id='ztRange' step='1'>"
        "</div>"
        "<div class='form-group'>"
        "<label>زمان تثبیت (ثانیه):</label>"
        "<input type='number' id='ztTime' step='0.1'>"
        "</div>"
        "</div>"
        "<div class='btn-row'>"
        "<button class='btn btn-save' onclick='saveSettings()'>ذخیره تنظیمات</button>"
        "<button class='btn btn-back' onclick='location.href=\"/\"'>بازگشت به منو</button>"
        "</div>"
        "</div>"
        "<script>"
        "function loadSettings(){"
        "fetch('/calib_load').then(r=>r.json()).then(data=>{"
        "if(data.maxCap) document.getElementById('maxCapacity').value = data.maxCap;"
        "if(data.breakPoint) document.getElementById('breakPoint').value = data.breakPoint;"
        "if(data.resLow) document.getElementById('resLow').value = (data.resLow * 1000).toFixed(0);"
        "if(data.resHigh) document.getElementById('resHigh').value = (data.resHigh * 1000).toFixed(0);"
        "if(data.firOrder) document.getElementById('firOrder').value = data.firOrder;"
        "if(data.firType !== undefined) document.getElementById('firType').value = data.firType;"
        "if(data.stabTh) document.getElementById('stabTh').value = (data.stabTh * 1000).toFixed(0);"
        "if(data.stabCnt) document.getElementById('stabCnt').value = data.stabCnt;"
        "if(data.uwTh) document.getElementById('uwTh').value = (data.uwTh * 1000).toFixed(0);"
        "if(data.ztEn !== undefined) document.getElementById('ztEn').value = data.ztEn;"
        "if(data.ztRange) document.getElementById('ztRange').value = (data.ztRange * 1000).toFixed(0);"
        "if(data.ztTime) document.getElementById('ztTime').value = data.ztTime;"
        "}).catch(e=>console.error('Load error', e));"
        "}"
        "function updateWeight(){"
        "fetch('/calib_read').then(r=>r.text()).then(t=>{"
        "document.getElementById('weightVal').innerText = (parseFloat(t) / 1000).toFixed(4);"
        "});"
        "}"
        "loadSettings();"
        "setInterval(updateWeight, 100);"
        "function saveSettings(){"
        "var params = new URLSearchParams();"
        "params.append('maxCapacity', document.getElementById('maxCapacity').value);"
        "params.append('breakPoint', document.getElementById('breakPoint').value);"
        "params.append('resLow', (document.getElementById('resLow').value / 1000));"
        "params.append('resHigh', (document.getElementById('resHigh').value / 1000));"
        "params.append('firType', document.getElementById('firType').value);"
        "params.append('firOrder', document.getElementById('firOrder').value);"
        "params.append('stabTh', (document.getElementById('stabTh').value / 1000));"
        "params.append('stabCnt', document.getElementById('stabCnt').value);"
        "params.append('uwTh', (document.getElementById('uwTh').value / 1000));"
        "params.append('ztEn', document.getElementById('ztEn').value);"
        "params.append('ztRange', (document.getElementById('ztRange').value / 1000));"
        "params.append('ztTime', document.getElementById('ztTime').value);"
        "fetch('/calib_save', {method:'POST', body: params})"
        ".then(r=>{ if(r.ok) alert('تنظیمات ذخیره شد'); else alert('خطا در ذخیره'); });"
        "}"
        "function doZero(){"
        "if(confirm('آیا مطمئن هستید که هیچ وزنی روی ترازو نیست؟')){"
        "var params = new URLSearchParams();"
        "params.append('maxCapacity', document.getElementById('maxCapacity').value);"
        "params.append('breakPoint', document.getElementById('breakPoint').value);"
        "params.append('resLow', (document.getElementById('resLow').value / 1000));"
        "params.append('resHigh', (document.getElementById('resHigh').value / 1000));"
        "params.append('firType', document.getElementById('firType').value);"
        "params.append('firOrder', document.getElementById('firOrder').value);"
        "fetch('/calib_zero', {method:'POST', body: params}).then(()=>alert('صفر تنظیم شد'));"
        "}"
        "}"
        "function doSpan(){"
        "var w = document.getElementById('knownWeight').value;"
        "if(w == '' || w <= 0){ alert('لطفا وزن نمونه معتبر وارد کنید'); return; }"
        "if(confirm('آیا وزن ' + w + ' کیلوگرم روی ترازو است؟')){"
        "var params = new URLSearchParams();"
        "params.append('weight', w);"
        "params.append('maxCapacity', document.getElementById('maxCapacity').value);"
        "params.append('breakPoint', document.getElementById('breakPoint').value);"
        "params.append('resLow', (document.getElementById('resLow').value / 1000));"
        "params.append('resHigh', (document.getElementById('resHigh').value / 1000));"
        "params.append('firType', document.getElementById('firType').value);"
        "params.append('firOrder', document.getElementById('firOrder').value);"
        "fetch('/calib_span', {method:'POST', body: params}).then(()=>alert('ضریب تنظیم شد'));"
        "}"
        "}"
        "function fineTune(direction){"
        "var params = new URLSearchParams();"
        "params.append('dir', direction);"
        "fetch('/calib_finetune', {method:'POST', body: params}).then(()=>{"
        "console.log('Fine tune request sent: ' + direction);"
        "});"
        "}"
        "function startAnalysis(){"
        "var btn = event.target;"
        "btn.innerText = 'در حال آنالیز... (۵ ثانیه)';"
        "btn.disabled = true;"
        "fetch('/calib_analyze_start').then(()=>{"
        "var checkInterval = setInterval(()=>{"
        "fetch('/calib_analyze_result').then(r=>r.json()).then(data=>{"
        "if(data.finished){"
        "clearInterval(checkInterval);"
        "btn.innerText = 'محاسبه خودکار دقت';"
        "btn.disabled = false;"
        "var resGrams = (data.res).toFixed(3);"
        "alert('دقت محاسبه شده: ' + resGrams + ' بیت');"
        "}"
        "});"
        "}, 500);"
        "});"
        "}"
        "</script>"
        "</body></html>";
    httpd_resp_set_hdr(req, "Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
    httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t OTAWebServer::calibLoadHandler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");
    static char json_buf[2048];
    int len = 0;
    if (instance_->adc_) {
        double maxCap = instance_->adc_->getMaxCapacity();
        double breakPoint = instance_->adc_->getBreakPoint();
        double resLow = instance_->adc_->getResLow();
        double resHigh = instance_->adc_->getResHigh();
        int firOrder = instance_->adc_->getFirOrder();
        int firType = (int)instance_->adc_->getFirType();
        double stabTh = instance_->adc_->getStabilityThreshold();
        int stabCnt = instance_->adc_->getStableCountLimit();
        double uwTh = instance_->adc_->getUnderWeightThreshold();
        int ztEn = instance_->adc_->getZeroTrackingEnabled() ? 1 : 0;
        double ztRange = instance_->adc_->getZeroTrackingRange();
        int ztTime = instance_->adc_->getZeroTrackingTime();
        len = snprintf(json_buf, sizeof(json_buf),
            "{"
            "\"maxCap\":%.3f, \"breakPoint\":%.3f, \"resLow\":%.4f, \"resHigh\":%.4f, "
            "\"firOrder\":%d, \"firType\":%d, "
            "\"stabTh\":%.4f, \"stabCnt\":%d, \"uwTh\":%.4f, "
            "\"ztEn\":%d, \"ztRange\":%.4f, \"ztTime\":%d"
            "}",
            maxCap, breakPoint, resLow, resHigh,
            firOrder, firType,
            stabTh, stabCnt, uwTh,
            ztEn, ztRange, ztTime
        );
    } else {
        len = snprintf(json_buf, sizeof(json_buf), "{\"error\":\"ADC Null\"}");
    }
    httpd_resp_send(req, json_buf, len);
    return ESP_OK;
}

esp_err_t OTAWebServer::calibSaveHandler(httpd_req_t *req) {
    ESP_LOGI(TAG, "[CalibSave] Handler called");
    double maxCap = 30.0;
    double breakPoint = 15.0;
    double resLow = 0.005;
    double resHigh = 0.01;
    int firOrder = 5;
    int firTypeVal = 0;
    double stabTh = 0.002;
    int stabCnt = 10;
    double uwTh = 0.05;
    int ztEn = 1;
    double ztRange = 0.05;
    int ztTime = 3;
    char buf[1024];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret > 0) {
        buf[ret] = 0;
        char val_str[32];
        if (httpd_query_key_value(buf, "maxCapacity", val_str, sizeof(val_str)) == ESP_OK) maxCap = atof(val_str);
        if (httpd_query_key_value(buf, "breakPoint", val_str, sizeof(val_str)) == ESP_OK) breakPoint = atof(val_str);
        if (httpd_query_key_value(buf, "resLow", val_str, sizeof(val_str)) == ESP_OK) resLow = atof(val_str);
        if (httpd_query_key_value(buf, "resHigh", val_str, sizeof(val_str)) == ESP_OK) resHigh = atof(val_str);
        if (httpd_query_key_value(buf, "firOrder", val_str, sizeof(val_str)) == ESP_OK) firOrder = atoi(val_str);
        if (httpd_query_key_value(buf, "firType", val_str, sizeof(val_str)) == ESP_OK) firTypeVal = atoi(val_str);
        if (httpd_query_key_value(buf, "stabTh", val_str, sizeof(val_str)) == ESP_OK) stabTh = atof(val_str);
        if (httpd_query_key_value(buf, "stabCnt", val_str, sizeof(val_str)) == ESP_OK) stabCnt = atoi(val_str);
        if (httpd_query_key_value(buf, "uwTh", val_str, sizeof(val_str)) == ESP_OK) uwTh = atof(val_str);
        if (httpd_query_key_value(buf, "ztEn", val_str, sizeof(val_str)) == ESP_OK) ztEn = atoi(val_str);
        if (httpd_query_key_value(buf, "ztRange", val_str, sizeof(val_str)) == ESP_OK) ztRange = atof(val_str);
        if (httpd_query_key_value(buf, "ztTime", val_str, sizeof(val_str)) == ESP_OK) ztTime = atoi(val_str);
        ESP_LOGI(TAG, "[CalibSave] Saving: ZTEn=%d, ZTTime=%d", ztEn, ztTime);
        if (instance_->adc_) {
            ADC_AD7191::FirType type = static_cast<ADC_AD7191::FirType>(firTypeVal);
            instance_->adc_->setRangeSettings(maxCap, breakPoint, resLow, resHigh);
            instance_->adc_->setStabilityParams(stabTh, stabCnt);
            instance_->adc_->setUnderWeightParams(uwTh);
            instance_->adc_->setZeroTracking(ztEn == 1);
            instance_->adc_->setZeroTrackingRange(ztRange);
            instance_->adc_->setZeroTrackingTime(ztTime);
            instance_->adc_->saveCalibration();
            const char resp[] = "Settings Saved";
            httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
        } else {
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
    } else {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t OTAWebServer::calibZeroHandler(httpd_req_t *req) {
    ESP_LOGI(TAG, "[CalibZero] Handler called");
    double maxCap = 30.0;
    double breakPoint = 15.0;
    double resLow = 0.005;
    double resHigh = 0.01;
    int firOrder = 5;
    int firTypeVal = 0;
    char buf[512];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret > 0) {
        buf[ret] = 0;
        char val_str[20];
        if (httpd_query_key_value(buf, "maxCapacity", val_str, sizeof(val_str)) == ESP_OK) maxCap = atof(val_str);
        if (httpd_query_key_value(buf, "breakPoint", val_str, sizeof(val_str)) == ESP_OK) breakPoint = atof(val_str);
        if (httpd_query_key_value(buf, "resLow", val_str, sizeof(val_str)) == ESP_OK) resLow = atof(val_str);
        if (httpd_query_key_value(buf, "resHigh", val_str, sizeof(val_str)) == ESP_OK) resHigh = atof(val_str);
        if (httpd_query_key_value(buf, "firOrder", val_str, sizeof(val_str)) == ESP_OK) firOrder = atoi(val_str);
        if (httpd_query_key_value(buf, "firType", val_str, sizeof(val_str)) == ESP_OK) firTypeVal = atoi(val_str);
    } else {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    if (instance_->adc_) {
        ADC_AD7191::FirType type = static_cast<ADC_AD7191::FirType>(firTypeVal);
        instance_->adc_->setRangeSettings(maxCap, breakPoint, resLow, resHigh);
        instance_->adc_->calibrateZero();
        ESP_LOGI(TAG, "[CalibZero] Calibration command sent.");
    } else {
        ESP_LOGE(TAG, "[CalibZero] ADC instance is NULL!");
    }
    const char resp[] = "Zero Calibrated";
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t OTAWebServer::calibSpanHandler(httpd_req_t *req) {
    ESP_LOGI(TAG, "[CalibSpan] Handler called");
    double knownWeight = 0;
    double maxCap = 30.0;
    double breakPoint = 15.0;
    double resLow = 0.005;
    double resHigh = 0.01;
    int firOrder = 5;
    int firTypeVal = 0;
    char buf[512];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret > 0) {
        buf[ret] = 0;
        char val_str[20];
        if (httpd_query_key_value(buf, "weight", val_str, sizeof(val_str)) == ESP_OK) knownWeight = atof(val_str);
        if (httpd_query_key_value(buf, "maxCapacity", val_str, sizeof(val_str)) == ESP_OK) maxCap = atof(val_str);
        if (httpd_query_key_value(buf, "breakPoint", val_str, sizeof(val_str)) == ESP_OK) breakPoint = atof(val_str);
        if (httpd_query_key_value(buf, "resLow", val_str, sizeof(val_str)) == ESP_OK) resLow = atof(val_str);
        if (httpd_query_key_value(buf, "resHigh", val_str, sizeof(val_str)) == ESP_OK) resHigh = atof(val_str);
        if (httpd_query_key_value(buf, "firOrder", val_str, sizeof(val_str)) == ESP_OK) firOrder = atoi(val_str);
        if (httpd_query_key_value(buf, "firType", val_str, sizeof(val_str)) == ESP_OK) firTypeVal = atoi(val_str);
    } else {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    if (instance_->adc_ && knownWeight > 0) {
        ADC_AD7191::FirType type = static_cast<ADC_AD7191::FirType>(firTypeVal);
        instance_->adc_->setRangeSettings(maxCap, breakPoint, resLow, resHigh);
        instance_->adc_->calibrateSpan(knownWeight);
        ESP_LOGI(TAG, "[CalibSpan] Calibration command sent.");
    } else {
        if (!instance_->adc_) ESP_LOGE(TAG, "[CalibSpan] ADC instance is NULL!");
        if (knownWeight <= 0) ESP_LOGW(TAG, "[CalibSpan] Invalid knownWeight: %.2f", knownWeight);
    }
    const char resp[] = "Span Calibrated";
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t OTAWebServer::calibReadHandler(httpd_req_t *req) {
    double weight = 0;
    if (instance_->adc_) {
        weight = instance_->adc_->read();
    } else {
        ESP_LOGE(TAG, "[READ] ADC is NULL!");
    }
    char resp[32];
    snprintf(resp, sizeof(resp), "%.3f", weight);
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t OTAWebServer::calibFineTuneHandler(httpd_req_t *req) {
    if (!instance_->isAuthenticated(req)) {
        httpd_resp_set_status(req, "302 Found");
        httpd_resp_set_hdr(req, "Location", "/login.html");
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }
    ESP_LOGI(TAG, "fine tunning.....................");
    char buf[50];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    ESP_LOGI(TAG, "fine tunning.......1111");
    if (ret > 0) {
            ESP_LOGI(TAG, "fine tunning.......2222");
        buf[ret] = '\0';
        char *dir_str = strstr(buf, "dir=");
        if (dir_str) {
            ESP_LOGI(TAG, "fine tunning.......3333");
            int direction = atoi(dir_str + 4);
            if (instance_->adc_) {
                ESP_LOGI(TAG, "fine tunning.......44444");
                instance_->adc_->fineTuneScale(direction);
            }
        }
    }
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_send(req, "OK", 2);
    return ESP_OK;
}

esp_err_t OTAWebServer::calibAnalyzeHandler(httpd_req_t *req) {
    if (!instance_->isAuthenticated(req)) return ESP_FAIL;
    if (instance_->adc_) {
        instance_->adc_->startResolutionAnalysis();
    }
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_send(req, "Analysis Started", 16);
    return ESP_OK;
}

esp_err_t OTAWebServer::calibAnalyzeResultHandler(httpd_req_t *req) {
    if (!instance_->isAuthenticated(req)) return ESP_FAIL;
    double res = 0.0;
    bool finished = false;
    if (instance_->adc_) {
        if (!instance_->adc_->isAnalyzingStability_) {
            res = instance_->adc_->getCalculatedResolution();
            finished = true;
        }
    }
    char resp[64];
    snprintf(resp, sizeof(resp), "{\"finished\":%s,\"res\":%.9f}", finished ? "true" : "false", res);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}