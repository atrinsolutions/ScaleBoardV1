#include "ADC_AD7191.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <cmath>    

static const char* TAG = "ADC_AD7191";
static const char* NVS_NAMESPACE = "adc_calib";

// --- پیاده‌سازی سازنده ---
ADC_AD7191::ADC_AD7191(gpio_num_t pinSck, gpio_num_t pinDataInt, gpio_num_t pinPwdn, gpio_num_t pinA0)
    // 1. متغیرهای اشتراکی
    : latestRawData_(0),
      latestFilteredData_(0.0),
      
      // 2. فلگ‌های وضعیت
      flagIsStable_(false),
      flagIsOverload_(false),
      flagIsTare_(false),
      flagIsZero_(false),
      flagIsNegative_(false),
      flagIsUnderWeight_(false),
      flagIsFirstRange_(false),
      flagIsSecondRange_(false),
      
      // 3. پین‌های سخت‌افزاری
      _pinSck(pinSck),
      _pinDataInt(pinDataInt),
      _pinPwdn(pinPwdn),
      _pinA0(pinA0),
      
      // 4. متغیرهای کالیبراسیون
      zeroOffset_(0),
      scale_(1.0),
      
      // 5. متغیرهای تنظیمات ظرفیت و دقت
      maxCapacity_(30.0),
      breakPoint_(15.0),
      resolutionLow_(0.005),
      resolutionHigh_(0.01),
      
      // 6. متغیرهای تست پایداری
      noiseMin_(0),
      noiseMax_(0),
      analysisStartTime_(0),
      
      // 7. متغیرهای فیلتر
      firOrder_(5),
      firCoeffs_(nullptr),
      firBuffer_(nullptr),
      firIndex_(0),
      firType_(FirType::MOVING_AVG),
      
      // 8. پارامترهای محاسبه وضعیت
      stabilityThreshold_(0.002),
      stableCountLimit_(10),
      underWeightThreshold_(0.05),
      
      // 9. پارامترهای Zero Tracking
      zeroTrackingEnabled_(true),
      zeroTrackingRange_(0.05),
      zeroTrackingTimeLimit_(3),
      zeroTrackingTimer_(0),
      
      // 10. هندلر تسک
      adcTaskHandle_(nullptr) {
}

// --- متد جدید: دریافت داده‌ها به صورت کلاس ---
WeightData ADC_AD7191::getWeightData() {
    WeightData data;
    data.setWeight(latestFilteredData_);
    data.setStable(flagIsStable_);
    data.setOverload(flagIsOverload_);
    data.setTare(flagIsTare_);
    data.setZero(flagIsZero_);
    data.setNegative(flagIsNegative_);
    data.setUnderWeight(flagIsUnderWeight_);
    data.setFirstRange(flagIsFirstRange_);
    data.setSecondRange(flagIsSecondRange_);
    return data;
}

// --- آنالیز پایداری ---
void ADC_AD7191::startResolutionAnalysis() {
    isAnalyzingStability_ = true;
    noiseMin_ = latestRawData_; 
    noiseMax_ = latestRawData_; 
    analysisStartTime_ = xTaskGetTickCount() * portTICK_PERIOD_MS;
    ESP_LOGI(TAG, "Resolution Analysis Started for 5 seconds...");
}

double ADC_AD7191::getCalculatedResolution() {
    if (noiseMax_ == noiseMin_) {
        return 0.0; 
    }
    int32_t peakToPeak = noiseMax_ - noiseMin_;
    double noiseBits = (peakToPeak > 0) ? log2(peakToPeak)  : 0;
    
    double effectiveBits = 24 - noiseBits;
    if (effectiveBits < 1) effectiveBits = 1; 
    double resolution = (maxCapacity_ * 1000.0) / pow(2.0, effectiveBits);
    
    ESP_LOGI(TAG, "Analysis Done: Min=%ld, Max=%ld, P2P=%ld, NoiseBits=%.2f, EffBits=%.2f, Res=%.9f gram", 
             noiseMin_, noiseMax_, peakToPeak, noiseBits, effectiveBits, resolution);
             
    return effectiveBits;
}

// --- Getters تنظیمات ---
double ADC_AD7191::getMaxCapacity() const { return maxCapacity_; }
double ADC_AD7191::getBreakPoint() const { return breakPoint_; }
double ADC_AD7191::getResLow() const { return resolutionLow_; }
double ADC_AD7191::getResHigh() const { return resolutionHigh_; }

