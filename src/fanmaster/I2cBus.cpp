#include "I2cBus.h"

#include "Config.h"

bool I2cBus::begin()
{
    ready_ = Wire.begin(
        Config::Pins::I2C_SDA,
        Config::Pins::I2C_SCL,
        Config::I2c::CLOCK_HZ
    );
    return ready_;
}

TwoWire &I2cBus::wire()
{
    return Wire;
}

bool I2cBus::isReady() const
{
    return ready_;
}
