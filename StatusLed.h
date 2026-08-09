#pragma once

#include <Adafruit_NeoPixel.h>
#include <Arduino.h>

class StatusLed {
public:
    StatusLed();

    void begin();
    void setDutyPercent(uint8_t dutyPercent);
    void setRpm(float rpm);
    void update();

private:
    float responseValue() const;
    float normalizedResponse() const;
    uint32_t flashIntervalMs() const;
    uint32_t colorForCurrentResponse();
    void turnOn();
    void turnOff();

    Adafruit_NeoPixel led_;
    uint8_t currentDutyPercent_ = 0;
    float currentRpm_ = 0.0f;
    uint32_t stateChangedTimeMs_ = 0;
    bool wasInactive_ = true;
    bool isOn_ = false;
};
