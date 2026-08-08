#include "StatusLed.h"

#include "Config.h"

void StatusLed::begin()
{
    turnOff();
}

void StatusLed::addCompletedRevolutions(uint32_t revolutions)
{
    const uint64_t total =
        static_cast<uint64_t>(revolutionsTowardFlash_) + revolutions;

    pendingFlashes_ += total / Config::StatusLed::REVOLUTIONS_PER_FLASH;
    revolutionsTowardFlash_ =
        total % Config::StatusLed::REVOLUTIONS_PER_FLASH;
}

void StatusLed::update()
{
    const uint32_t now = millis();

    if (isOn_) {
        if (static_cast<int32_t>(now - offTimeMs_) < 0) {
            return;
        }

        turnOff();
        isOn_ = false;
    }

    if (pendingFlashes_ == 0) {
        return;
    }

    pendingFlashes_--;
    turnRed();
    isOn_ = true;
    offTimeMs_ = now + Config::StatusLed::FLASH_DURATION_MS;
}

void StatusLed::turnOff()
{
    rgbLedWrite(Config::Pins::RGB_LED, 0, 0, 0);
}

void StatusLed::turnRed()
{
    rgbLedWrite(Config::Pins::RGB_LED, Config::StatusLed::RED_BRIGHTNESS, 0, 0);
}