// --- Getters وضعیت ---
double ADC_AD7191::getStabilityThreshold() const { return stabilityThreshold_; }
uint8_t ADC_AD7191::getStableCountLimit() const { return stableCountLimit_; }
double ADC_AD7191::getUnderWeightThreshold() const { return underWeightThreshold_; }

// --- Getters Zero Tracking ---
bool ADC_AD7191::getZeroTrackingEnabled() const { return zeroTrackingEnabled_; }
double ADC_AD7191::getZeroTrackingRange() const { return zeroTrackingRange_; }
uint16_t ADC_AD7191::getZeroTrackingTime() const { return zeroTrackingTimeLimit_; }

// --- تنظیمات Zero Tracking ---
void ADC_AD7191::setZeroTracking(bool enable) {
    zeroTrackingEnabled_ = enable;
    zeroTrackingTimer_ = 0;
    ESP_LOGI(TAG, "Zero Tracking %s", enable ? "Enabled" : "Disabled");
}

void ADC_AD7191::setZeroTrackingRange(double range) {
    zeroTrackingRange_ = range;
    ESP_LOGI(TAG, "Zero Tracking Range set to: %.4f", range);
}

void ADC_AD7191::setZeroTrackingTime(uint16_t seconds) {
    zeroTrackingTimeLimit_ = seconds;
    ESP_LOGI(TAG, "Zero Tracking Time set to: %d seconds", seconds);
}

// --- منطق Zero Tracking ---
void ADC_AD7191::applyZeroTrackingLogic(double currentWeight) {
    if (!zeroTrackingEnabled_) {
        zeroTrackingTimer_ = 0;
        return;
    }
    if (std::abs(currentWeight) <= zeroTrackingRange_) {
        zeroTrackingTimer_++;
        if (zeroTrackingTimer_ >= (zeroTrackingTimeLimit_ * 100)) { 
            ESP_LOGI(TAG, "Zero Tracking: Adjusting Offset. Old: %ld", zeroOffset_);
            int32_t offsetAdjustment = (int32_t)(currentWeight / scale_);
            zeroOffset_ += offsetAdjustment;
            ESP_LOGI(TAG, "Zero Tracking: New Offset: %ld (Adj: %ld)", zeroOffset_, offsetAdjustment);
            saveCalibration();
            zeroTrackingTimer_ = 0;
        }
    } else {
        zeroTrackingTimer_ = 0;
    }
}

// --- تنظیم پارامترهای استیبل ---
void ADC_AD7191::setStabilityParams(double threshold, uint8_t countLimit) {
    stabilityThreshold_ = threshold;
    stableCountLimit_ = countLimit;
    ESP_LOGI(TAG, "Stability params updated: Th=%.3f, Limit=%d", threshold, countLimit);
}

// --- تنظیم پارامترهای UnderWeight ---
void ADC_AD7191::setUnderWeightParams(double threshold) {
    underWeightThreshold_ = threshold;
    ESP_LOGI(TAG, "UnderWeight threshold updated: %.3f", threshold);
}

// --- تابع راه‌اندازی ---
void ADC_AD7191::init() {
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << _pinSck) | (1ULL << _pinPwdn) | (1ULL << _pinA0);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);

    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << _pinDataInt);
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);

    gpio_set_level(_pinPwdn, 1);
    vTaskDelay(10 / portTICK_PERIOD_MS);
    gpio_set_level(_pinPwdn, 0);
    vTaskDelay(50 / portTICK_PERIOD_MS);
    
    gpio_set_level(_pinSck, 1);
    gpio_set_level(_pinA0, 0);
    
    ESP_LOGI(TAG, "درایور ADC راه‌اندازی شد.");
    
    if (loadCalibration()) {
         ESP_LOGI(TAG, "تنظیمات از حافظه بارگذاری شد.");
         // بارگذاری مجدد ضرایب فیلتر بعد از اطمینان از بارگذاری تنظیمات
         initFirCoeffs();
     } else {
         ESP_LOGW(TAG, "تنظیماتی در حافظه یافت نشد. استفاده از مقادیر پیش‌فرض.");
         initFirCoeffs(); // ضرایب پیش‌فرض را بارگذاری کن
     }
     
     latestFilteredData_ = 0;
     startContinuousReading();
}

void ADC_AD7191::setChannel(uint8_t channel) {
    if (channel > 1) channel = 1;
    gpio_set_level(_pinA0, channel);
}

