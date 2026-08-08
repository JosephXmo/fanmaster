#pragma once

#include <Arduino.h>

namespace Config {

namespace Pins {
constexpr uint8_t FAN_TACH = 4;
constexpr uint8_t FAN_PWM = 5;
constexpr uint8_t RGB_LED = 38;

// Both reserved I2C connectors are wired in parallel to this bus.
constexpr uint8_t I2C_SDA = 8;
constexpr uint8_t I2C_SCL = 9;

// Active-low buttons. Change these four values to match the carrier board.
constexpr uint8_t BUTTON_1 = 10;
constexpr uint8_t BUTTON_2 = 11;
constexpr uint8_t BUTTON_3 = 12;
constexpr uint8_t BUTTON_4 = 13;

// Adafruit_SSD1306 uses -1 when the display has no dedicated reset pin.
constexpr int8_t OLED_RESET = -1;
}  // namespace Pins

namespace Fan {
constexpr bool PWM_INVERTED = false;
constexpr uint32_t PULSES_PER_REVOLUTION = 2;
constexpr uint32_t RPM_SAMPLE_INTERVAL_MS = 1000;
constexpr uint32_t PWM_FREQUENCY_HZ = 25000;
constexpr uint8_t PWM_RESOLUTION_BITS = 8;
constexpr uint32_t PWM_MAX_VALUE = (1UL << PWM_RESOLUTION_BITS) - 1;
constexpr uint8_t DEFAULT_DUTY_PERCENT = 35;
constexpr uint32_t STARTUP_TIME_MS = 1000;
}  // namespace Fan

namespace StatusLed {
constexpr uint32_t REVOLUTIONS_PER_FLASH = 500;
constexpr uint8_t RED_BRIGHTNESS = 204;
constexpr uint32_t FLASH_DURATION_MS = 150;
}  // namespace StatusLed

namespace SerialConsole {
constexpr uint32_t BAUD_RATE = 115200;
constexpr bool PLOTTER_MODE = true;
constexpr size_t INPUT_BUFFER_SIZE = 24;
}  // namespace SerialConsole

namespace Wifi {
// Replace these two strings before flashing the firmware.
constexpr char SSID[] = "YOUR_WIFI_SSID";
constexpr char PASSWORD[] = "YOUR_WIFI_PASSWORD";
constexpr uint32_t CONNECT_TIMEOUT_MS = 15000;
constexpr uint32_t RETRY_INTERVAL_MS = 10000;
}  // namespace Wifi

namespace Web {
constexpr uint16_t HTTP_PORT = 80;
constexpr uint32_t POLL_INTERVAL_MS = 1000;
}  // namespace Web

namespace Buttons {
constexpr size_t COUNT = 4;
constexpr uint8_t PINS[COUNT] = {
    Pins::BUTTON_1,
    Pins::BUTTON_2,
    Pins::BUTTON_3,
    Pins::BUTTON_4,
};
constexpr bool ENABLE_INTERNAL_PULLUPS = true;
constexpr uint32_t DEBOUNCE_MS = 30;
}  // namespace Buttons

namespace I2c {
constexpr uint32_t CLOCK_HZ = 100000;

// Reserved endpoint 1: SSD1306 OLED.
constexpr uint8_t OLED_ADDRESS = 0x3C;
constexpr uint16_t OLED_WIDTH = 128;
constexpr uint16_t OLED_HEIGHT = 64;

// Reserved endpoint 2: SHT30 temperature/humidity sensor.
constexpr uint8_t SHT30_ADDRESS = 0x44;
}  // namespace I2c

}  // namespace Config
