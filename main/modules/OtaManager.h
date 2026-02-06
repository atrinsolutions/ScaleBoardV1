#pragma once

#include <string>
#include <atomic>
#include "esp_https_ota.h"
#include "esp_crt_bundle.h"
#include "esp_log.h"

/**
 * @brief کلاس مدیریت آپدیت OTA به صورت Thread-safe
 * این کلاس مسئولیت دانلود و رایت فریمور را در یک تسک جداگانه بر عهده دارد
 */
class OtaManager {
public:
    static OtaManager& getInstance() {
        static OtaManager instance;
        return instance;
    }

    // شروع عملیات آپدیت با استفاده از URL ذخیره شده در تنظیمات
    void startUpdate(const std::string& url);

    // متدهای عمومی برای خواندن وضعیت توسط وب‌سرور
    int getProgress() const { return _progress.load(); }
    bool isUpdating() const { return _isUpdating.load(); }
    std::string getLastError() const { return _lastError; }

private:
    OtaManager() : _progress(0), _isUpdating(false), _lastError("") {}
    
    // متد داخلی که در تسک FreeRTOS اجرا می‌شود
    static void otaTask(void* pvParameter);
    
    std::string _updateUrl;
    std::atomic<int> _progress;
    std::atomic<bool> _isUpdating;
    std::string _lastError;
    
    static const char* TAG;
};
