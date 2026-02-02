#include "SerialManager.h"
#include <cstring>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <sstream>
#include "esp_log.h"
#include "tusb_cdc_acm.h"
#include "esp_netif.h"
#include "OTAWebServer.h" // برای دسترسی به myServer

// تعریف سراسری سرور (که در main.cpp تعریف شده است)
extern OTAWebServer* myServer;

const char* SerialManagerBase::TAG = "SerialMgr";

// ==========================================
// پیاده‌سازی کلاس پایه
// ==========================================
SerialManagerBase::SerialManagerBase(SerialProtocol protocol) 
    : protocol_(protocol), adc_(nullptr) {}

void SerialManagerBase::setAdcInstance(ADC_AD7191* adc) {
    adc_ = adc;
    ESP_LOGI(TAG, "ADC Instance linked to SerialManager");
}

void SerialManagerBase::sendWeight() {
    if (adc_ == nullptr) return;
    WeightData data = adc_->getWeightData();
    std::string message;
    switch (protocol_) {
        case SerialProtocol::PROTOCOL_PAND: message = formatPand(data); break;
        case SerialProtocol::PROTOCOL_TAKIN_ARKA_1: message = formatTakinArka1(data); break;
        case SerialProtocol::PROTOCOL_TAKIN_ARKA_2: message = formatTakinArka2(data); break;
        case SerialProtocol::PROTOCOL_TOZIN_SADR: message = formatTozinSadr(data); break;
        case SerialProtocol::PROTOCOL_RADIN: message = formatRadin(data); break;
        case SerialProtocol::PROTOCOL_MAHAK: message = formatMahak(data); break;
        case SerialProtocol::PROTOCOL_KARA_TOZIN: message = formatKaraTozin(data); break;
        case SerialProtocol::PROTOCOL_SERVINA: message = formatServina(data); break;
        default: message = formatPand(data); break;
    }
    sendRaw(message.c_str(), message.length());
}

void SerialManagerBase::setProtocol(SerialProtocol protocol) {
    protocol_ = protocol;
    ESP_LOGI(TAG, "Protocol changed to %d", (int)protocol);
}

// --- پیاده‌سازی فرمت‌ها ---
std::string SerialManagerBase::formatPand(const WeightData& data) {
    char buf[64];
    char sign = data.isNegative() ? '-' : '+';
    snprintf(buf, sizeof(buf), "ST,GS,%c %7.3f kg\r\n", sign, data.getWeight());
    return std::string(buf);
}
std::string SerialManagerBase::formatTakinArka1(const WeightData& data) {
    char buf[64];
    char sign = data.isNegative() ? '-' : '+';
    snprintf(buf, sizeof(buf), "W=%c%07.3f\r\n", sign, data.getWeight());
    return std::string(buf);
}
std::string SerialManagerBase::formatTakinArka2(const WeightData& data) {
    char buf[64];
    char sign = data.isNegative() ? '-' : '+';
    snprintf(buf, sizeof(buf), "%c%08.3f\r\n", sign, data.getWeight());
    return std::string(buf);
}
std::string SerialManagerBase::formatTozinSadr(const WeightData& data) {
    char buf[64];
    char sign = data.isNegative() ? '-' : '+';
    snprintf(buf, sizeof(buf), "WT %c%07.3f KG\r\n", sign, data.getWeight());
    return std::string(buf);
}
std::string SerialManagerBase::formatRadin(const WeightData& data) {
    char buf[64];
    char sign = data.isNegative() ? '-' : '+';
    snprintf(buf, sizeof(buf), "NET %c%07.2f\r\n", sign, data.getWeight());
    return std::string(buf);
}
std::string SerialManagerBase::formatMahak(const WeightData& data) {
    char buf[64];
    char sign = data.isNegative() ? '-' : '+';
    snprintf(buf, sizeof(buf), "GS, %c%07.3f\r\n", sign, data.getWeight());
    return std::string(buf);
}
std::string SerialManagerBase::formatKaraTozin(const WeightData& data) {
    char buf[64];
    char sign = data.isNegative() ? '-' : '+';
    snprintf(buf, sizeof(buf), "W=%c%07.3f KG\r\n", sign, data.getWeight());
    return std::string(buf);
}
std::string SerialManagerBase::formatServina(const WeightData& data) {
    char buf[64];
    char sign = data.isNegative() ? '-' : '+';
    snprintf(buf, sizeof(buf), "ST,GS,%c%07.3f kg\r\n", sign, data.getWeight());
    return std::string(buf);
}

