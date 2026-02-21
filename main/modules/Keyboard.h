#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"

struct KeyEvent {
    int row;       // اضافه شد
    int col;       // اضافه شد
    int key_index; // برای سازگاری با منطق داخلی
    bool pressed;
};


class Keyboard {
public:
    Keyboard();
    void init(QueueHandle_t queue = nullptr);
    static void taskWrapper(void* pvParameters);

private:
    gpio_num_t row_pins[4] = {GPIO_NUM_47, GPIO_NUM_48, GPIO_NUM_45, GPIO_NUM_36};
    gpio_num_t col_pins[3] = {GPIO_NUM_39, GPIO_NUM_38, GPIO_NUM_37};
    gpio_num_t buzzer_pin = GPIO_NUM_8;

    QueueHandle_t event_queue;
    
    // متغیرهای الهام گرفته شده از کد شما
    uint8_t col_counter = 0;
    int last_sample = -1;
    int key_cnt = 0;
    int beez_time = 0;
    int key_zero_time=0;
    int current_key=0;

    void init_gpio();
    void run_scan_logic(); // منطق اصلی استخراج شده از کد سی
};

#endif
