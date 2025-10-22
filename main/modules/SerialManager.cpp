#include "SerialManager.h"
#include "esp_log.h"
#include <cstring>

static const char* TAG = "SerialManager";

void SerialManager::init(){
    // basic UART0 is used by ESP-IDF console; user may configure further
    esp_log_level_set(TAG, ESP_LOG_INFO);
    ESP_LOGI(TAG, "SerialManager init");
}

void SerialManager::send(const char* msg){
    if(!msg) return;
    size_t len = strlen(msg);
    uart_write_bytes(UART_USER_PORT, msg, len);
}