void ADC_AD7191::delayUs(uint32_t us) {
    esp_rom_delay_us(us);
}

int32_t ADC_AD7191::readRaw() {
    int32_t value = 0;
    int timeout = 100;
    while (gpio_get_level(_pinDataInt) != 0 && timeout > 0) {
        delayUs(1000);
        timeout--;
    }
    for (int i = 0; i < 24; i++) {
        gpio_set_level(_pinSck, 0);
        delayUs(1);
        int bit = gpio_get_level(_pinDataInt);
        value = (value << 1) | bit;
        gpio_set_level(_pinSck, 1);
        delayUs(1);
    }
    for (int i = 0; i < 3; i++) {
        gpio_set_level(_pinSck, 0);
        delayUs(1);
        gpio_set_level(_pinSck, 1);
        delayUs(1);
    }
    value -= 0x800000;
    return value;
}

double ADC_AD7191::applyFirFilter(double input) {
    if (firOrder_ <= 1 || firBuffer_ == nullptr) {
        return input;
    }
    firBuffer_[firIndex_] = input;
    double sum = 0.0;
    for (int i = 0; i < firOrder_; i++) {
        int bufIndex = (firIndex_ - i + firOrder_) % firOrder_;
        sum += firBuffer_[bufIndex] * firCoeffs_[i];
    }
    firIndex_ = (firIndex_ + 1) % firOrder_;
    return sum;
}

void ADC_AD7191::setFirOrder(uint8_t order) {
    if (order < 1) order = 1;
    if (order > 50) order = 50;
    firOrder_ = order;
    initFirCoeffs();
    saveCalibration();
}

uint8_t ADC_AD7191::getFirOrder() const {
    return firOrder_;
}

void ADC_AD7191::initFirCoeffs() {
    // پاکسازی حافظه قبلی
    if (firCoeffs_ != nullptr) {
        delete[] firCoeffs_;
        firCoeffs_ = nullptr;
    }
    if (firBuffer_ != nullptr) {
        delete[] firBuffer_;
        firBuffer_ = nullptr;
    }
    if (firOrder_ == 0) return;

    size_t free_heap = xPortGetFreeHeapSize();
    size_t required_mem = (firOrder_ * sizeof(double)) * 2;
    if (free_heap < required_mem + 1024) {
        ESP_LOGE(TAG, "Memory Full! Cannot allocate FIR filter.");
        return;
    }

    firCoeffs_ = new (std::nothrow) double[firOrder_];
    firBuffer_ = new (std::nothrow) double[firOrder_];
    
    if (firCoeffs_ == nullptr || firBuffer_ == nullptr) {
        ESP_LOGE(TAG, "Allocation Failed!");
        if (firCoeffs_) { delete[] firCoeffs_; firCoeffs_ = nullptr; }
        if (firBuffer_) { delete[] firBuffer_; firBuffer_ = nullptr; }
        return;
    }

    for (int i = 0; i < firOrder_; i++) firBuffer_[i] = 0.0;
    int center = firOrder_ / 2;

    switch (firType_) {
        case FirType::MOVING_AVG:
            {
                double coeff = 1.0 / firOrder_;
                for (int i = 0; i < firOrder_; i++) firCoeffs_[i] = coeff;
            }
            ESP_LOGI(TAG, "Filter: Moving Average (Order=%d)", firOrder_);
            break;
        case FirType::SINC3:
            for (int i = 0; i < firOrder_; i++) {
                double x = M_PI * (i - center);
                if (std::abs(x) < 1e-9) {
                    firCoeffs_[i] = 1.0;
                } else {
                    double sinc = sin(x) / x;
                    firCoeffs_[i] = sinc * sinc * sinc;
                }
            }
            ESP_LOGI(TAG, "Filter: Sinc3 (Order=%d)", firOrder_);
            break;
        case FirType::SINC4:
            for (int i = 0; i < firOrder_; i++) {
                double x = M_PI * (i - center);
                if (std::abs(x) < 1e-9) {
                    firCoeffs_[i] = 1.0;
                } else {
                    double sinc = sin(x) / x;
                    firCoeffs_[i] = sinc * sinc * sinc * sinc;
                }
            }
            ESP_LOGI(TAG, "Filter: Sinc4 (Order=%d)", firOrder_);
            break;
        default:
            {
                double coeff = 1.0 / firOrder_;
                for (int i = 0; i < firOrder_; i++) firCoeffs_[i] = coeff;
            }
            break;
    }

    // نرمال‌سازی ضرایب
    double sum = 0.0;
    for (int i = 0; i < firOrder_; i++) sum += firCoeffs_[i];
    if (sum > 1e-9) {
        for (int i = 0; i < firOrder_; i++) firCoeffs_[i] /= sum;
    } else {
        ESP_LOGW(TAG, "Sum of coeffs is zero, skipping normalization");
    }
    firIndex_ = 0;
}

