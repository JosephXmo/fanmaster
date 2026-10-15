#pragma once

#include <Arduino.h>
#include <atomic>
#include <mutex>

class FanController {
public:
    bool begin();
    void setDutyPercent(uint8_t dutyPercent);
    bool updateRpm(float &rpmResult);

    uint8_t dutyPercent() const;
    float currentRpm() const;

private:
    static void IRAM_ATTR tachInterrupt();
    void IRAM_ATTR handleTachInterrupt();

    static FanController *interruptOwner_;

    portMUX_TYPE tachMux_ = portMUX_INITIALIZER_UNLOCKED;
    volatile uint32_t rpmWindowPulseCount_ = 0;

    uint32_t lastRpmSampleMs_ = 0;
    std::atomic<float> currentRpm_{0.0f};
    std::atomic<uint8_t> currentDutyPercent_{0};
    std::mutex dutyMutex_;
};
