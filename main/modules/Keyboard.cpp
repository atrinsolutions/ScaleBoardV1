#include "Keyboard.h"
#include "app_config.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "Keyboard";

Keyboard::Keyboard(){
    rows = { GPIO_NUM_21, GPIO_NUM_22, GPIO_NUM_23 };
    cols = { GPIO_NUM_25, GPIO_NUM_26, GPIO_NUM_27, GPIO_NUM_14 };
}

void Keyboard::init(){
    for(auto r: rows){
        gpio_set_direction(r, GPIO_MODE_OUTPUT);
        gpio_set_level(r, 1);
    }
    for(auto c: cols){
        gpio_set_direction(c, GPIO_MODE_INPUT);
        gpio_set_pull_mode(c, GPIO_PULLUP_ONLY);
    }
    ESP_LOGI(TAG, "Keyboard initialized");
}

void Keyboard::poll(){
    for(size_t r=0;r<rows.size();++r){
        // drive only current row low
        for(auto rr: rows) gpio_set_level(rr, 1);
        gpio_set_level(rows[r], 0);
        vTaskDelay(pdMS_TO_TICKS(5));
        for(size_t c=0;c<cols.size();++c){
            int v = gpio_get_level(cols[c]);
            if(v==0){
                uint8_t key = r*cols.size() + c + 1;
                ESP_LOGI(TAG, "Key pressed: %d", key);
                if(callback) callback(key);
                vTaskDelay(pdMS_TO_TICKS(120)); // debounce
            }
        }
    }
}

void Keyboard::setCallback(KeyHandler cb){
    callback = cb;
}
