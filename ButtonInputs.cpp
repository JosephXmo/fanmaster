#include "ButtonInputs.h"

static_assert(
    Config::Buttons::COUNT == static_cast<size_t>(ButtonId::Count),
    "Button pin count must match ButtonId"
);

void ButtonInputs::begin()
{
    const uint32_t now = millis();

    for (size_t index = 0; index < Config::Buttons::COUNT; index++) {
        pinMode(
            Config::Buttons::PINS[index],
            Config::Buttons::ENABLE_INTERNAL_PULLUPS ? INPUT_PULLUP : INPUT
        );

        const bool pressed = digitalRead(Config::Buttons::PINS[index]) == LOW;
        states_[index].rawPressed = pressed;
        states_[index].stablePressed = pressed;
        states_[index].rawChangedAtMs = now;
    }
}

void ButtonInputs::update()
{
    const uint32_t now = millis();

    for (size_t index = 0; index < Config::Buttons::COUNT; index++) {
        ButtonState &state = states_[index];
        state.pressedEvent = false;
        state.releasedEvent = false;

        const bool rawPressed =
            digitalRead(Config::Buttons::PINS[index]) == LOW;

        if (rawPressed != state.rawPressed) {
            state.rawPressed = rawPressed;
            state.rawChangedAtMs = now;
        }

        if (state.rawPressed == state.stablePressed ||
            now - state.rawChangedAtMs < Config::Buttons::DEBOUNCE_MS) {
            continue;
        }

        state.stablePressed = state.rawPressed;
        state.pressedEvent = state.stablePressed;
        state.releasedEvent = !state.stablePressed;
    }
}

bool ButtonInputs::isPressed(ButtonId button) const
{
    return isValid(button) && states_[indexOf(button)].stablePressed;
}

bool ButtonInputs::wasPressed(ButtonId button) const
{
    return isValid(button) && states_[indexOf(button)].pressedEvent;
}

bool ButtonInputs::wasReleased(ButtonId button) const
{
    return isValid(button) && states_[indexOf(button)].releasedEvent;
}

size_t ButtonInputs::indexOf(ButtonId button)
{
    return static_cast<size_t>(button);
}

bool ButtonInputs::isValid(ButtonId button)
{
    return indexOf(button) < Config::Buttons::COUNT;
}
