#pragma once
#include "driver/gpio.h"
#include "config.h"


class StatusLED {
public:
    void init(gpio_num_t pin = STATUS_LED_PIN);
    void blink();
private:
    gpio_num_t ledPin;
    bool state = false;
};
