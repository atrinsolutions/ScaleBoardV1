#pragma once
#include <string>
#include "esp_http_server.h"
#include "esp_netif.h"

class ADC_AD7191;

enum class WIFIMode {
    DIRECT_AP = 0,    // فقط اکسس پوینت (بدون اتصال به مودم)
    ROUTER_STA = 1,   // فقط اتصال به مودم (Station)
    HYBRID = 2        // حالت ترکیبی (هم AP و هم STA)
};

class WebServer {
public:
    WebServer(WIFIMode mode = WIFIMode::HYBRID,
                 const std::string& wifi_ssid = "",
                 const std::string& wifi_pass = "",
                 const std::string& ap_ssid = "ESP32_AP",
                 const std::string& ap_pass = "12345678",
                 const std::string& fw_url = "http://jahatpro.ir/posscale/ps.bin",
                 const std::string& username = "admin",
                 const std::string& password = "admin");
    
    void begin();
    void setAdcInstance(ADC_AD7191* adc);
    
    static WebServer* instance_;

    // --- متدهای Setter (قبلاً داشتید یا اضافه شدند) ---
    void setWiFiCredentials(const std::string& ssid, const std::string& pass);
    void setWebCredentials(const std::string& username, const std::string& password);
    
    void setAPCredentials(const std::string& ssid, const std::string& pass) {
        ap_ssid_ = ssid;
        ap_pass_ = pass;
    }
    
    void setFwUrl(const std::string& url) {
        fw_url_ = url;
    }
    
    void setMode(WIFIMode mode) {
        mode_ = mode;
    }

    // --- متدهای Getter (جدید - برای خواندن تنظیمات در دستور GET_CONFIG) ---
    std::string getWiFiSSID() const { return wifi_ssid_; }
    std::string getWiFiPass() const { return wifi_pass_; }
    std::string getAPSSID() const { return ap_ssid_; }
    std::string getAPPass() const { return ap_pass_; }
    std::string getFwUrl() const { return fw_url_; }
    std::string getWebUser() const { return username_; }
    std::string getWebPass() const { return password_; }
    WIFIMode getMode() const { return mode_; }

    // --- متدهای مربوط به NVS ---
    void loadSettings();
    void saveSettings();
    
    // متد کمکی برای ذخیره و ریستارت (استفاده شده در SerialManager)
    void saveAndRestart();

private:
    // متغیرهای عضو (Member Variables)
    WIFIMode mode_;
    std::string wifi_ssid_;
    std::string wifi_pass_;
    std::string ap_ssid_;
    std::string ap_pass_;
    std::string fw_url_;
    std::string username_;
    std::string password_;
    
    httpd_handle_t server_;
    bool ota_in_progress_;
    bool is_logged_in_;
    ADC_AD7191* adc_;
    
    bool wifi_connected_;
    std::string hostname_; 
    
    void initWiFi();
    void startMDNS();
    void startWebServer();

    static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
    static void otaTask(void* param);
    
    bool isAuthenticated(httpd_req_t *req);
    void showLoginPage(httpd_req_t *req, bool hasError = false);
    
    static esp_err_t rootHandler(httpd_req_t *req);
    static esp_err_t loginPageHandler(httpd_req_t *req);
    static esp_err_t loginHandler(httpd_req_t *req);
    static esp_err_t updateHandler(httpd_req_t *req);
    static esp_err_t statusHandler(httpd_req_t *req);
    static esp_err_t logoutHandler(httpd_req_t *req);
    static esp_err_t managePortPageHandler(httpd_req_t *req);
    
    static esp_err_t calibPageHandler(httpd_req_t *req);
    static esp_err_t calibZeroHandler(httpd_req_t *req);
    static esp_err_t calibSpanHandler(httpd_req_t *req);
    static esp_err_t calibReadHandler(httpd_req_t *req);
    static esp_err_t calibSaveHandler(httpd_req_t *req);
    static esp_err_t calibLoadHandler(httpd_req_t *req);
    static esp_err_t calibFineTuneHandler(httpd_req_t *req);
    static esp_err_t calibAnalyzeHandler(httpd_req_t *req);
    static esp_err_t calibAnalyzeResultHandler(httpd_req_t *req);
};