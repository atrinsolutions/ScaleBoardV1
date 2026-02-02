#include "SerialManager.h"
#include <cstring>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <sstream>
#include "esp_log.h"
#include "esp_netif.h"
#include "tinyusb.h"
#include "tinyusb_cdc_acm.h"
#include "tinyusb_default_config.h"
#include "OTAWebServer.h" 


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

// --- فرمت‌ها ---
std::string SerialManagerBase::formatPand(const WeightData& data) {
    char buf[64];
    char sign = data.isNegative() ? '-' : '+';
    snprintf(buf, sizeof(buf), "ST,GS,%c %7.3f kg\r\n", sign, data.getWeight());
    return std::string(buf);
}
std::string SerialManagerBase::formatTakinArka1(const WeightData& data) {
    char buf[64]; char sign = data.isNegative() ? '-' : '+';
    snprintf(buf, sizeof(buf), "W=%c%07.3f\r\n", sign, data.getWeight()); return std::string(buf);
}
std::string SerialManagerBase::formatTakinArka2(const WeightData& data) {
    char buf[64]; char sign = data.isNegative() ? '-' : '+';
    snprintf(buf, sizeof(buf), "%c%08.3f\r\n", sign, data.getWeight()); return std::string(buf);
}
std::string SerialManagerBase::formatTozinSadr(const WeightData& data) {
    char buf[64]; char sign = data.isNegative() ? '-' : '+';
    snprintf(buf, sizeof(buf), "WT %c%07.3f KG\r\n", sign, data.getWeight()); return std::string(buf);
}
std::string SerialManagerBase::formatRadin(const WeightData& data) {
    char buf[64]; char sign = data.isNegative() ? '-' : '+';
    snprintf(buf, sizeof(buf), "NET %c%07.2f\r\n", sign, data.getWeight()); return std::string(buf);
}
std::string SerialManagerBase::formatMahak(const WeightData& data) {
    char buf[64]; char sign = data.isNegative() ? '-' : '+';
    snprintf(buf, sizeof(buf), "GS, %c%07.3f\r\n", sign, data.getWeight()); return std::string(buf);
}
std::string SerialManagerBase::formatKaraTozin(const WeightData& data) {
    char buf[64]; char sign = data.isNegative() ? '-' : '+';
    snprintf(buf, sizeof(buf), "W=%c%07.3f KG\r\n", sign, data.getWeight()); return std::string(buf);
}
std::string SerialManagerBase::formatServina(const WeightData& data) {
    char buf[64]; char sign = data.isNegative() ? '-' : '+';
    snprintf(buf, sizeof(buf), "ST,GS,%c%07.3f kg\r\n", sign, data.getWeight()); return std::string(buf);
}

