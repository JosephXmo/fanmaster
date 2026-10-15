#pragma once

#include "ButtonInputs.h"

struct ButtonActions {
    int8_t direction = 0;
    bool fine = false;
    bool toggleLed = false;
    bool modeSwitchRequested = false;
};

// Interpret debounced input separately from hardware outputs. Button 3 clicks
// are resolved on release so either press order of the function chord works.
class ButtonControls {
public:
    ButtonActions update(const ButtonInputs &buttons) {
        ButtonActions actions;
        const bool function = buttons.isPressed(ButtonId::Button4);
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
};
