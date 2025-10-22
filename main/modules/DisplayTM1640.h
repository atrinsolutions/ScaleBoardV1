#pragma once
#include <cstdint>
#include "driver/gpio.h"

class DisplayTM1640 {
public:
    DisplayTM1640(gpio_num_t dio = TM1640_DIO_PIN, gpio_num_t clk = TM1640_CLK_PIN);
    void init();
    void showNumber(int value, bool leadingZeros=false);
    void clear();
private:
    gpio_num_t dio_;
    gpio_num_t clk_;
    void start();
    void stop();
    void writeByte(uint8_t b);
};
