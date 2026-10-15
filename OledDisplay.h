#pragma once

#include <Adafruit_SSD1306.h>
#include <IPAddress.h>
#include <Wire.h>

class OledDisplay {
public:
    explicit OledDisplay(TwoWire &wire);

    bool begin();
    bool isReady() const;
    void showStatus(
        float rpm,
        uint8_t dutyPercent,
        bool wifiConnected,
        const IPAddress &ipAddress,
        const String &version,
        bool hasClimateReading,
        float temperatureC,
        float humidityPercent
    );
    Adafruit_SSD1306 &device();

private:
    Adafruit_SSD1306 display_;
    bool ready_ = false;
};
