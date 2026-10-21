#pragma once

#include "ButtonInputs.h"

static_assert(Config::Power::SHUTDOWN_HOLD_MS > 0 && Config::Power::SHUTDOWN_HOLD_MS < 0x80000000UL &&
              Config::Power::WAKE_HOLD_MS > 0 && Config::Power::WAKE_HOLD_MS < 0x80000000UL &&
              Config::Power::HINT_MS > 0 && Config::Power::HINT_MS < 0x80000000UL,
              "Power gesture/display intervals must be positive and rollover-safe");

struct ButtonActions {
    int8_t direction = 0;
    bool fine = false;
    bool toggleLed = false;
    bool modeSwitchRequested = false;
    bool powerOffRequested = false;
    bool powerOnRequested = false;
    bool powerHintRequested = false;
};

// Interpret debounced input separately from hardware outputs. Button 3 clicks
// are resolved on release so either press order of the function chord works.
class ButtonControls {
public:
    ButtonActions update(const ButtonInputs &buttons, bool powered = true,
                         bool waitingForPowerKey = false) {
        ButtonActions actions;
        const uint32_t now = millis();
        if (powered != lastPowered_) {
            lastPowered_ = powered;
            waitForRelease_ = true;
            shutdownArmed_ = wakeArmed_ = false;
            button3Chord_ = false;
        }
        bool anyHeld = false, anyEdge = false;
        bool otherHeld = false;
        for (size_t i = 0; i < Config::Buttons::COUNT; ++i) {
            const auto key = static_cast<ButtonId>(i);
            const bool held = buttons.isPressed(key) || buttons.isRawPressed(key);
            anyHeld |= held;
            anyEdge |= buttons.wasPressed(key) || buttons.wasReleased(key);
            if (key != ButtonId::Button4)
                otherHeld |= held || buttons.wasReleased(key);
        }
        if (!powered && anyEdge) actions.powerHintRequested = true;
        if (waitForRelease_) {
            // Power transitions consume the entire gesture, including releases.
            if (!anyHeld) waitForRelease_ = false;
            return actions;
        }
        if (!powered) {
            if (waitingForPowerKey) {
                for (size_t i = 0; i < Config::Buttons::COUNT; ++i)
                    actions.powerOnRequested |= buttons.wasPressed(static_cast<ButtonId>(i));
            } else {
                const bool chord = buttons.isPressed(ButtonId::Button1) && buttons.isRawPressed(ButtonId::Button1) &&
                    buttons.isPressed(ButtonId::Button4) && buttons.isRawPressed(ButtonId::Button4) &&
                    !buttons.isPressed(ButtonId::Button2) && !buttons.isRawPressed(ButtonId::Button2) &&
                    !buttons.isPressed(ButtonId::Button3) && !buttons.isRawPressed(ButtonId::Button3);
                if (!chord) wakeArmed_ = false;
                else if (!wakeArmed_) { wakeArmed_ = true; wakeStartedAtMs_ = now; }
                else if (now - wakeStartedAtMs_ >= Config::Power::WAKE_HOLD_MS)
                    actions.powerOnRequested = true;
            }
            if (actions.powerOnRequested) waitForRelease_ = true;
            return actions;
        }
        const bool function = buttons.isPressed(ButtonId::Button4);
        // Once cancelled, do not re-arm until a fresh function-key press.
        if (buttons.wasPressed(ButtonId::Button4)) {
            shutdownArmed_ = !otherHeld;
            shutdownStartedAtMs_ = now;
        }
        if (!function || !buttons.isRawPressed(ButtonId::Button4) || otherHeld)
            shutdownArmed_ = false;
        if (shutdownArmed_ && now - shutdownStartedAtMs_ >= Config::Power::SHUTDOWN_HOLD_MS) {
            actions.powerOffRequested = true;
            shutdownArmed_ = false;
            waitForRelease_ = true;
            button3Chord_ = false;
            return actions;
        }
        const bool increase = buttons.wasActivated(ButtonId::Button1);
        const bool decrease = buttons.wasActivated(ButtonId::Button2);
        // Opposing held buttons cancel even when their repeat timers differ.
        if (increase != decrease &&
            !(buttons.isPressed(ButtonId::Button1) && buttons.isPressed(ButtonId::Button2))) {
            actions.direction = increase ? 1 : -1;
            actions.fine = function;
        }
        if (buttons.wasPressed(ButtonId::Button3)) button3Chord_ = false;
        if (buttons.isPressed(ButtonId::Button3) && function && !button3Chord_) {
            actions.toggleLed = true;
            button3Chord_ = true;
        }
        if (buttons.wasReleased(ButtonId::Button3)) {
            actions.modeSwitchRequested = !button3Chord_ && !function;
            button3Chord_ = false;
        }
        return actions;
    }
private:
    bool button3Chord_ = false;
    bool lastPowered_ = true;
    bool waitForRelease_ = false;
    bool shutdownArmed_ = false, wakeArmed_ = false;
    uint32_t shutdownStartedAtMs_ = 0, wakeStartedAtMs_ = 0;
};
