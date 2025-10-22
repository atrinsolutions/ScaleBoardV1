#pragma once
#include <string>
#include "esp_http_server.h"

enum class OTAMode {
    DIRECT_AP = 0, // پیش‌فرض: گوشی مستقیم به ESP وصل شود
    ROUTER_STA = 1 // همه به مودم/روتر وصل شوند
};

class OTAWebServer {
public:
    OTAWebServer(OTAMode mode = OTAMode::DIRECT_AP,
                 const std::string& wifi_ssid = "",
                 const std::string& wifi_pass = "",
                 const std::string& ap_ssid = "ESP32_AP",
                 const std::string& ap_pass = "12345678",
                 const std::string& fw_url = "http://jahatpro.ir/posscale/ps.bin");

    void begin(); // راه‌اندازی WiFi و وب سرور

private:
    OTAMode mode_;
    std::string wifi_ssid_;
    std::string wifi_pass_;
    std::string ap_ssid_;
    std::string ap_pass_;
    std::string fw_url_;

    httpd_handle_t server_;
    bool ota_in_progress_;

    void initWiFi();
    void startWebServer();
    static void otaTask(void* param);

    // Handlerهای HTTP
    static esp_err_t rootHandler(httpd_req_t *req);
    static esp_err_t updateHandler(httpd_req_t *req);
    static esp_err_t statusHandler(httpd_req_t *req);

    // دسترسی به instance داخل static handlers
    static OTAWebServer* instance_;
};