// --- مدیریت دستورات ---
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
    else if (cmd == "GET_CONFIG") {
        if (myServer != nullptr) {
            ESP_LOGI(TAG, "Command: GET_CONFIG");
            std::string ssid = myServer->getWiFiSSID();
            std::string wifi_pass = myServer->getWiFiPass();
            std::string ap_ssid = myServer->getAPSSID();
            std::string ap_pass = myServer->getAPPass();
            std::string fw_url = myServer->getFwUrl();
            std::string web_user = myServer->getWebUser();
            std::string web_pass = myServer->getWebPass();
            int mode_val = static_cast<int>(myServer->getMode());
            char buffer[512];
            int len = snprintf(buffer, sizeof(buffer), 
                "CONFIG %s %s %s %s %s %s %s %d\r\n", 
                ssid.c_str(), wifi_pass.c_str(), ap_ssid.c_str(), ap_pass.c_str(), 
                fw_url.c_str(), web_user.c_str(), web_pass.c_str(), mode_val);
            sendRaw(buffer, len);
        } else {
            sendRaw("ERROR: Server not ready\r\n", 26);
        }
    }
    else if (cmd.rfind("SET_CONFIG ", 0) == 0) {
        std::string params = cmd.substr(11);
        std::stringstream ss(params);
        std::string ssid, wifi_pass, ap_ssid, ap_pass, fw_url, web_user, web_pass;
        int mode_val = 2;
        if (ss >> ssid >> wifi_pass >> ap_ssid >> ap_pass >> fw_url >> web_user >> web_pass >> mode_val) {
            if (myServer != nullptr) {
                ESP_LOGI(TAG, "Updating Full Config...");
                myServer->setWiFiCredentials(ssid, wifi_pass);
                myServer->setAPCredentials(ap_ssid, ap_pass); 
                myServer->setFwUrl(fw_url);
                myServer->setWebCredentials(web_user, web_pass);
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
// پیاده‌سازی SerialManagerUART (با Callback)
// ==========================================
SerialManagerUART::SerialManagerUART(uart_port_t uart_num, SerialProtocol protocol) 
    : SerialManagerBase(protocol), uart_num_(uart_num), is_initialized_(false), uart_task_handle_(nullptr), uart_queue_(nullptr) {}

SerialManagerUART::~SerialManagerUART() {
    if (uart_task_handle_ != nullptr) {
        vTaskDelete(uart_task_handle_);
    }
    if (uart_queue_ != nullptr) {
        vQueueDelete(uart_queue_);
    }
    if (is_initialized_) {
        uart_driver_delete(uart_num_);
    }
}

void SerialManagerUART::init(int baudRate, int txPin, int rxPin, int dataBits, int stopBits, int parity) {
    // تبدیل پارامترها به فرمت ESP-IDF
    int esp_data_bits = UART_DATA_8_BITS;
    switch (dataBits) {
        case 5: esp_data_bits = UART_DATA_5_BITS; break;
        case 6: esp_data_bits = UART_DATA_6_BITS; break;
        case 7: esp_data_bits = UART_DATA_7_BITS; break;
        case 8: esp_data_bits = UART_DATA_8_BITS; break;
    }
    int esp_stop_bits = UART_STOP_BITS_1;
    switch (stopBits) {
        case 1: esp_stop_bits = UART_STOP_BITS_1; break;
        case 2: esp_stop_bits = UART_STOP_BITS_2; break;
        case 3: esp_stop_bits = UART_STOP_BITS_1_5; break;
    }
    int esp_parity = UART_PARITY_DISABLE;
    switch (parity) {
        case 0: esp_parity = UART_PARITY_DISABLE; break;
        case 1: esp_parity = UART_PARITY_ODD; break;
        case 2: esp_parity = UART_PARITY_EVEN; break;
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

    // 1. نصب درایور با پشتیبانی از صف رویداد
    esp_err_t ret = uart_driver_install(uart_num_, 1024, 1024, 20, &uart_queue_, 0);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "UART%d install failed", uart_num_);
        return;
    }
    
    ret = uart_param_config(uart_num_, &uart_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "UART%d config failed", uart_num_);
        return;
    }
    
    ret = uart_set_pin(uart_num_, txPin, rxPin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "UART%d pin set failed", uart_num_);
        return;
    }

    // 2. ایجاد تسک برای پردازش رویدادها
    xTaskCreate(uart_event_task_wrapper, "uart_event_task", 4096, this, 12, &uart_task_handle_);

    is_initialized_ = true;
    ESP_LOGI(TAG, "UART%d initialized with Event Task", uart_num_);
}

// Wrapper برای استفاده در xTaskCreate
void SerialManagerUART::uart_event_task_wrapper(void* pvParameters) {
    SerialManagerUART* manager = static_cast<SerialManagerUART*>(pvParameters);
    manager->uart_event_task_impl();
}

// پیاده‌سازی واقعی تسک
void SerialManagerUART::uart_event_task_impl() {
    uart_event_t event;
    uint8_t* data = (uint8_t*) malloc(1024 + 1);

    for (;;) {
        // منتظر ماندن برای رویداد
        if (xQueueReceive(uart_queue_, (void *)&event, portMAX_DELAY)) {
            bzero(data, 1024 + 1); // پاک کردن بافر
            
            switch (event.type) {
                case UART_DATA: {
                    // استفاده از آکولاد برای تعریف متغیر در داخل case
                    int len = uart_read_bytes(uart_num_, data, event.size, 100 / portTICK_PERIOD_MS);
                    if (len > 0) {
                        processRx(data, len);
                    }
                    break;
                }
                
                case UART_FIFO_OVF:
                    ESP_LOGI(TAG, "hw fifo overflow");
                    uart_flush_input(uart_num_);
                    xQueueReset(uart_queue_);
                    break;
                    
                case UART_BUFFER_FULL:
                    ESP_LOGI(TAG, "ring buffer full");
                    uart_flush_input(uart_num_);
                    xQueueReset(uart_queue_);
                    break;
                
                case UART_PARITY_ERR:
                    ESP_LOGI(TAG, "uart parity error");
                    uart_flush_input(uart_num_);
                    break;

                case UART_FRAME_ERR:
                    ESP_LOGI(TAG, "uart frame error");
                    uart_flush_input(uart_num_);
                    break;
                    
                default:
                    // مدیریت سایر رویدادها برای جلوگیری از Warning
                    break;
            }
        }
    }
    free(data);
    vTaskDelete(NULL);
}

