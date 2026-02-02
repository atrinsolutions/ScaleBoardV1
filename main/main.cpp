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
#include "OTAWebServer.h"
#include "nvs_flash.h"

// --- کتابخانه‌های مورد نیاز برای USB و شبکه ---
#include "tinyusb.h"
#include "tinyusb_cdc_acm.h"
#include "tinyusb_default_config.h"
#include "esp_netif.h"

// --- نمونه‌های سراسری ---
OTAWebServer* myServer = nullptr;
ADC_AD7191* adc = nullptr;
SerialManagerUSB* usbSerial = nullptr; // ماژول USB
SerialManagerUART* uartSerial = nullptr; // ماژول UART (سریال دوم)

// پارامترهایی که به تسک ارسال می‌شوند
struct TaskParams {
    Keyboard* kb;
    ADC_AD7191* adc;
    DisplayTM1640* display;
    PowerManager* power;
    StatusLED* led;
    SerialManagerUART* uart; // اضافه شدن سریال به پارامترهای تسک
};

// تابع تسک اصلی
void main_loop_task(void* pvParameters) {
    TaskParams* params = (TaskParams*)pvParameters;
    Keyboard* kb = params->kb;
    ADC_AD7191* adc = params->adc;
    DisplayTM1640* display = params->display;
    PowerManager* power = params->power;
    StatusLED* led = params->led;
    SerialManagerUART* uart = params->uart;

    while (true) {
        kb->poll();
        
        // خواندن وزن
        double weight = adc->read();
        display->showNumber((int)weight, true);
        
        power->monitor();
        led->blink();

        // بررسی داده‌های دریافتی روی پورت سریال سخت‌افزاری (UART)
        if (uart != nullptr) {
            uart->checkUartRx();
        }
        
        // تاخیر ۲۰۰ میلی‌ثانیه
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

// --- Callback دریافت داده از USB (CDC) ---
void tinyusb_cdc_rx_callback(int itf, cdcacm_event_t *event)
{
    size_t rx_size = 0;
    uint8_t temp_buf[CONFIG_TINYUSB_CDC_RX_BUFSIZE + 1]; 
    
    // خواندن داده از درایور TinyUSB
    esp_err_t ret = tinyusb_cdcacm_read((tinyusb_cdcacm_itf_t)itf, temp_buf, CONFIG_TINYUSB_CDC_RX_BUFSIZE, &rx_size);
    
    if (ret == ESP_OK && rx_size > 0) {
        // ارسال داده به کلاس SerialManagerUSB برای پردازش
        if (usbSerial != nullptr) {
            usbSerial->processRx(temp_buf, rx_size);
        }
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

    // 3. راه‌اندازی اولیه GPIO
    gpio_set_direction(GPIO_NUM_16, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_NUM_16, 1);
    vTaskDelay(500 / portTICK_PERIOD_MS); 

    // 4. راه‌اندازی ADC
    adc = new ADC_AD7191();
    adc->init();

    // 5. راه‌اندازی USB Serial (TinyUSB)
    Logger::info("Initializing USB CDC...");
    const tinyusb_config_t tusb_cfg = TINYUSB_DEFAULT_CONFIG();
    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));

    tinyusb_config_cdcacm_t acm_cfg = {
        .cdc_port = TINYUSB_CDC_ACM_0,
        .callback_rx = &tinyusb_cdc_rx_callback,
        .callback_rx_wanted_char = NULL,
        .callback_line_state_changed = NULL,
        .callback_line_coding_changed = NULL
    };
    ESP_ERROR_CHECK(tinyusb_cdcacm_init(&acm_cfg));
    
    usbSerial = new SerialManagerUSB(SerialProtocol::PROTOCOL_PAND);
    usbSerial->begin();
    usbSerial->setAdcInstance(adc);
    Logger::info("USB CDC Ready.");

    // 5.1 راه‌اندازی UART Serial (سریال دوم)
    // پین‌ها را بر اساس سخت‌افزار خود تنظیم کنید (مثلاً TX=4, RX=5)
    Logger::info("Initializing UART Serial...");
    uartSerial = new SerialManagerUART(UART_NUM_1, SerialProtocol::PROTOCOL_PAND);
    uartSerial->begin(115200, 4, 5); // Baud=115200, TX=4, RX=5
    uartSerial->setAdcInstance(adc);
    Logger::info("UART Serial Ready.");

    // 6. راه‌اندازی OTAWebServer
    myServer = new OTAWebServer(
        OTAMode::HYBRID,             
        "Takin",                     
        "takinarka",                 
        "ESP32_AP",                  
        "12345678",                  
        "http://jahatpro.ir/posscale/ps.bin", 
        "admin",                     
        "admin"                      
    );
    
    myServer->setAdcInstance(adc);
    myServer->begin();

    // 7. راه‌اندازی سایر ماژول‌ها
    // Keyboard* kb = new Keyboard();
    // DisplayTM1640* display = new DisplayTM1640();
    // PowerManager* power = new PowerManager();
    // StatusLED* led = new StatusLED();

    // // آماده‌سازی پارامترها برای تسک
    // TaskParams params = {
    //     .kb = kb,
    //     .adc = adc,
    //     .display = display,
    //     .power = power,
    //     .led = led,
    //     .uart = uartSerial // اضافه کردن سریال به پارامترها
    // };

    // // 8. ساخت و شروع تسک اصلی
    // xTaskCreate(main_loop_task, "MainLoop", 4096, &params, 5, NULL);

    Logger::info("System Started Successfully.");
}