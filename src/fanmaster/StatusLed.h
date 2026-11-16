#pragma once

#include <Arduino.h>
#include <FastLED.h>

#include "Config.h"

class StatusLed {
public:
    StatusLed();

    void begin();
    void setDutyPercent(uint8_t dutyPercent);
    void setRpm(float rpm);
    void toggleEnabled();
    void update();

private:
    float responseValue() const;
    float normalizedResponse() const;
    uint32_t flashIntervalMs() const;
    CRGB colorForCurrentResponse() const;
    void showPixels();
    void reportDiagnostics(uint32_t now);
    void turnOn();
    void turnOff();

    CRGB leds_[Config::StatusLed::LED_COUNT];
    uint8_t currentDutyPercent_ = 0;
    float currentRpm_ = 0.0f;
    uint32_t stateChangedTimeMs_ = 0;
    bool wasInactive_ = true;
    bool isOn_ = false;
    bool enabled_ = true;
    bool colorRefreshPending_ = true;
    uint32_t showCount_ = 0;
    uint32_t maxShowDurationUs_ = 0;
    uint32_t lastDiagnosticMs_ = 0;
};
