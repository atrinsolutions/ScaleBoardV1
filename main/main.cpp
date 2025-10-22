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
#include "config.h"
#include "OTAWebServer.h"  // فایل ماژول OTA که ساخته‌ایم


// پارامترهایی که به تسک ارسال می‌شوند
struct TaskParams {
    Keyboard* kb;
    ADC_AD7191* adc;
    DisplayTM1640* display;
    PowerManager* power;
    StatusLED* led;
};

// تابع تسک اصلی
void main_loop_task(void* pvParameters) {
    TaskParams* params = (TaskParams*)pvParameters;

    Keyboard* kb = params->kb;
    ADC_AD7191* adc = params->adc;
    DisplayTM1640* display = params->display;
    PowerManager* power = params->power;
    StatusLED* led = params->led;

    while (true) {
        kb->poll();
        double weight = adc->read();
        display->showNumber((int)weight, true);
        power->monitor();
        led->blink();
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

extern "C" void app_main(void)
{
    // Logger
    Logger::init();
    Logger::info("Scale Controller Starting...");

    // ماژول‌ها
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

    // --- شروع OTAWebServer ---
    OTAWebServer otaServer(
        OTAMode::DIRECT_AP,   // پیش‌فرض: گوشی مستقیم به ESP وصل شود
        "MobileSSID",         // SSID اینترنت گوشی
        "MobilePassword",     // Password اینترنت گوشی
        "ESP32_AP",           // AP داخلی ESP برای موبایل
        "12345678",           // پسورد AP
        "http://jahatpro.ir/posscale/ps.bin" // URL فایل فریمور
    );
    otaServer.begin();

    // آماده کردن پارامترها برای تسک اصلی
    static TaskParams params { &kb, &adc, &display, &power, &led };

    // ایجاد تسک اصلی
    xTaskCreate(
        main_loop_task,   // تابع تسک
        "main_loop",      // نام تسک
        8*1024,           // اندازه استک
        &params,          // پارامترها
        5,                // اولویت
        NULL              // handle (نیاز نداریم)
    );
}
