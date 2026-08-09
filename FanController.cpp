#include "FanController.h"

#include "Config.h"

FanController *FanController::interruptOwner_ = nullptr;

bool FanController::begin()
{
    const bool pwmAttached = ledcAttach(
        Config::Pins::FAN_PWM,
        Config::Fan::PWM_FREQUENCY_HZ,
        Config::Fan::PWM_RESOLUTION_BITS
    );

    if (!pwmAttached) {
        return false;
    }

    setDutyPercent(100);
    delay(Config::Fan::STARTUP_TIME_MS);
    setDutyPercent(Config::Fan::DEFAULT_DUTY_PERCENT);

    // A 4.7k-10k external pull-up to 3.3V is recommended for TACH.
    pinMode(Config::Pins::FAN_TACH, INPUT_PULLUP);
    interruptOwner_ = this;
    attachInterrupt(
        digitalPinToInterrupt(Config::Pins::FAN_TACH),
        tachInterrupt,
        FALLING
    );

    lastRpmSampleMs_ = millis();
    return true;
}

void FanController::setDutyPercent(uint8_t dutyPercent)
{
    if (dutyPercent > 100) {
        dutyPercent = 100;
    }

    const uint32_t fanDutyValue =
        (static_cast<uint32_t>(dutyPercent) * Config::Fan::PWM_MAX_VALUE) /
        100;

    const uint32_t gpioDutyValue = Config::Fan::PWM_INVERTED
        ? Config::Fan::PWM_MAX_VALUE - fanDutyValue
        : fanDutyValue;

    ledcWrite(Config::Pins::FAN_PWM, gpioDutyValue);
    currentDutyPercent_ = dutyPercent;
}

bool FanController::updateRpm(float &rpmResult)
{
    const uint32_t now = millis();
    const uint32_t elapsedMs = now - lastRpmSampleMs_;

    if (elapsedMs < Config::Fan::RPM_SAMPLE_INTERVAL_MS) {
        return false;
    }

    uint32_t pulseSnapshot;
    portENTER_CRITICAL(&tachMux_);
    pulseSnapshot = rpmWindowPulseCount_;
    rpmWindowPulseCount_ = 0;
    portEXIT_CRITICAL(&tachMux_);

    lastRpmSampleMs_ = now;
    currentRpm_ =
        (static_cast<float>(pulseSnapshot) * 60000.0f) /
        (static_cast<float>(elapsedMs) *
         static_cast<float>(Config::Fan::PULSES_PER_REVOLUTION));

    rpmResult = currentRpm_;
    return true;
}

uint8_t FanController::dutyPercent() const
{
    return currentDutyPercent_;
}

float FanController::currentRpm() const
{
    return currentRpm_;
}

void IRAM_ATTR FanController::tachInterrupt()
{
    if (interruptOwner_ != nullptr) {
        interruptOwner_->handleTachInterrupt();
    }
}

void IRAM_ATTR FanController::handleTachInterrupt()
{
    portENTER_CRITICAL_ISR(&tachMux_);
    rpmWindowPulseCount_++;
    portEXIT_CRITICAL_ISR(&tachMux_);
}
