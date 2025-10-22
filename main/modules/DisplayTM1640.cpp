#include "DisplayTM1640.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

static const char* TAG = "TM1640";
static const uint8_t segmap[] = {0x3f,0x06,0x5b,0x4f,0x66,0x6d,0x7d,0x07,0x7f,0x6f};

DisplayTM1640::DisplayTM1640(gpio_num_t dio, gpio_num_t clk) : dio_(dio), clk_(clk) {}

void DisplayTM1640::init(){
    gpio_set_direction(dio_, GPIO_MODE_OUTPUT);
    gpio_set_direction(clk_, GPIO_MODE_OUTPUT);
    clear();
    ESP_LOGI(TAG, "TM1640 init on DIO=%d CLK=%d", dio_, clk_);
}

void DisplayTM1640::start(){ gpio_set_level(dio_, 1); gpio_set_level(clk_, 1); gpio_set_level(dio_, 0); gpio_set_level(clk_, 0); }
void DisplayTM1640::stop(){ gpio_set_level(dio_, 0); gpio_set_level(clk_, 1); gpio_set_level(dio_, 1); }

void DisplayTM1640::writeByte(uint8_t b){
    for(int i=0;i<8;i++){
        gpio_set_level(clk_, 0);
        gpio_set_level(dio_, (b>>i)&1);
        gpio_set_level(clk_, 1);
    }
    // ack bit handling omitted for simplicity
    gpio_set_level(clk_, 0);
    gpio_set_direction(dio_, GPIO_MODE_INPUT);
    vTaskDelay(pdMS_TO_TICKS(1));
    gpio_set_direction(dio_, GPIO_MODE_OUTPUT);
}

void DisplayTM1640::showNumber(int value, bool leadingZeros){
    if(value<0) value = 0;
    if(value>9999) value = 9999;
    uint8_t digits[4];
    for(int i=3;i>=0;i--){
        digits[i] = segmap[value%10];
        value /= 10;
    }
    // data command
    start(); writeByte(0x40); stop();
    for(int i=0;i<4;i++){
        start();
        writeByte(0xC0 | i);
        writeByte(digits[i]);
        stop();
    }
    start(); writeByte(0x88 | 7); stop(); // display control max brightness
}

void DisplayTM1640::clear(){
    for(int i=0;i<4;i++){
        start(); writeByte(0xC0 | i); writeByte(0x00); stop();
    }
}
