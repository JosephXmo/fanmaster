# Hardware and configuration

| Function | Default configuration |
| --- | --- |
| Fan TACH | GPIO 1, active falling edge |
| Fan PWM | GPIO 2, 25 kHz, 8-bit |
| Fan supply relay input | GPIO 13, active high (configurable) |
| Onboard WS2812 RGB status LED | GPIO 21 |
| I2C SDA | GPIO 3 |
| I2C SCL | GPIO 4 |
| Button 1 | GPIO 7, active low |
| Button 2 | GPIO 8, active low |
| Button 3 | GPIO 9, active low |
| Button 4 | GPIO 10, active low |
| SSD1306 address | `0x3C` |
| SSD1306 reset | `-1` (no dedicated reset pin) |
| SHT30 address | `0x44` |
| W25Q64 SCK / MOSI / MISO / CS | GPIO 11 / 5 / 6 / 12 |

Buttons are expected to connect their GPIO input to ground when pressed. The
current carrier board provides external pull-ups, so internal pull-ups are
disabled. Enable `Config::Buttons::ENABLE_INTERNAL_PULLUPS` if external pull-ups
are absent.

Do not wire a switch so that pressing it directly shorts VCC to ground.

## Arduino library dependencies

- ESP32 Arduino core 3.x
- Adafruit SSD1306
- Adafruit GFX Library
- Sensirion arduino-sht (`SHTSensor.h`)
- FastLED 3.10.5 or later
- SPIMemory 3.4.0

The Adafruit GFX dependency is normally installed automatically with Adafruit
SSD1306. The display, sensor, and WS2812 libraries are not vendored in this
repository. Validation uses FastLED 3.10.7, arduino-sht 2.0.0, SPIMemory 3.4.0,
and ESP32 Arduino core 3.3.12.

## Configuration ownership

Edit the original `src/fanmaster/Config.h`, then stage/rebuild the affected
applications. Staged copies in `build/` are disposable. Both applications must use
the same wiring and relay polarity. Pin-collision checks run at compile time.

The current power thresholds are 3000 ms for shutdown and 500 ms for the wake
chord. See [local controls](controls.md) and [fan supply](power.md).
