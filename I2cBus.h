#pragma once

#include <Wire.h>

class I2cBus {
public:
    bool begin();
    TwoWire &wire();
    bool isReady() const;

private:
    bool ready_ = false;
};
