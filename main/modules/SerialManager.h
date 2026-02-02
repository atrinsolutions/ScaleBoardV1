#ifndef SERIAL_MANAGER_H
#define SERIAL_MANAGER_H

#include <string>
#include <cstdint>
#include <vector>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "tusb_cdc_acm.h"
#include "ADC_AD7191.h" 

// --- تعریف پروتکل‌ها ---
enum class SerialProtocol : uint8_t {
    PROTOCOL_PAND,
    PROTOCOL_TAKIN_ARKA_1,
    PROTOCOL_TAKIN_ARKA_2,
    PROTOCOL_TOZIN_SADR,
    PROTOCOL_RADIN,
    PROTOCOL_MAHAK,
    PROTOCOL_KARA_TOZIN,
    PROTOCOL_SERVINA
};

// --- کلاس پایه ---
class SerialManagerBase {
protected:
    SerialProtocol protocol_;
    static const char* TAG;
    ADC_AD7191* adc_; 
    std::vector<uint8_t> rx_buffer_;

public:
    explicit SerialManagerBase(SerialProtocol protocol);
    virtual ~SerialManagerBase() = default;
    
    void setProtocol(SerialProtocol protocol);
    void setAdcInstance(ADC_AD7191* adc);
    void sendWeight();
    
    // متد عمومی برای پردازش داده (هم توسط USB و هم UART استفاده می‌شود)
    void processRx(const uint8_t* buf, size_t len);

protected:
    virtual void sendRaw(const char* data, size_t len) = 0;
    
    // توابع فرمت‌دهی
    std::string formatPand(const WeightData& data);
    std::string formatTakinArka1(const WeightData& data);
    std::string formatTakinArka2(const WeightData& data);
    std::string formatTozinSadr(const WeightData& data);
    std::string formatRadin(const WeightData& data);
    std::string formatMahak(const WeightData& data);
    std::string formatKaraTozin(const WeightData& data);
    std::string formatServina(const WeightData& data);
    
    // توابع مدیریت دستورات
    void handleCommand(const std::string& cmd);
    void sendIP();
};

// --- کلاس فرزند UART ---
class SerialManagerUART : public SerialManagerBase {
private:
    uart_port_t uart_num_;
    bool is_initialized_;
    TaskHandle_t uart_task_handle_;
    QueueHandle_t uart_queue_;

public:
    SerialManagerUART(uart_port_t uart_num, SerialProtocol protocol = SerialProtocol::PROTOCOL_PAND);
    ~SerialManagerUART() override;

    // متد init برای راه‌اندازی کامل (درایور + تسک)
    void init(int baudRate, int txPin, int rxPin, int dataBits = 8, int stopBits = 1, int parity = 0);
    
    // متد کمکی برای خواندن (اگر نیاز به Polling دستی بود)
    void checkUartRx(); 

protected:
    void sendRaw(const char* data, size_t len) override;
    
    // تسک داخلی برای مدیریت رویدادهای UART
    static void uart_event_task_wrapper(void* pvParameters);
    void uart_event_task_impl();
};

// --- کلاس فرزند USB ---
class SerialManagerUSB : public SerialManagerBase {
private:
    bool is_initialized_;

public:
    explicit SerialManagerUSB(SerialProtocol protocol = SerialProtocol::PROTOCOL_PAND);
    
    // متد init برای راه‌اندازی TinyUSB
    void init();
    
    // متد استاتیک برای Callback (چون تابع C نیاز دارد)
    static void tinyusb_cdc_rx_callback_wrapper(int itf, cdcacm_event_t *event);

protected:
    void sendRaw(const char* data, size_t len) override;
};

#endif // SERIAL_MANAGER_H