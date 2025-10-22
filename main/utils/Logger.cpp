#include "Logger.h"
#include "esp_log.h"

static const char* TAG = "ScaleApp";

void Logger::init(){
    esp_log_level_set("*", ESP_LOG_INFO);
}

void Logger::info(const char* msg){
    ESP_LOGI(TAG, "%s", msg);
}

void Logger::error(const char* msg){
    ESP_LOGE(TAG, "%s", msg);
}
