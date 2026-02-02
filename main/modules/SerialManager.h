#ifndef SERIAL_MANAGER_H
#define SERIAL_MANAGER_H

#include <string>
#include <cstdint>
#include <vector>
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_netif.h"
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
    
    // اشاره‌گر به ADC
    ADC_AD7191* adc_; 
    
    // بافر مشترک برای دریافت داده‌ها
    std::vector<uint8_t> rx_buffer_;

public:
    explicit SerialManagerBase(SerialProtocol protocol);
    virtual ~SerialManagerBase() = default;
    void setProtocol(SerialProtocol protocol);
    
    // متد تزریق ADC
    void setAdcInstance(ADC_AD7191* adc);
    
    // --- متد اصلی ارسال ---
    void sendWeight();
    
    // --- متد پردازش داده دریافتی (مشترک) ---
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
    
    // توابع مدیریت دستورات (مشترک)
    void handleCommand(const std::string& cmd);
    void sendIP();
};

// --- کلاس فرزند UART ---
class SerialManagerUART : public SerialManagerBase {
private:
    uart_port_t uart_num_;
    bool is_initialized_;
public:
    SerialManagerUART(uart_port_t uart_num, SerialProtocol protocol = SerialProtocol::PROTOCOL_PAND);
    ~SerialManagerUART() override;
    void begin(int baudRate, int txPin, int rxPin, int dataBits = 8, int stopBits = 1, int parity = 0);
    bool updateConfig(int baudRate, int dataBits, int stopBits, int parity);
    
    // متد برای خواندن از UART و پاس دادن به Base
    void checkUartRx();
    
protected:
    void sendRaw(const char* data, size_t len) override;
};

// --- کلاس فرزند USB ---
class SerialManagerUSB : public SerialManagerBase {
private:
    bool is_initialized_;
    
public:
    explicit SerialManagerUSB(SerialProtocol protocol = SerialProtocol::PROTOCOL_PAND);
    void begin();
    
    // این متد باید از Callback TinyUSB صدا زده شود
    void processRx(const uint8_t* buf, size_t len); // Override برای فراخوانی مستقیم اگر نیاز شد
    
protected:
    void sendRaw(const char* data, size_t len) override;
};

#endif // SERIAL_MANAGER_H