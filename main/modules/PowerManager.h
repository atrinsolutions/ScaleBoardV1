#pragma once

class PowerManager {
public:
    void init();
    void monitor(); // check battery and handle UPS-CTRL
};
