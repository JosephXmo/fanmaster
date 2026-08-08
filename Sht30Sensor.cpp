#include "Sht30Sensor.h"

#include "Config.h"

static_assert(
    Config::I2c::SHT30_ADDRESS == 0x44 ||
        Config::I2c::SHT30_ADDRESS == 0x45,
    "SHT30 address must be 0x44 or 0x45"
);

Sht30Sensor::Sht30Sensor()
    : sensor_(
          Config::I2c::SHT30_ADDRESS == 0x45
              ? SHTSensor::SHT3X_ALT
              : SHTSensor::SHT3X
      )
{
}

bool Sht30Sensor::begin()
{
    ready_ = sensor_.init();
    return ready_;
}

bool Sht30Sensor::isReady() const
{
    return ready_;
}

SHTSensor &Sht30Sensor::device()
{
    return sensor_;
}
