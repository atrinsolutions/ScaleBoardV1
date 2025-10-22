#include "StatusLED.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

void StatusLED::init(gpio_num_t pin) {
    ledPin = pin;
    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = (1ULL << ledPin);
    io_conf.mode = GPIO_MODE_OUTPUT;
    gpio_config(&io_conf);
    gpio_set_level(ledPin, 0);
}

void StatusLED::blink() {
    state = !state;
    gpio_set_level(ledPin, state ? 1 : 0);
}
