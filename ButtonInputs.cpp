#include "ButtonInputs.h"

static_assert(
    Config::Buttons::COUNT == static_cast<size_t>(ButtonId::Count),
    "Button pin count must match ButtonId"
);
static_assert(Config::Buttons::REPEAT_INTERVAL_MS > 0, "Repeat interval must be positive");

void ButtonInputs::begin()
{
    const uint32_t now = millis();

    for (size_t index = 0; index < Config::Buttons::COUNT; index++) {
        pinMode(
            Config::Buttons::PINS[index],
            Config::Buttons::ENABLE_INTERNAL_PULLUPS ? INPUT_PULLUP : INPUT
        );

        const bool pressed = digitalRead(Config::Buttons::PINS[index]) == LOW;
        states_[index] = {};
        states_[index].rawPressed = pressed;
        states_[index].stablePressed = pressed;
        states_[index].rawChangedAtMs = now;
        states_[index].pressedAtMs = now;
    }
}

void ButtonInputs::update()
{
    const uint32_t now = millis();

    for (size_t index = 0; index < Config::Buttons::COUNT; index++) {
        ButtonState &state = states_[index];
        state.pressedEvent = false;
        state.releasedEvent = false;
        state.repeatedEvent = false;

        const bool rawPressed =
            digitalRead(Config::Buttons::PINS[index]) == LOW;

        if (rawPressed != state.rawPressed) {
            state.rawPressed = rawPressed;
            state.rawChangedAtMs = now;
        }

        if (state.rawPressed != state.stablePressed &&
            now - state.rawChangedAtMs >= Config::Buttons::DEBOUNCE_MS) {
            state.stablePressed = state.rawPressed;
            state.pressedEvent = state.stablePressed;
            state.releasedEvent = !state.stablePressed;
            state.pressedAtMs = now;
            state.repeating = false;
        }

        // Only adjustment buttons repeat. Emit one event per update, never a
        // burst of catch-up steps after a delayed loop. Suppress on raw release.
        if (index <= indexOf(ButtonId::Button2) && state.stablePressed &&
            state.rawPressed && !state.pressedEvent &&
            now - state.pressedAtMs >= Config::Buttons::REPEAT_DELAY_MS &&
            (!state.repeating || now - state.lastRepeatMs >= Config::Buttons::REPEAT_INTERVAL_MS)) {
            state.repeatedEvent = true;
            state.repeating = true;
            state.lastRepeatMs = now;
        }
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

bool ButtonInputs::isRawPressed(ButtonId button) const
{
    return isValid(button) && states_[indexOf(button)].rawPressed;
}

bool ButtonInputs::wasReleased(ButtonId button) const
{
    return isValid(button) && states_[indexOf(button)].releasedEvent;
}

bool ButtonInputs::wasRepeated(ButtonId button) const
{
    return isValid(button) && states_[indexOf(button)].repeatedEvent;
}

bool ButtonInputs::wasActivated(ButtonId button) const
{
    return wasPressed(button) || wasRepeated(button);
}

size_t ButtonInputs::indexOf(ButtonId button)
{
    return static_cast<size_t>(button);
}

bool ButtonInputs::isValid(ButtonId button)
{
    return indexOf(button) < Config::Buttons::COUNT;
}
