#include "PowerManager.h"
#include "app_config.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "PowerMgr";

void PowerManager::init(){
    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = (1ULL << UPS_CTRL_PIN);
    io_conf.mode = GPIO_MODE_OUTPUT;
    gpio_config(&io_conf);
    gpio_set_level(UPS_CTRL_PIN, 1); // enable loads
    ESP_LOGI(TAG, "PowerManager init");
}

void PowerManager::monitor(){
    // stub: in real project sample ADC or AD7191 to measure battery.
    // Here we just log periodically.
    static int cnt = 0;
    cnt++;
    if((cnt % 50) == 0){
        ESP_LOGI(TAG, "PowerManager monitor tick");
    }
    vTaskDelay(pdMS_TO_TICKS(1));
}
