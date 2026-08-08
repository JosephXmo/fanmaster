#pragma once

#include <Adafruit_SSD1306.h>
#include <Wire.h>

class OledDisplay {
public:
    explicit OledDisplay(TwoWire &wire);

    bool begin();
    bool isReady() const;
    Adafruit_SSD1306 &device();

private:
    Adafruit_SSD1306 display_;
    bool ready_ = false;
};
