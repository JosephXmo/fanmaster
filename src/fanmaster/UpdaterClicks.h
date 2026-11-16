#pragma once
#include "ButtonInputs.h"

static_assert(Config::Updater::CLICK_COUNT > 0 && Config::Updater::CLICK_COUNT <= 255 &&
              Config::Updater::CLICK_INTERVAL_MS > 0 && Config::Updater::CLICK_INTERVAL_MS < 0x80000000UL &&
              Config::Updater::CLICK_MAX_HOLD_MS > 0 && Config::Updater::CLICK_MAX_HOLD_MS < 0x80000000UL,
              "Updater click limits must be positive and rollover-safe");

// Independent of fan power and the normal function-key gesture state machine.
class UpdaterClicks {
public:
    bool update(const ButtonInputs &buttons) {
        const uint32_t now = millis();
        if (count_ && now - lastClickMs_ > Config::Updater::CLICK_INTERVAL_MS) count_ = 0;
        for (size_t i = 0; i < Config::Buttons::COUNT - 1; ++i) {
            const auto key = static_cast<ButtonId>(i);
            if (buttons.isRawPressed(key) || buttons.isPressed(key) || buttons.wasReleased(key)) {
                count_ = 0;
                armed_ = false;
                return false;
            }
        }
        if (buttons.wasPressed(ButtonId::Button4)) {
            armed_ = true;
            pressedMs_ = now;
        }
        if (armed_ && now - pressedMs_ >= Config::Updater::CLICK_MAX_HOLD_MS) {
            armed_ = false;
            count_ = 0;
        }
        if (!buttons.wasReleased(ButtonId::Button4) || !armed_) return false;
        armed_ = false;
        lastClickMs_ = now;
        if (++count_ < Config::Updater::CLICK_COUNT) return false;
        count_ = 0;
        return true;
    }
private:
    uint8_t count_ = 0;
    bool armed_ = false;
    uint32_t pressedMs_ = 0, lastClickMs_ = 0;
};