// --- مدیریت دریافت داده و دستورات (مشترک) ---
void SerialManagerBase::processRx(const uint8_t* buf, size_t len) {
    rx_buffer_.insert(rx_buffer_.end(), buf, buf + len);
    auto it = std::find(rx_buffer_.begin(), rx_buffer_.end(), '\n');
    
    while (it != rx_buffer_.end()) {
        std::string line(rx_buffer_.begin(), it);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        
        if (!line.empty()) {
            ESP_LOGI(TAG, "Received Cmd: %s", line.c_str());
            handleCommand(line);
        }
        rx_buffer_.erase(rx_buffer_.begin(), it + 1);
        it = std::find(rx_buffer_.begin(), rx_buffer_.end(), '\n');
    }
}

void SerialManagerBase::handleCommand(const std::string& cmd) {
    if (cmd == "GET_IP") {
        sendIP();
    } else if (cmd == "PING") {
        sendRaw("PONG\r\n", 6);
    } 
    else if (cmd == "SAVE_RESTART") {
        if (myServer != nullptr) {
            ESP_LOGI(TAG, "Command: SAVE_RESTART");
            sendRaw("OK: Saving settings and restarting...\r\n", 38);
            myServer->saveSettings(); 
            vTaskDelay(500 / portTICK_PERIOD_MS);
            esp_restart();
        } else {
            sendRaw("ERROR: Server not ready\r\n", 26);
        }
    }
    else if (cmd.rfind("SET_CONFIG ", 0) == 0) {
        // فرمت کامل جدید:
        // SET_CONFIG <SSID> <WiFiPass> <AP_SSID> <AP_Pass> <FW_URL> <WebUser> <WebPass> <Mode>
        
        std::string params = cmd.substr(11);
        std::stringstream ss(params);
        
        std::string ssid, wifi_pass, ap_ssid, ap_pass, fw_url, web_user, web_pass;
        int mode_val = 2; // پیش‌فرض Hybrid

        // خواندن 7 رشته و 1 عدد
        if (ss >> ssid >> wifi_pass >> ap_ssid >> ap_pass >> fw_url >> web_user >> web_pass >> mode_val) {
            if (myServer != nullptr) {
                ESP_LOGI(TAG, "Updating Full Config...");
                
                // 1. تنظیمات WiFi Station
                myServer->setWiFiCredentials(ssid, wifi_pass);
                
                // 2. تنظیمات WiFi AP (نیاز به متد جدید در OTAWebServer داریم یا دسترسی مستقیم)
                myServer->setAPCredentials(ap_ssid, ap_pass); 
                
                // 3. تنظیمات URL (نیاز به متد setter)
                myServer->setFwUrl(fw_url);

                // 4. تنظیمات وب
                myServer->setWebCredentials(web_user, web_pass);

                // 5. تنظیم مود (نیاز به متد setter)
                myServer->setMode(static_cast<OTAMode>(mode_val));

                ESP_LOGI(TAG, "Config received: Mode=%d", mode_val);
                
                sendRaw("OK: All Config Updated in Memory. Send 'SAVE_RESTART' to apply.\r\n", 60);
            } else {
                sendRaw("ERROR: Server not ready\r\n", 26);
            }
        } else {
            sendRaw("ERROR: Invalid format.\r\nFormat: SET_CONFIG <SSID> <WiFiPass> <AP_SSID> <AP_Pass> <FW_URL> <WebUser> <WebPass> <Mode>\r\n", 130);
        }
    } else {
        ESP_LOGW(TAG, "Unknown command: %s", cmd.c_str());
        sendRaw("ERROR: Unknown command\r\n", 25);
    }
}

void SerialManagerBase::sendIP() {
    esp_netif_ip_info_t ip_info;
    esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    char response[128];
    
    if (netif && esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
        snprintf(response, sizeof(response), "IP: %d.%d.%d.%d\r\n", IP2STR(&ip_info.ip));
    } else {
        snprintf(response, sizeof(response), "IP: Error\r\n");
    }
    sendRaw(response, strlen(response));
}

// ==========================================
// پیاده‌سازی کلاس SerialManagerUART
// ==========================================
SerialManagerUART::SerialManagerUART(uart_port_t uart_num, SerialProtocol protocol) 
    : SerialManagerBase(protocol), uart_num_(uart_num), is_initialized_(false) {}

SerialManagerUART::~SerialManagerUART() {
    if (is_initialized_) uart_driver_delete(uart_num_);
}

