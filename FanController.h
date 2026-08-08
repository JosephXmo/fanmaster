#pragma once

#include <Arduino.h>

class FanController {
public:
    bool begin();
    void setDutyPercent(uint8_t dutyPercent);
    bool updateRpm(float &rpmResult);
    uint32_t takeCompletedRevolutions();

    uint8_t dutyPercent() const;

private:
    static void IRAM_ATTR tachInterrupt();
    void IRAM_ATTR handleTachInterrupt();

    static FanController *interruptOwner_;

    portMUX_TYPE tachMux_ = portMUX_INITIALIZER_UNLOCKED;
    volatile uint32_t rpmWindowPulseCount_ = 0;
    volatile uint32_t pulsesTowardNextRevolution_ = 0;
    volatile uint32_t completedRevolutionCount_ = 0;

    uint32_t lastRpmSampleMs_ = 0;
    float currentRpm_ = 0.0f;
    uint8_t currentDutyPercent_ = 0;
};
