#include "StatusLed.h"

#include "Config.h"

static_assert(
    Config::StatusLed::SLOW_DUTY_PERCENT <
        Config::StatusLed::FAST_DUTY_PERCENT,
    "Status LED duty range must increase"
);
static_assert(
    Config::StatusLed::FAST_DUTY_PERCENT <= 100,
    "Status LED duty range cannot exceed 100 percent"
);
static_assert(
    Config::StatusLed::SLOW_RPM < Config::StatusLed::FAST_RPM,
    "Status LED RPM range must increase"
);
static_assert(
    Config::StatusLed::FAST_FLASH_INTERVAL_MS >
        Config::StatusLed::FLASH_ON_TIME_MS,
    "Status LED flash interval must exceed its on time"
);
static_assert(
    Config::StatusLed::FAST_FLASH_INTERVAL_MS <
        Config::StatusLed::SLOW_FLASH_INTERVAL_MS,
    "Status LED flash interval must decrease as the response rises"
);

StatusLed::StatusLed()
    : led_(
          Config::StatusLed::LED_COUNT,
          Config::Pins::RGB_LED,
          NEO_GRB + NEO_KHZ800
      )
{
}

void StatusLed::begin()
{
    led_.begin();
    led_.setBrightness(Config::StatusLed::BRIGHTNESS);
    turnOff();
    stateChangedTimeMs_ = millis();
}

void StatusLed::setDutyPercent(uint8_t dutyPercent)
{
    currentDutyPercent_ = dutyPercent <= 100 ? dutyPercent : 100;
}

void StatusLed::setRpm(float rpm)
{
    currentRpm_ = rpm > 0.0f ? rpm : 0.0f;
}

void StatusLed::update()
{
    const uint32_t now = millis();

    if (responseValue() <= 0.0f) {
        if (isOn_) {
            turnOff();
        }

        wasInactive_ = true;
        return;
    }

    if (wasInactive_) {
        wasInactive_ = false;
        turnOn();
        stateChangedTimeMs_ = now;
        return;
    }

    const uint32_t elapsedMs = now - stateChangedTimeMs_;

    if (isOn_) {
        if (elapsedMs >= Config::StatusLed::FLASH_ON_TIME_MS) {
            turnOff();
            stateChangedTimeMs_ = now;
        }
        return;
    }

    const uint32_t offTimeMs =
        flashIntervalMs() - Config::StatusLed::FLASH_ON_TIME_MS;
    if (elapsedMs >= offTimeMs) {
        turnOn();
        stateChangedTimeMs_ = now;
    }
}

float StatusLed::responseValue() const
{
    if (Config::StatusLed::RESPONSE_SOURCE ==
        Config::StatusLed::ResponseSource::DUTY) {
        return currentDutyPercent_;
    }

    return currentRpm_;
}

float StatusLed::normalizedResponse() const
{
    const float value = responseValue();
    const float minimum =
        Config::StatusLed::RESPONSE_SOURCE ==
            Config::StatusLed::ResponseSource::DUTY
        ? Config::StatusLed::SLOW_DUTY_PERCENT
        : Config::StatusLed::SLOW_RPM;
    const float maximum =
        Config::StatusLed::RESPONSE_SOURCE ==
            Config::StatusLed::ResponseSource::DUTY
        ? Config::StatusLed::FAST_DUTY_PERCENT
        : Config::StatusLed::FAST_RPM;

    if (value <= minimum) {
        return 0.0f;
    }

    if (value >= maximum) {
        return 1.0f;
    }

    return (value - minimum) / (maximum - minimum);
}

uint32_t StatusLed::flashIntervalMs() const
{
    const float intervalRange =
        Config::StatusLed::SLOW_FLASH_INTERVAL_MS -
        Config::StatusLed::FAST_FLASH_INTERVAL_MS;

    return Config::StatusLed::SLOW_FLASH_INTERVAL_MS -
        static_cast<uint32_t>(intervalRange * normalizedResponse());
}

uint32_t StatusLed::colorForCurrentResponse()
{
    const uint16_t hue = Config::StatusLed::VIOLET_HUE -
        static_cast<uint16_t>(
            Config::StatusLed::VIOLET_HUE * normalizedResponse()
        );

    return led_.gamma32(led_.ColorHSV(
        hue,
        Config::StatusLed::SATURATION,
        Config::StatusLed::VALUE
    ));
}

void StatusLed::turnOn()
{
    led_.setPixelColor(0, colorForCurrentResponse());
    led_.show();
    isOn_ = true;
}

void StatusLed::turnOff()
{
    led_.clear();
    led_.show();
    isOn_ = false;
}
