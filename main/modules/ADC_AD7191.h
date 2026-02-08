#ifndef ADC_AD7191_H
#define ADC_AD7191_H

#include "driver/gpio.h"
#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// --- کلاس داده وزن (جایگزین struct) ---
class WeightData {
private:
    double _weight;
    bool _isStable;
    bool _isOverload;
    bool _isTare;
    bool _isZero;
    bool _isNegative;
    bool _isUnderWeight;
    bool _isFirstRange;
    bool _isSecondRange;

public:
    // سازنده
    WeightData() : _weight(0.0), _isStable(false), _isOverload(false), _isTare(false), 
                   _isZero(false), _isNegative(false), _isUnderWeight(false), 
                   _isFirstRange(false), _isSecondRange(false) {}

    // --- Setters ---
    void setWeight(double w) { _weight = w; }
    void setStable(bool s) { _isStable = s; }
    void setOverload(bool o) { _isOverload = o; }
    void setTare(bool t) { _isTare = t; }
    void setZero(bool z) { _isZero = z; }
    void setNegative(bool n) { _isNegative = n; }
    void setUnderWeight(bool u) { _isUnderWeight = u; }
    void setFirstRange(bool r) { _isFirstRange = r; }
    void setSecondRange(bool r) { _isSecondRange = r; }

    // --- Getters ---
    double getWeight() const { return _weight; }
    bool isStable() const { return _isStable; }
    bool isOverload() const { return _isOverload; }
    bool isTare() const { return _isTare; }
    bool isZero() const { return _isZero; }
    bool isNegative() const { return _isNegative; }
    bool isUnderWeight() const { return _isUnderWeight; }
    bool isFirstRange() const { return _isFirstRange; }
    bool isSecondRange() const { return _isSecondRange; }
    
    // متد کمکی برای ریست کردن همه فلگ‌ها
    void resetFlags() {
        _isStable = false;
        _isOverload = false;
        _isTare = false;
        _isZero = false;
        _isNegative = false;
        _isUnderWeight = false;
        _isFirstRange = false;
        _isSecondRange = false;
    }
};

class ADC_AD7191 {
public:
    // سازنده پیش‌فرض
    ADC_AD7191() : _pinSck(GPIO_NUM_41), _pinDataInt(GPIO_NUM_1), _pinPwdn(GPIO_NUM_42), _pinA0(GPIO_NUM_2) 
    {
        adcTaskHandle_ = nullptr;
    }
   
    // سازنده کلاس
    ADC_AD7191(gpio_num_t pinSck, gpio_num_t pinDataInt, gpio_num_t pinPwdn, gpio_num_t pinA0);
    
    // --- توابع عمومی و راه‌اندازی ---
    void init();                       
    int32_t readRaw();                 
    double read();                     
    void setChannel(uint8_t channel);  
    
    // --- متد دریافت داده‌ها (حالا یک شیء کلاس برمی‌گرداند) ---
    WeightData getWeightData();

    // --- توابع کالیبراسیون ---
    void calibrateZero();              
    void calibrateSpan(double knownWeight);
    void fineTuneScale(int direction);
    
    // --- تنظیمات ظرفیت و دقت ---
    void setRangeSettings(double maxCap, double breakPoint, double resLow, double resHigh);
    
    // --- توابع دریافت تنظیمات ظرفیت و دقت (Getters) ---
    double getMaxCapacity() const;
    double getBreakPoint() const;
    double getResLow() const;
    double getResHigh() const;
    
    // --- تنظیمات فیلتر ---
    enum class FirType {
        MOVING_AVG = 0,
        SINC3 = 1,
        SINC4 = 2,
        CUSTOM = 3
    };
    void setFirOrder(uint8_t order);              
    void setFirType(FirType type);                
    uint8_t getFirOrder() const;
    FirType getFirType() const;
    void initFirCoeffs();       
    
    // --- تنظیمات پارامترهای وضعیت ---
    void setStabilityParams(double threshold, uint8_t countLimit);
    void setUnderWeightParams(double threshold);
    
