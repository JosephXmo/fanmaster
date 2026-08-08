#pragma once

#include <Arduino.h>

class StatusLed {
public:
    void begin();
    void addCompletedRevolutions(uint32_t revolutions);
    void update();

private:
    void turnOff();
    void turnRed();

    uint32_t revolutionsTowardFlash_ = 0;
    uint32_t pendingFlashes_ = 0;
    uint32_t offTimeMs_ = 0;
    bool isOn_ = false;
};
