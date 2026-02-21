#include "Keyboard.h"
#include <cstring>
#include "esp_log.h"

static const char *TAG = "KEYBOARD";

Keyboard::Keyboard() {
    event_queue = nullptr;
}

void Keyboard::init_gpio() {
    // سطرها ورودی با Pull-up
    for (int i = 0; i < 4; i++) {
        gpio_reset_pin(row_pins[i]);
        gpio_set_direction(row_pins[i], GPIO_MODE_INPUT);
        gpio_set_pull_mode(row_pins[i], GPIO_PULLUP_ONLY);
    }
    // ستون‌ها در ابتدا خروجی High (غیرفعال)
    for (int i = 0; i < 3; i++) {
        gpio_reset_pin(col_pins[i]);
        gpio_set_direction(col_pins[i], GPIO_MODE_OUTPUT);
        gpio_set_level(col_pins[i], 1);
    }
    // بازر
    gpio_reset_pin(buzzer_pin);
    gpio_set_direction(buzzer_pin, GPIO_MODE_OUTPUT);
}

void Keyboard::run_scan_logic() {
    // ۱. مدیریت بازر (غیرمسدودکننده مشابه کد شما)
    if (beez_time > 0) {
        gpio_set_level(buzzer_pin, 1);
        beez_time--;
    } else {
        gpio_set_level(buzzer_pin, 0);
    }

 
    // ۳. خواندن سطرها (مشابه key_read کد شما)
    uint8_t key_read = 0;
    if (gpio_get_level(row_pins[0]) == 1) key_read |= 0x01;
    if (gpio_get_level(row_pins[1]) == 1) key_read |= 0x02;
    if (gpio_get_level(row_pins[2]) == 1) key_read |= 0x04;
    if (gpio_get_level(row_pins[3]) == 1) key_read |= 0x08;

    if (key_read != 0) 
    {
        int key_n = 0;
        // تبدیل سطر به عدد (مشابه switch-case کد شما)
        if (key_read & 0x01) key_n = 0;
        else if (key_read & 0x02) key_n = 1;
        else if (key_read & 0x04) key_n = 2;
        else if (key_read & 0x08) key_n = 3;

        // محاسبه شماره کلید نهایی
        current_key = key_n + (col_counter * 4);
        if (key_zero_time==2)
		{
            if (current_key == last_sample) 
            {
                key_cnt++;
                if (key_cnt == 3) 
                { 
                    beez_time = 5; 
                    if (event_queue) 
                    {
                        // مقداردهی سطر و ستون بر اساس محاسبات موجود در کد
                        KeyEvent ev;
                        ev.row = key_n;             // همان شماره سطر (0 تا 3)
                        ev.col = col_counter;       // شماره ستون فعلی (0 تا 2)
                        ev.key_index = current_key; // شماره کلید کلی (0 تا 11)
                        ev.pressed = true;
                        xQueueSend(event_queue, &ev, 0);
                        key_cnt=0;
                        key_zero_time = 0 ;
                        last_sample   = 0 ;
                    }
                }
            } 
            else 
            {
                last_sample = current_key;
                key_cnt = 0;
            }
        }
        else{
            key_zero_time = 0 ;
        }
    } 
    else 
    {
        if((current_key&0xfc)==(col_counter*4))
            if(key_zero_time!=2) 
                key_zero_time = key_zero_time+1 ;
    }

    // ۲. انتخاب ستون فعلی (مثل Key_Column_Counter)
    gpio_set_level(col_pins[col_counter], 0); 
    col_counter++;
    if (col_counter >= 3) col_counter = 0;
    // ۴. آزاد سازی ستون و رفتن به ستون بعدی برای دور بعد
    gpio_set_level(col_pins[col_counter], 1);
}

void Keyboard::init(QueueHandle_t queue) {
    event_queue = queue;
    init_gpio();
    xTaskCreate(taskWrapper, "KeyTask", 3072, this, 10, NULL);
}

void Keyboard::taskWrapper(void* p) {
    Keyboard* obj = (Keyboard*)p;
    while(1) {
        obj->run_scan_logic();
        vTaskDelay(pdMS_TO_TICKS(10)); // هر ۱۰ میلی‌ثانیه یک ستون اسکن می‌شود
    }
}
