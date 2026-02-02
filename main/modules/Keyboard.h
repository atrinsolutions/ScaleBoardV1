#pragma once
#include <vector>
#include <functional>
#include "driver/gpio.h"

using KeyHandler = std::function<void(uint8_t)>;

class Keyboard {
public:
    Keyboard();

    // راه‌اندازی کیبورد
    void init();

    // خواندن وضعیت کلیدها (poll)
    void poll();

    // ثبت callback هنگام فشار کلید
    void setCallback(KeyHandler cb);

private:
    std::vector<gpio_num_t> rows;  // پین‌های ردیف
    std::vector<gpio_num_t> cols;  // پین‌های ستون
    KeyHandler callback = nullptr;  // تابع فراخوان هنگام فشار کلید
};
