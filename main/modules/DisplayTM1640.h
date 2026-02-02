/* DisplayTM1640 stub */#pragma once
#include "driver/gpio.h"
#include <cstdint>

class DisplayTM1640 {
public:

    DisplayTM1640() : dio_(GPIO_NUM_16), clk_(GPIO_NUM_17) {} // سازنده پیش‌فرض
    DisplayTM1640(gpio_num_t dio, gpio_num_t clk);

    // راه‌اندازی نمایشگر
    void init();

    // نمایش عدد روی 4 رقم
    void showNumber(int value, bool leadingZeros = true);

    // پاک کردن نمایشگر
    void clear();

private:
    gpio_num_t dio_;
    gpio_num_t clk_;

    // شروع و پایان انتقال داده
    void start();
    void stop();

    // ارسال یک بایت به نمایشگر
    void writeByte(uint8_t b);
};
