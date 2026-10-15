#pragma once

#include <Arduino.h>

#include "Config.h"

enum class ButtonId : uint8_t {
    Button1 = 0,
    Button2,
    Button3,
    Button4,
    Count,
};

class ButtonInputs {
public:
    void begin();
    void update();

    bool isPressed(ButtonId button) const;
    bool wasPressed(ButtonId button) const;
    bool wasReleased(ButtonId button) const;
    bool wasRepeated(ButtonId button) const;
    bool wasActivated(ButtonId button) const;

private:
    struct ButtonState {
        uint32_t rawChangedAtMs = 0;
        uint32_t pressedAtMs = 0;
        uint32_t lastRepeatMs = 0;
        bool rawPressed = false;
        bool stablePressed = false;
        bool pressedEvent = false;
        bool releasedEvent = false;
        bool repeatedEvent = false;
        bool repeating = false;
    };

    static size_t indexOf(ButtonId button);
    static bool isValid(ButtonId button);

    ButtonState states_[Config::Buttons::COUNT];
};
