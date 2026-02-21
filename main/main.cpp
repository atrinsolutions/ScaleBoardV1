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
#include "freertos/queue.h"
#include "config.h"
#include "WebServer.h"
#include "nvs_flash.h"

// --- کتابخانه‌های مورد نیاز برای USB و شبکه ---
#include "tinyusb.h"
#include "tinyusb_cdc_acm.h"
#include "tinyusb_default_config.h"
#include "esp_netif.h"

// --- نمونه‌های سراسری ---
WebServer* myServer = nullptr;
ADC_AD7191* adc = nullptr;
SerialManagerUSB* usbSerial = nullptr;
SerialManagerUART* uartSerial = nullptr;
EthernetManager* ethManager = nullptr; 
Keyboard* kb = nullptr; 

// صف برای دریافت رویدادهای کیبورد
QueueHandle_t keyboard_queue = nullptr;

// پارامترهایی که به تسک اصلی ارسال می‌شوند
struct TaskParams {
    Keyboard* kb;
    ADC_AD7191* adc;
    DisplayTM1640* display;
    PowerManager* power;
    StatusLED* led;
    SerialManagerUART* uart; 
};

// --- تابع تسک اصلی ---
void main_loop_task(void* pvParameters) {
    // تبدیل پارامترها به ساختار مربوطه
    TaskParams* params = (TaskParams*)pvParameters;
    
    ESP_LOGI("MainLoop", "Main Loop Task Started.");

    while (true) {
        // 1. بررسی داده‌های پورت سریال (UART)
        if (params->uart != nullptr) {
            params->uart->checkUartRx();
        }

        // 2. بررسی رویدادهای کیبورد از صف
        if (keyboard_queue != nullptr) {
            KeyEvent event;
            // خواندن تمام رویدادهای موجود در صف (Non-blocking)
            while (xQueueReceive(keyboard_queue, &event, 0) == pdTRUE) {
                if (event.pressed) {
                    // چاپ مختصات کلید فشرده شده
                    ESP_LOGI("MainLoop", "Key Code = %d", event.key_index);
                    }
            }
        }

        // 3. سایر پردازش‌های دوره ای
        // ...

        // تاخیر ۲۰ میلی‌ثانیه برای جلوگیری از اشغال کامل پردازنده
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

extern "C" void app_main(void)
{
    // 1. راه‌اندازی حافظه NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 2. راه‌اندازی Logger
    Logger::init();
    Logger::info("Scale Controller Starting...");

    // 3. راه‌اندازی اولیه GPIO (ریست اترنت)
    gpio_reset_pin(GPIO_NUM_16);
    gpio_set_direction(GPIO_NUM_16, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_NUM_16, 1);
    vTaskDelay(pdMS_TO_TICKS(100)); // تاخیر کوتاه برای پایداری

    // 4. راه‌اندازی ADC
    adc = new ADC_AD7191();
    adc->init();

    // 5. راه‌اندازی Serial (USB & UART)
    usbSerial = new SerialManagerUSB(SerialProtocol::PROTOCOL_PAND);
    usbSerial->init(); 
    usbSerial->setAdcInstance(adc);

    uartSerial = new SerialManagerUART(UART_NUM_1, SerialProtocol::PROTOCOL_PAND);
    uartSerial->init(115200, GPIO_NUM_4, GPIO_NUM_5); 
    uartSerial->setAdcInstance(adc);

    // 6. راه‌اندازی شبکه و وب‌سرور
    myServer = new WebServer(
        WIFIMode::HYBRID,             
        "MobinNet7665", "31477665",                 
        "ESP32_AP", "12345678",                  
        "http://jahatpro.ir/posscale/ps.bin", 
        "admin", "admin"                      
    );
    myServer->setAdcInstance(adc);
    myServer->begin();

    // 7. راه‌اندازی Ethernet
    ethManager = new EthernetManager();
    ethManager->start();

    // 8. راه‌اندازی کیبورد (بسیار مهم: ابتدا صف، سپس کیبورد)
    keyboard_queue = xQueueCreate(10, sizeof(KeyEvent));
    if (keyboard_queue == nullptr) {
        Logger::error("Failed to create keyboard queue!");
    }

    kb = new Keyboard();
    kb->init(keyboard_queue);

    // 9. آماده‌سازی پارامترها به صورت static (ماندگار در حافظه)
    static TaskParams params; 
    params.kb = kb;
    params.adc = adc;
    params.display = nullptr; 
    params.power = nullptr;   
    params.led = nullptr;     
    params.uart = uartSerial;

    // 10. ساخت تسک اصلی با اولویت ۵
    xTaskCreate(main_loop_task, "MainLoop", 4096, &params, 5, NULL);

    Logger::info("System Started Successfully.");
}