void ADC_AD7191::setFirType(FirType type) {
    firType_ = type;
    initFirCoeffs();
    saveCalibration();
}

ADC_AD7191::FirType ADC_AD7191::getFirType() const {
    return firType_;
}

double ADC_AD7191::read() {
    return latestFilteredData_;
}

// --- Getters وضعیت ---
bool ADC_AD7191::getIsStable() const { return flagIsStable_; }
bool ADC_AD7191::getIsOverload() const { return flagIsOverload_; }
bool ADC_AD7191::getIsTare() const { return flagIsTare_; }
bool ADC_AD7191::getIsZero() const { return flagIsZero_; }
bool ADC_AD7191::getIsNegative() const { return flagIsNegative_; }
bool ADC_AD7191::getIsUnderWeight() const { return flagIsUnderWeight_; }
bool ADC_AD7191::getIsFirstRange() const { return flagIsFirstRange_; }
bool ADC_AD7191::getIsSecondRange() const { return flagIsSecondRange_; }

// --- توابع محاسبه وضعیت ---
void ADC_AD7191::updateStability(double currentWeight, double prevWeight, uint8_t &counter) {
    if (std::abs(currentWeight - prevWeight) < stabilityThreshold_) {
        counter++;
        if (counter >= stableCountLimit_) {
            flagIsStable_ = true;
        }
    } else {
        counter = 0;
        flagIsStable_ = false;
    }
}

void ADC_AD7191::updateOverload(double weight, double currentRes) {
    double overloadLimit = maxCapacity_ + (9.0 * currentRes);
    flagIsOverload_ = (weight > overloadLimit);
}

void ADC_AD7191::updateUnderWeight(double weight) {
    flagIsUnderWeight_ = (weight > 0 && weight < underWeightThreshold_);
}

void ADC_AD7191::updateRangeFlags(double weight) {
    flagIsZero_ = (std::abs(weight) < resolutionLow_);
    flagIsNegative_ = (weight < 0);
    if (weight >= 0 && weight <= breakPoint_) {
        flagIsFirstRange_ = true;
        flagIsSecondRange_ = false;
    } else if (weight > breakPoint_ && weight <= maxCapacity_) {
        flagIsFirstRange_ = false;
        flagIsSecondRange_ = true;
    } else {
        flagIsFirstRange_ = false;
        flagIsSecondRange_ = false;
    }
}

// --- کالیبراسیون ---
void ADC_AD7191::calibrateZero() {
    ESP_LOGI(TAG, "شروع کالیبراسیون صفر...");
    vTaskDelay(pdMS_TO_TICKS(500));
    zeroOffset_ = latestRawData_;
    ESP_LOGI(TAG, "کالیبراسیون صفر انجام شد. مقدار خام: %ld", zeroOffset_);
}

void ADC_AD7191::calibrateSpan(double knownWeight) {
    int32_t rawSpan = latestRawData_ - zeroOffset_;
    if (rawSpan != 0) {
        scale_ = (knownWeight * 1000.0) / rawSpan; // تبدیل کیلو به گرم برای دقت بیشتر در محاسبات داخلی
        ESP_LOGI(TAG, "کالیبراسیون ضریب انجام شد. ضریب جدید: %.9f", scale_);
    } else {
        ESP_LOGE(TAG, "خطا در کالیبراسیون ضریب: تفاضل صفر است.");
    }
}

void ADC_AD7191::setRangeSettings(double maxCap, double breakPoint, double resLow, double resHigh) {
    maxCapacity_ = maxCap;
    breakPoint_ = breakPoint;
    resolutionLow_ = resLow;
    resolutionHigh_ = resHigh;
}

// --- تنظیم دقیق ضریب (Fine Tuning) ---
void ADC_AD7191::fineTuneScale(int direction) {
    // تغییر درصدی اسکیل (مثلاً ۰.۰۵ درصد)
    double percentChange = 0.0005; 
    if (direction > 0) {
        scale_ *= (1.0 + percentChange);
    } else {
        scale_ *= (1.0 - percentChange);
    }
    
    // محدود کردن برای جلوگیری از صفر یا بی‌نهایت شدن
    if (scale_ <= 0.000000001) scale_ = 0.000000001;
    
    ESP_LOGI(TAG, "Fine Tune: Direction=%d, New Scale=%.9f", direction, scale_);
    saveCalibration();
}

