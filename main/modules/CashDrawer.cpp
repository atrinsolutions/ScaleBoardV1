#include "CashDrawer.h"
#include "app_config.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "CashDrawer";

void CashDrawer::init(){
    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = (1ULL << CASH_DRAWER_PIN);
    io_conf.mode = GPIO_MODE_OUTPUT;
    gpio_config(&io_conf);
    gpio_set_level(CASH_DRAWER_PIN, 0);
    ESP_LOGI(TAG, "CashDrawer init");
}

void CashDrawer::open(){
    ESP_LOGI(TAG, "Opening cash drawer");
    gpio_set_level(CASH_DRAWER_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(200));
    gpio_set_level(CASH_DRAWER_PIN, 0);
}
