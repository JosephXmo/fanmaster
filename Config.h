#pragma once

#include <Arduino.h>

namespace Config {

namespace Pins {
constexpr uint8_t FAN_TACH = 4;
constexpr uint8_t FAN_PWM = 5;
constexpr uint8_t RGB_LED = 38;
// External W25Q64, ordinary single-bit SPI. CS must be driven, not tied high.
constexpr uint8_t FLASH_SCK = 8;
constexpr uint8_t FLASH_MOSI = 9;
constexpr uint8_t FLASH_MISO = 10;
constexpr uint8_t FLASH_CS = 15;

// Both reserved I2C connectors are wired in parallel to this bus.
constexpr uint8_t I2C_SDA = 6;
constexpr uint8_t I2C_SCL = 7;

// Active-low buttons. Change these four values to match the carrier board.
constexpr uint8_t BUTTON_1 = 11;
constexpr uint8_t BUTTON_2 = 12;
constexpr uint8_t BUTTON_3 = 13;
constexpr uint8_t BUTTON_4 = 14;

// Adafruit_SSD1306 uses -1 when the display has no dedicated reset pin.
constexpr int8_t OLED_RESET = -1;
}  // namespace Pins

namespace Fan {
constexpr bool PWM_INVERTED = true;
constexpr uint32_t PULSES_PER_REVOLUTION = 2;
constexpr uint32_t RPM_SAMPLE_INTERVAL_MS = 1000;
constexpr uint32_t PWM_FREQUENCY_HZ = 25000;
constexpr uint8_t PWM_RESOLUTION_BITS = 8;
constexpr uint32_t PWM_MAX_VALUE = (1UL << PWM_RESOLUTION_BITS) - 1;
constexpr uint8_t DEFAULT_DUTY_PERCENT = 35;
constexpr uint32_t STARTUP_TIME_MS = 1000;
}  // namespace Fan

namespace StatusLed {
enum class ResponseSource : uint8_t {
    DUTY,
    TACH,
};

enum class IndicationMode : uint8_t {
    FLASH,
    SPECTRUM,
    FLASH_AND_SPECTRUM,
};

// Use measured TACH speed for LED indication; no circuit changes are required.
constexpr ResponseSource RESPONSE_SOURCE = ResponseSource::TACH;
constexpr IndicationMode INDICATION_MODE = IndicationMode::SPECTRUM;
constexpr uint16_t LED_COUNT = 1;
constexpr uint8_t BRIGHTNESS = 32;
constexpr uint8_t SLOW_DUTY_PERCENT = 1;
constexpr uint8_t FAST_DUTY_PERCENT = 100;
constexpr uint32_t SLOW_RPM = 550;
constexpr uint32_t FAST_RPM = 2800;
constexpr uint32_t SLOW_FLASH_INTERVAL_MS = 1200;
constexpr uint32_t FAST_FLASH_INTERVAL_MS = 150;
constexpr uint32_t FLASH_ON_TIME_MS = 120;
// Used by FLASH mode, encoded as 0xRRGGBB.
constexpr uint32_t FLASH_COLOR_RGB = 0xFF0000;
constexpr uint8_t VIOLET_HUE = 192;
constexpr uint8_t SATURATION = 255;
constexpr uint8_t VALUE = 255;
}  // namespace StatusLed

namespace SerialConsole {
constexpr uint32_t BAUD_RATE = 115200;
constexpr bool PLOTTER_MODE = false;
constexpr size_t INPUT_BUFFER_SIZE = 24;
}  // namespace SerialConsole

namespace Wifi {
// Credentials are provisioned over the setup AP and stored in ESP32 NVS.
constexpr char SETUP_AP_PREFIX[] = "Fanmaster-Setup-";
constexpr uint32_t CONNECT_TIMEOUT_MS = 15000;
constexpr uint32_t RETRY_INTERVAL_MS = 10000;
constexpr uint32_t SETUP_AFTER_FAILURE_MS = 45000;
// Keep the setup AP until credentials connect and are saved successfully.
constexpr uint32_t SETUP_CLOSE_GRACE_MS = 5000;
constexpr uint32_t SETUP_RETRY_INTERVAL_MS = 10000;
}  // namespace Wifi

namespace Storage {
constexpr uint32_t SPI_CLOCK_HZ = 10000000;
constexpr size_t HTTP_CHUNK_BYTES = 1024;
constexpr uint32_t HTTP_WRITE_TIMEOUT_MS = 1000;
}  // namespace Storage

namespace Web {
constexpr uint16_t HTTP_PORT = 80;
constexpr uint32_t POLL_INTERVAL_MS = 1000;
}  // namespace Web

namespace Diagnostics {
constexpr uint32_t REPORT_INTERVAL_MS = 10000;
}  // namespace Diagnostics

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
constexpr uint8_t DUTY_STEP_PERCENT = 5;
constexpr uint8_t MODIFIED_DUTY_STEP_PERCENT = 1;
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

namespace Oled {
constexpr uint32_t REFRESH_INTERVAL_MS = 250;
}  // namespace Oled

}  // namespace Config