// --- حافظه ---
void ADC_AD7191::saveCalibration() {
    nvs_handle_t nvsHandle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvsHandle);
    if (err == ESP_OK) {
        nvs_set_i32(nvsHandle, "zero", zeroOffset_);
        nvs_set_blob(nvsHandle, "scale", &scale_, sizeof(scale_));
        
        nvs_set_blob(nvsHandle, "maxcap", &maxCapacity_, sizeof(maxCapacity_));
        nvs_set_blob(nvsHandle, "breakpt", &breakPoint_, sizeof(breakPoint_));
        nvs_set_blob(nvsHandle, "reslow", &resolutionLow_, sizeof(resolutionLow_));
        nvs_set_blob(nvsHandle, "reshigh", &resolutionHigh_, sizeof(resolutionHigh_));
        
        nvs_set_u8(nvsHandle, "firorder", firOrder_);
        nvs_set_u8(nvsHandle, "firtype", (uint8_t)firType_);
        
        nvs_set_blob(nvsHandle, "stab_th", &stabilityThreshold_, sizeof(stabilityThreshold_));
        nvs_set_u8(nvsHandle, "stab_cnt", stableCountLimit_);
        nvs_set_blob(nvsHandle, "uw_th", &underWeightThreshold_, sizeof(underWeightThreshold_));
        
        nvs_set_u8(nvsHandle, "zt_en", (uint8_t)zeroTrackingEnabled_);
        nvs_set_blob(nvsHandle, "zt_range", &zeroTrackingRange_, sizeof(zeroTrackingRange_));
        nvs_set_u16(nvsHandle, "zt_time", zeroTrackingTimeLimit_);
        
        nvs_commit(nvsHandle);
        nvs_close(nvsHandle);
        ESP_LOGD(TAG, "All settings saved to NVS.");
    } else {
        ESP_LOGE(TAG, "Error opening NVS for saving: %s", esp_err_to_name(err));
    }
}

bool ADC_AD7191::loadCalibration() {
    nvs_handle_t nvsHandle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvsHandle);
    if (err == ESP_OK) {
        int32_t z;
        double s;
        size_t req_size;
        
        if (nvs_get_i32(nvsHandle, "zero", &z) == ESP_OK) zeroOffset_ = z;
        
        req_size = sizeof(s);
        if (nvs_get_blob(nvsHandle, "scale", &s, &req_size) == ESP_OK) scale_ = s;
        
        req_size = sizeof(maxCapacity_);
        if (nvs_get_blob(nvsHandle, "maxcap", &maxCapacity_, &req_size) == ESP_OK);
        req_size = sizeof(breakPoint_);
        if (nvs_get_blob(nvsHandle, "breakpt", &breakPoint_, &req_size) == ESP_OK);
        req_size = sizeof(resolutionLow_);
        if (nvs_get_blob(nvsHandle, "reslow", &resolutionLow_, &req_size) == ESP_OK);
        req_size = sizeof(resolutionHigh_);
        if (nvs_get_blob(nvsHandle, "reshigh", &resolutionHigh_, &req_size) == ESP_OK);
        
        uint8_t order;
        if (nvs_get_u8(nvsHandle, "firorder", &order) == ESP_OK) firOrder_ = order;
        uint8_t typeVal;
        if (nvs_get_u8(nvsHandle, "firtype", &typeVal) == ESP_OK) firType_ = (FirType)typeVal;
        
        req_size = sizeof(stabilityThreshold_);
        if (nvs_get_blob(nvsHandle, "stab_th", &stabilityThreshold_, &req_size) == ESP_OK);
        if (nvs_get_u8(nvsHandle, "stab_cnt", &stableCountLimit_) == ESP_OK);
        req_size = sizeof(underWeightThreshold_);
        if (nvs_get_blob(nvsHandle, "uw_th", &underWeightThreshold_, &req_size) == ESP_OK);
        
        uint8_t ztEn;
        if (nvs_get_u8(nvsHandle, "zt_en", &ztEn) == ESP_OK) zeroTrackingEnabled_ = (bool)ztEn;
        req_size = sizeof(zeroTrackingRange_);
        if (nvs_get_blob(nvsHandle, "zt_range", &zeroTrackingRange_, &req_size) == ESP_OK);
        if (nvs_get_u16(nvsHandle, "zt_time", &zeroTrackingTimeLimit_) == ESP_OK);
        
        nvs_close(nvsHandle);
        ESP_LOGI(TAG, "Settings loaded from NVS.");
        return true;
    }
    ESP_LOGW(TAG, "NVS not found or empty.");
    return false;
}

