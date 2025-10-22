#include "ADC_AD7191.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
static const char* TAG = "AD7191";

ADC_AD7191::ADC_AD7191(){}

void ADC_AD7191::init(){
    ESP_LOGI(TAG, "AD7191 init (stub) - replace with SPI driver");
}

double ADC_AD7191::read(){
    // stub: return a simulated increasing value
    static double v = 0;
    v += 0.1;
    if(v > 9999) v = 0;
    vTaskDelay(pdMS_TO_TICKS(1));
    return v;
}
