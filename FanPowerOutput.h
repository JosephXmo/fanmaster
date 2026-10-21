#pragma once
#include <Arduino.h>
#include "Config.h"

namespace FanPowerOutput {
inline void setEnabled(bool enabled)
{
    digitalWrite(Config::Pins::FAN_POWER, enabled == Config::Power::RELAY_ACTIVE_HIGH ? HIGH : LOW);
}

inline void begin()
{
    // ESP32 Arduino 3.x requires a GPIO owner before digitalWrite works.
    // Register as an input, preload the inactive latch, then enable output.
    pinMode(Config::Pins::FAN_POWER, INPUT);
    setEnabled(false);
    pinMode(Config::Pins::FAN_POWER, OUTPUT);
}
}  // namespace FanPowerOutput
