#pragma once

class ADC_AD7191 {
public:
    ADC_AD7191();
    void init();
    double read(); // returns a weight-like value for demo
};
