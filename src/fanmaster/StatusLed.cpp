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
static_assert(
    Config::StatusLed::FLASH_COLOR_RGB <= 0xFFFFFF,
    "Status LED flash color must use the 0xRRGGBB format"
);

StatusLed::StatusLed() = default;

void StatusLed::begin()
{
    FastLED.addLeds<WS2812B, Config::Pins::RGB_LED, GRB>(
        leds_,
        Config::StatusLed::LED_COUNT
    );
    FastLED.setBrightness(Config::StatusLed::BRIGHTNESS);
    turnOff();
    stateChangedTimeMs_ = millis();
}

void StatusLed::setDutyPercent(uint8_t dutyPercent)
{
    const uint8_t nextDutyPercent = dutyPercent <= 100 ? dutyPercent : 100;
    if (nextDutyPercent != currentDutyPercent_) {
        currentDutyPercent_ = nextDutyPercent;
        colorRefreshPending_ = true;
    }
}

void StatusLed::setRpm(float rpm)
{
    const float nextRpm = rpm > 0.0f ? rpm : 0.0f;
    if (nextRpm != currentRpm_) {
        currentRpm_ = nextRpm;
        colorRefreshPending_ = true;
    }
}

void StatusLed::toggleEnabled()
{
    enabled_ = !enabled_;
    wasInactive_ = true;
    colorRefreshPending_ = true;
    stateChangedTimeMs_ = millis();

    if (!enabled_) {
        // Keep FastLED active and explicitly transmit black when disabled.
        turnOff();
    }
}

void StatusLed::update()
{
    const uint32_t now = millis();
    reportDiagnostics(now);

    if (!enabled_) {
        if (isOn_) {
            turnOff();
        }
        return;
    }

    if (responseValue() <= 0.0f) {
        if (isOn_) {
            turnOff();
        }

        wasInactive_ = true;
        return;
    }

    if (Config::StatusLed::INDICATION_MODE ==
        Config::StatusLed::IndicationMode::SPECTRUM) {
        if (wasInactive_ || colorRefreshPending_) {
            wasInactive_ = false;
            turnOn();
        }
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

CRGB StatusLed::colorForCurrentResponse() const
{
    const uint8_t hue = Config::StatusLed::VIOLET_HUE -
        static_cast<uint8_t>(
            Config::StatusLed::VIOLET_HUE * normalizedResponse()
        );

    return CHSV(
        hue,
        Config::StatusLed::SATURATION,
        Config::StatusLed::VALUE
    );
}

void StatusLed::showPixels()
{
    const uint32_t startedUs = micros();
    FastLED.show();
    const uint32_t durationUs = micros() - startedUs;

    showCount_++;
    if (durationUs > maxShowDurationUs_) {
        maxShowDurationUs_ = durationUs;
    }
}

void StatusLed::reportDiagnostics(uint32_t now)
{
    if (Config::SerialConsole::PLOTTER_MODE ||
        now - lastDiagnosticMs_ < Config::Diagnostics::REPORT_INTERVAL_MS) {
        return;
    }

    lastDiagnosticMs_ = now;
    Serial.print("[LED] enabled=");
    Serial.print(enabled_ ? "yes" : "no");
    Serial.print(" shows=");
    Serial.print(showCount_);
    Serial.print(" max_show_us=");
    Serial.println(maxShowDurationUs_);
    showCount_ = 0;
    maxShowDurationUs_ = 0;
}

void StatusLed::turnOn()
{
    if (Config::StatusLed::INDICATION_MODE ==
        Config::StatusLed::IndicationMode::FLASH) {
        leds_[0] = CRGB(Config::StatusLed::FLASH_COLOR_RGB);
    } else {
        leds_[0] = colorForCurrentResponse();
    }

    showPixels();
    isOn_ = true;
    colorRefreshPending_ = false;
}

void StatusLed::turnOff()
{
    fill_solid(leds_, Config::StatusLed::LED_COUNT, CRGB::Black);
    showPixels();
    isOn_ = false;
}
