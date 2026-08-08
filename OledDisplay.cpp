#include "OledDisplay.h"

#include "Config.h"

OledDisplay::OledDisplay(TwoWire &wire)
    : display_(
          Config::I2c::OLED_WIDTH,
          Config::I2c::OLED_HEIGHT,
          &wire,
          Config::Pins::OLED_RESET
      )
{
}

bool OledDisplay::begin()
{
    // I2cBus already owns Wire.begin(), so do not let the library reset it.
    ready_ = display_.begin(
        SSD1306_SWITCHCAPVCC,
        Config::I2c::OLED_ADDRESS,
        true,
        false
    );
    return ready_;
}

bool OledDisplay::isReady() const
{
    return ready_;
}

Adafruit_SSD1306 &OledDisplay::device()
{
    return display_;
}