void SerialManagerUART::checkUartRx() {
    // این متد دیگر در حالت Callback نیاز نیست، اما برای سازگاری باقی می‌ماند
    if (!is_initialized_) return;
    uint8_t buf[128];
    size_t len = uart_read_bytes(uart_num_, buf, sizeof(buf), 0);
    if (len > 0) processRx(buf, len);
}

void SerialManagerUART::sendRaw(const char* data, size_t len) {
    if (is_initialized_) uart_write_bytes(uart_num_, data, len);
}

// ==========================================
// پیاده‌سازی SerialManagerUSB
// ==========================================
SerialManagerUSB::SerialManagerUSB(SerialProtocol protocol) 
    : SerialManagerBase(protocol), is_initialized_(false) {}

// اشاره‌گر استاتیک برای دسترسی در Callback C
static SerialManagerUSB* usb_instance = nullptr;

void SerialManagerUSB::init() {
    // این متغیر استاتیک تضمین می‌کند که درایور TinyUSB فقط یک بار در کل برنامه نصب شود
    static bool driver_installed = false;

    // 1. نصب درایور اصلی TinyUSB (فقط برای بار اول)
    if (!driver_installed) {
        const tinyusb_config_t tusb_cfg = TINYUSB_DEFAULT_CONFIG();
        esp_err_t ret = tinyusb_driver_install(&tusb_cfg);
        
        if (ret == ESP_OK) {
            driver_installed = true;
            ESP_LOGI(TAG, "TinyUSB Driver Installed successfully.");
        } else if (ret == ESP_ERR_INVALID_STATE) {
            // اگر درایور قبلاً توسط سیستم (Bootloader) نصب شده باشد، خطا نمی‌دهیم
            ESP_LOGW(TAG, "TinyUSB Driver already installed by system.");
            driver_installed = true; 
        } else {
            ESP_LOGE(TAG, "Failed to install TinyUSB driver: %s", esp_err_to_name(ret));
            return;
        }
    }

    // 2. راه‌اندازی پورت CDC ACM
    tinyusb_config_cdcacm_t acm_cfg = {
        .cdc_port = TINYUSB_CDC_ACM_0,
        .callback_rx = &tinyusb_cdc_rx_callback_wrapper,
        .callback_rx_wanted_char = NULL,
        .callback_line_state_changed = NULL,
        .callback_line_coding_changed = NULL
    };
    
    esp_err_t ret = tinyusb_cdcacm_init(&acm_cfg);
    
    if (ret == ESP_ERR_INVALID_STATE) {
        // اگر پورت قبلاً راه‌اندازی شده باشد (مثلاً در اجراهای قبلی یا توسط کانفیگ)،
        // خطا نمی‌دهیم و ادامه می‌دهیم.
        ESP_LOGW(TAG, "CDC ACM already initialized. Skipping re-init.");
    } else if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init CDC ACM: %s", esp_err_to_name(ret));
        return;
    }
    
    // ذخیره اینستنس برای استفاده در Callback
    usb_instance = this;
    
    is_initialized_ = true;
    ESP_LOGI(TAG, "USB CDC Ready");
}

// Callback Wrapper
void SerialManagerUSB::tinyusb_cdc_rx_callback_wrapper(int itf, cdcacm_event_t *event) {
    if (usb_instance != nullptr) {
        size_t rx_size = 0;
        uint8_t temp_buf[CONFIG_TINYUSB_CDC_RX_BUFSIZE + 1]; 
        esp_err_t ret = tinyusb_cdcacm_read((tinyusb_cdcacm_itf_t)itf, temp_buf, CONFIG_TINYUSB_CDC_RX_BUFSIZE, &rx_size);
        
        if (ret == ESP_OK && rx_size > 0) {
            usb_instance->processRx(temp_buf, rx_size);
        }
    }
}

void SerialManagerUSB::sendRaw(const char* data, size_t len) {
    if (is_initialized_) {
        tinyusb_cdcacm_write_queue((tinyusb_cdcacm_itf_t)0, (const uint8_t*)data, len);
        tinyusb_cdcacm_write_flush((tinyusb_cdcacm_itf_t)0, 0);
    }
}