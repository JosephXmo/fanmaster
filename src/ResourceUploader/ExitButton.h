#pragma once
#include "Config.h"

static_assert(Config::Updater::EXIT_HOLD_MS > 0 && Config::Updater::EXIT_HOLD_MS < 0x80000000UL,
              "Updater escape hold must be positive and rollover-safe");

// Raw release cancels immediately. Only the initial press is debounced; other
// buttons do not participate in the uploader's offline escape operation.
class ExitButton {
public:
    void begin() {
        pinMode(Config::Pins::BUTTON_4, Config::Buttons::ENABLE_INTERNAL_PULLUPS ? INPUT_PULLUP : INPUT);
        raw_ = digitalRead(Config::Pins::BUTTON_4) == LOW;
        rawChangedMs_ = millis();
        held_ = fired_ = false;
    }
    bool update() {
        const uint32_t now = millis();
        const bool raw = digitalRead(Config::Pins::BUTTON_4) == LOW;
        if (raw != raw_) { raw_ = raw; rawChangedMs_ = now; }
        if (!raw) { held_ = fired_ = false; return false; }
        if (!held_ && now - rawChangedMs_ >= Config::Buttons::DEBOUNCE_MS) {
            held_ = true;
            heldSinceMs_ = now;
        }
        if (!held_ || fired_ || now - heldSinceMs_ < Config::Updater::EXIT_HOLD_MS) return false;
        fired_ = true;
        return true;
    }
private:
    bool raw_ = false, held_ = false, fired_ = false;
    uint32_t rawChangedMs_ = 0, heldSinceMs_ = 0;
};