void SerialManagerUART::begin(int baudRate, int txPin, int rxPin, int dataBits, int stopBits, int parity) {
    // --- اصلاح مقادیر Data Bits برای تطابق با ESP-IDF ---
    // در ESP-IDF: 5bit=1, 6bit=2, 7bit=3, 8bit=0
    int esp_data_bits = UART_DATA_8_BITS; // پیش‌فرض (0)
    switch (dataBits) {
        case 5: esp_data_bits = UART_DATA_5_BITS; break;
        case 6: esp_data_bits = UART_DATA_6_BITS; break;
        case 7: esp_data_bits = UART_DATA_7_BITS; break;
        case 8: esp_data_bits = UART_DATA_8_BITS; break;
        default: esp_data_bits = UART_DATA_8_BITS; break;
    }

    // --- اصلاح مقادیر Stop Bits ---
    // در ESP-IDF: 1 stop=0, 1.5 stop=1, 2 stop=2
    int esp_stop_bits = UART_STOP_BITS_1;
    switch (stopBits) {
        case 1: esp_stop_bits = UART_STOP_BITS_1; break;
        case 2: esp_stop_bits = UART_STOP_BITS_2; break;
        case 3: esp_stop_bits = UART_STOP_BITS_1_5; break; // اگر ورودی 1.5 باشد (معمولاً به صورت 3 یا float پاس داده می‌شود)
        default: esp_stop_bits = UART_STOP_BITS_1; break;
    }

    // --- اصلاح مقادیر Parity ---
    // در ESP-IDF: None=0, Odd=1, Even=2
    int esp_parity = UART_PARITY_DISABLE;
    switch (parity) {
        case 0: esp_parity = UART_PARITY_DISABLE; break;
        case 1: esp_parity = UART_PARITY_ODD; break;
        case 2: esp_parity = UART_PARITY_EVEN; break;
        default: esp_parity = UART_PARITY_DISABLE; break;
    }

    uart_config_t uart_config = {
        .baud_rate = baudRate,
        .data_bits = (uart_word_length_t)esp_data_bits,
        .parity = (uart_parity_t)esp_parity,
        .stop_bits = (uart_stop_bits_t)esp_stop_bits,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 122,
        .source_clk = UART_SCLK_APB,
    };

    // نصب درایور
    esp_err_t ret = uart_driver_install(uart_num_, 256, 0, 0, NULL, 0);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "UART%d install failed", uart_num_);
        return;
    }

    // تنظیم پارامترها
    ret = uart_param_config(uart_num_, &uart_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "UART%d config failed", uart_num_);
        return;
    }

    // تنظیم پین‌ها (با چک کردن اینکه پین‌ها معتبر باشند)
    if (txPin >= 0 && rxPin >= 0) {
        ret = uart_set_pin(uart_num_, txPin, rxPin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "UART%d pin set failed", uart_num_);
            return;
        }
    } else {
        ESP_LOGW(TAG, "UART%d pins not set (invalid pin numbers)", uart_num_);
    }

    is_initialized_ = true;
    ESP_LOGI(TAG, "UART%d initialized successfully", uart_num_);
}

bool SerialManagerUART::updateConfig(int baudRate, int dataBits, int stopBits, int parity) {
    if (!is_initialized_) return false;
    if (uart_set_baudrate(uart_num_, baudRate) != ESP_OK) return false;
    if (uart_set_word_length(uart_num_, (uart_word_length_t)dataBits) != ESP_OK) return false;
    if (uart_set_stop_bits(uart_num_, (uart_stop_bits_t)stopBits) != ESP_OK) return false;
    if (uart_set_parity(uart_num_, (uart_parity_t)parity) != ESP_OK) return false;
    ESP_LOGI(TAG, "UART%d Config Updated", uart_num_);
    return true;
}

void SerialManagerUART::sendRaw(const char* data, size_t len) {
    if (is_initialized_) uart_write_bytes(uart_num_, data, len);
}

void SerialManagerUART::checkUartRx() {
    if (!is_initialized_) return;
    uint8_t buf[128];
    size_t len = 0;
    // خواندن داده موجود در بافر UART
    len = uart_read_bytes(uart_num_, buf, sizeof(buf), 0); // Timeout = 0 (Non-blocking)
    if (len > 0) {
        // ارسال به کلاس پایه برای پردازش
        processRx(buf, len);
    }
}

// ==========================================
// پیاده‌سازی کلاس SerialManagerUSB
// ==========================================
SerialManagerUSB::SerialManagerUSB(SerialProtocol protocol) 
    : SerialManagerBase(protocol), is_initialized_(false) {}

void SerialManagerUSB::begin() {
    is_initialized_ = true;
    ESP_LOGI(TAG, "USB Serial Manager Ready");
}

void SerialManagerUSB::sendRaw(const char* data, size_t len) {
    if (!is_initialized_) return;
    tinyusb_cdcacm_write_queue((tinyusb_cdcacm_itf_t)0, (const uint8_t*)data, len);
    tinyusb_cdcacm_write_flush((tinyusb_cdcacm_itf_t)0, 0);
}

// Override برای فراخوانی مستقیم (اختیاری، چون Base هم همین کار را میکند)
void SerialManagerUSB::processRx(const uint8_t* buf, size_t len) {
    SerialManagerBase::processRx(buf, len);
}