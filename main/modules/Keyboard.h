#pragma once
#include "driver/gpio.h"
#include <array>
#include <functional>

class Keyboard {
public:
    using KeyHandler = std::function<void(uint8_t)>;
    Keyboard();
    void init();
    void poll();
    void setCallback(KeyHandler cb);
private:
    std::array<gpio_num_t,3> rows;
    std::array<gpio_num_t,4> cols;
    KeyHandler callback;
};
