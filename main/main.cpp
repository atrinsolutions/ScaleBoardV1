#include "app_config.h"
#include "modules/SerialManager.h"
#include "modules/WiFiManager.h"
#include "modules/EthernetManager.h"
#include "modules/Keyboard.h"
#include "modules/DisplayTM1640.h"
#include "modules/ADC_AD7191.h"
#include "modules/PowerManager.h"
#include "modules/CashDrawer.h"
#include "modules/StatusLED.h"
#include "utils/Logger.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern "C" void app_main(void)
{
    Logger::init();
    Logger::info("Scale Controller Starting...");

    SerialManager serial;
    serial.init();

    WiFiManager wifi;
    wifi.init();

    EthernetManager eth;
    eth.init();

    Keyboard kb;
    kb.init();

    DisplayTM1640 display;
    display.init();

    ADC_AD7191 adc;
    adc.init();

    PowerManager power;
    power.init();

    CashDrawer drawer;
    drawer.init();

    StatusLED led;
    led.init();

    // simple tasks: create a periodic task for demonstration
    xTaskCreate([](void*){
        while(true){
            kb.poll();
            double weight = adc.read();
            display.showNumber((int)weight, true);
            power.monitor();
            led.blink();
            vTaskDelay(pdMS_TO_TICKS(200));
        }
    }, "main_loop", 8*1024, NULL, 5, NULL);
}