void ADC_AD7191::startContinuousReading() {
    if (adcTaskHandle_ == nullptr) {
        xTaskCreate(adcTask, "ADC_Task", 4096, this, 5, &adcTaskHandle_); // استک کمی افزایش یافت
        ESP_LOGI(TAG, "Continuous ADC Task started");
    }
}

void ADC_AD7191::stopContinuousReading() {
    if (adcTaskHandle_ != nullptr) {
        vTaskDelete(adcTaskHandle_);
        adcTaskHandle_ = nullptr;
        ESP_LOGI(TAG, "Continuous ADC Task stopped");
    }
}

double ADC_AD7191::manageResolution(double weight) {
    double resolution;
    if (weight < 0) {
        resolution = resolutionLow_;
    } else if (weight <= breakPoint_) {
        resolution = resolutionLow_;
    } else {
        resolution = resolutionHigh_;
    }
    return std::round(weight / resolution) * resolution;
}

// --- تسک اصلی ADC ---
void ADC_AD7191::adcTask(void* pvParameters) {
    ADC_AD7191* adc = static_cast<ADC_AD7191*>(pvParameters);
    double prevWeight = 0.0;
    uint8_t stableCounter = 0;
    
    while (1) {
        // ۱. خواندن داده خام
        adc->latestRawData_ = adc->readRaw();

        // ۲. منطق آنالیز پایداری (اگر فعال باشد)
        if (adc->isAnalyzingStability_) {
            uint32_t currentTime = xTaskGetTickCount() * portTICK_PERIOD_MS;
            
            if (adc->latestRawData_ < adc->noiseMin_) adc->noiseMin_ = adc->latestRawData_;
            if (adc->latestRawData_ > adc->noiseMax_) adc->noiseMax_ = adc->latestRawData_;
            
            if ((currentTime - adc->analysisStartTime_) >= 5000) {
                adc->isAnalyzingStability_ = false;
                double res = adc->getCalculatedResolution();
                ESP_LOGI(TAG, "Auto-Analysis Finished. Calculated Resolution: %.9f kg", res);
            }
        }

        // ۳. محاسبه وزن دقیق (خام - آفست) * اسکیل
        double exactWeight = (adc->latestRawData_ - adc->zeroOffset_) * adc->scale_;
        
        // ۴. اعمال فیلتر بر روی وزن دقیق
        double filteredWeight = adc->applyFirFilter(exactWeight);
        
        // ۵. اعمال منطق Zero Tracking بر روی وزن فیلتر شده
        adc->applyZeroTrackingLogic(filteredWeight);
        
        // ۶. محاسبه نهایی وزن با استفاده از آفست احتمالاً تغییر کرده (توسط Zero Tracking)
        // نکته: برای یکپارچگی، دوباره وزن را محاسبه می‌کنیم اما این بار از فیلتر شده استفاده نمی‌کنیم چون فیلتر تاخیر دارد
        // اما برای نمایش نهایی، استفاده از فیلتر بهتر است. پس:
        double finalWeight = filteredWeight; 
        
        // ۷. گرد کردن و مدیریت رزولوشن
        double roundedWeight = adc->manageResolution(finalWeight);
        
        // ۸. بروزرسانی فلگ‌ها
        adc->updateRangeFlags(roundedWeight);
        adc->updateOverload(roundedWeight, adc->resolutionHigh_);
        adc->updateUnderWeight(roundedWeight);
        adc->updateStability(roundedWeight, prevWeight, stableCounter);
        
        adc->flagIsTare_ = adc->flagIsZero_;
        adc->latestFilteredData_ = roundedWeight;
        prevWeight = roundedWeight;
        
        // لاگ گرفتن فقط برای دیباگ (در حالت عادی کامنت کنید)
        // ESP_LOGI(TAG, "Raw: %ld, W: %.3f", adc->latestRawData_, roundedWeight);
        
        vTaskDelay(pdMS_TO_TICKS(10)); 
    }
}