#pragma once

#include <Adafruit_SSD1306.h>
#include <Wire.h>
#include <string.h>
#include "Config.h"

// Optional display: a missing OLED must not prevent serial/flash recovery.
// Nothing is printed to Serial here; that stream carries the upload protocol.
class UploadDisplay {
public:
    UploadDisplay()
        : display_(Config::I2c::OLED_WIDTH, Config::I2c::OLED_HEIGHT,
                   &Wire, Config::Pins::OLED_RESET) {}

    bool begin() {
        ready_ = Wire.begin(Config::Pins::I2C_SDA, Config::Pins::I2C_SCL,
                            Config::I2c::CLOCK_HZ) &&
                 display_.begin(SSD1306_SWITCHCAPVCC, Config::I2c::OLED_ADDRESS, true, false);
        return ready_;
    }

    void show(const char *phase, uint32_t completed = 0, uint32_t total = 0) {
        if (!ready_) return;
        const uint32_t now = millis();
        const bool phaseChanged = !lastPhase_ || strcmp(lastPhase_, phase) != 0;
        const bool finished = total && completed >= total;
        if (!phaseChanged && !finished && now - lastRefreshMs_ < Config::Oled::REFRESH_INTERVAL_MS) return;
        lastRefreshMs_ = now;
        lastPhase_ = phase;
        display_.clearDisplay();
        display_.setTextColor(SSD1306_WHITE);
        display_.setTextSize(1);
        display_.setTextWrap(false);
        display_.setCursor(0, 0);
        display_.print("FANMASTER UPLOADER");
        display_.setCursor(0, 14);
        display_.print(phase);
        if (total) {
            const uint32_t done = min(completed, total);
            const uint32_t percent = static_cast<uint64_t>(done) * 100 / total;
            display_.setCursor(0, 28);
            display_.print(percent);
            display_.print('%');
            display_.setCursor(0, 40);
            display_.print(done);
            display_.print('/');
            display_.print(total);
            display_.print(" B");
            const int16_t width = display_.width() - 2;
            display_.drawRect(0, 53, width + 2, 10, SSD1306_WHITE);
            display_.fillRect(1, 54, static_cast<uint64_t>(done) * width / total, 8, SSD1306_WHITE);
        }
        display_.display();
    }

private:
    Adafruit_SSD1306 display_;
    bool ready_ = false;
    const char *lastPhase_ = nullptr;
    uint32_t lastRefreshMs_ = 0;
};