    // --- توابع دریافت پارامترهای وضعیت (Getters) ---
    double getStabilityThreshold() const;
    uint8_t getStableCountLimit() const;
    double getUnderWeightThreshold() const;
    
    // --- تنظیمات Zero Tracking ---
    void setZeroTracking(bool enable);
    void setZeroTrackingRange(double range);       
    void setZeroTrackingTime(uint16_t seconds);    
    // --- توابع دریافت تنظیمات Zero Tracking (Getters) ---
    bool getZeroTrackingEnabled() const;
    double getZeroTrackingRange() const;
    uint16_t getZeroTrackingTime() const;
    
    void startContinuousReading();
    void stopContinuousReading();  
    
    // --- توابع دریافت وضعیت (Flags) ---
    bool getIsStable() const;
    bool getIsOverload() const;
    bool getIsTare() const;
    bool getIsZero() const;
    bool getIsNegative() const;
    bool getIsUnderWeight() const;
    bool getIsFirstRange() const;
    bool getIsSecondRange() const;
    
    // --- توابع آنالیز دقت ---
    void startResolutionAnalysis();
    double getCalculatedResolution();
    
    // متغیر وضعیت آنالیز
    volatile bool isAnalyzingStability_;
    
    // --- توابع حافظه ---
    void saveCalibration();                    
    bool loadCalibration();    

private:
    // متغیرهای اشتراکی
    volatile int32_t latestRawData_;       
    volatile double latestFilteredData_;   
    
    // --- فلگ‌های وضعیت ---
    volatile bool flagIsStable_;
    volatile bool flagIsOverload_;
    volatile bool flagIsTare_;
    volatile bool flagIsZero_;
    volatile bool flagIsNegative_;
    volatile bool flagIsUnderWeight_;
    volatile bool flagIsFirstRange_;
    volatile bool flagIsSecondRange_;
    
    // --- پین‌های سخت‌افزاری ---
    gpio_num_t _pinSck;       
    gpio_num_t _pinDataInt;   
    gpio_num_t _pinPwdn;      
    gpio_num_t _pinA0;        
    
    // --- متغیرهای کالیبراسیون ---
    int32_t zeroOffset_;      
    double scale_;            
    
    // --- متغیرهای تنظیمات ظرفیت و دقت ---
    double maxCapacity_;      
    double breakPoint_;       
    double resolutionLow_;    
    double resolutionHigh_;   
    
    // --- متغیرهای تست پایداری ---
    int32_t noiseMin_;                  
    int32_t noiseMax_;                  
    uint32_t analysisStartTime_;        
    
    // --- متغیرهای فیلتر ---
    uint8_t firOrder_;        
    double* firCoeffs_;       
    double* firBuffer_;       
    uint8_t firIndex_;        
    FirType firType_;         
    
    // --- پارامترهای محاسبه وضعیت ---
    double stabilityThreshold_;      
    uint8_t stableCountLimit_;       
    double underWeightThreshold_;    
    
    // --- پارامترهای Zero Tracking ---
    bool zeroTrackingEnabled_;       
    double zeroTrackingRange_;       
    uint16_t zeroTrackingTimeLimit_; 
    uint16_t zeroTrackingTimer_;     
    
    TaskHandle_t adcTaskHandle_;
    
    // --- توابع کمکی داخلی ---
    void delayUs(uint32_t us);                 
    double applyFirFilter(double input);       
    void applyZeroTrackingLogic(double currentWeight); 
    
    // --- توابع محاسبه وضعیت ---
    void updateStability(double currentWeight, double prevWeight, uint8_t &counter);
    void updateOverload(double weight, double currentRes);
    void updateUnderWeight(double weight);
    void updateRangeFlags(double weight);
    double manageResolution(double weight);
    
    // --- تسک ---
    static void adcTask(void* pvParameters);
};

#endif // ADC_AD7191_H