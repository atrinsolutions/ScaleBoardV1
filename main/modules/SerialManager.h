#pragma once
#include "driver/uart.h"

class SerialManager {
public:
    void init();
    void send(const char* msg);
};
