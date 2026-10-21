#include <cassert>
#include <iostream>
#include <limits>
#include "ButtonControls.h"

struct Controls {
    ButtonInputs buttons;
    ButtonControls controls;
    bool powered = true, waitingForKey = false;
    explicit Controls(uint32_t start = 0) {
        fakeMillis = start;
        fakePinLevels.fill(HIGH);
        buttons.begin();
    }
    ButtonActions tick(uint32_t elapsed = 0) {
        fakeMillis += elapsed;
        buttons.update();
        return controls.update(buttons, powered, waitingForKey);
    }
    ButtonActions set(ButtonId button, bool down) {
        fakePinLevels[Config::Buttons::PINS[static_cast<size_t>(button)]] = down ? LOW : HIGH;
        tick();
        return tick(Config::Buttons::DEBOUNCE_MS);
    }
};

void powerGestures()
{
    for (uint32_t start : {0U, std::numeric_limits<uint32_t>::max() - 4000}) {
        Controls c(start);
        c.set(ButtonId::Button4, true);
        assert(!c.tick(7999).powerOffRequested);
        assert(c.tick(1).powerOffRequested);
        assert(!c.tick(9000).powerOffRequested); // Exactly once per hold.
        c.powered = false; c.tick();
        assert(c.set(ButtonId::Button4, false).powerHintRequested);
        auto press = c.set(ButtonId::Button3, true);
        assert(press.powerHintRequested && !press.powerOnRequested && !press.toggleLed);
        assert(!c.set(ButtonId::Button3, false).modeSwitchRequested);
        c.set(ButtonId::Button1, true);
        c.set(ButtonId::Button4, true);
        assert(!c.tick(999).powerOnRequested);
        auto wake = c.tick(1);
        assert(wake.powerOnRequested && wake.direction == 0 && !wake.powerOffRequested);
        c.powered = true; c.tick();
        assert(!c.tick(9000).powerOffRequested); // Wake chord is consumed until release.
        assert(!c.set(ButtonId::Button4, false).modeSwitchRequested);
        assert(c.set(ButtonId::Button1, false).direction == 0);
        c.set(ButtonId::Button4, true);
        assert(c.tick(8000).powerOffRequested);
    }
    for (auto other : {ButtonId::Button1, ButtonId::Button2, ButtonId::Button3}) {
        Controls c;
        c.set(ButtonId::Button4, true); c.tick(2000);
        c.set(other, true); c.set(other, false);
        assert(!c.tick(20000).powerOffRequested); // No restart during this function hold.
        c.set(ButtonId::Button4, false); c.set(ButtonId::Button4, true);
        assert(c.tick(8000).powerOffRequested);
        Controls preheld;
        preheld.set(other, true); preheld.set(ButtonId::Button4, true);
        preheld.set(other, false);
        assert(!preheld.tick(20000).powerOffRequested);
    }
    {
        Controls c;
        c.set(ButtonId::Button4, true); c.tick(7999);
        fakePinLevels[Config::Pins::BUTTON_4] = HIGH;
        assert(!c.tick(1).powerOffRequested); // Raw release takes precedence at threshold.
        fakePinLevels[Config::Pins::BUTTON_4] = LOW;
        assert(!c.tick(9000).powerOffRequested); // Bounce cannot re-arm the same press.
    }
    {
        Controls c;
        c.set(ButtonId::Button4, true);
        fakePinLevels[Config::Pins::BUTTON_2] = LOW;
        c.tick(8000); // Raw other-key press aborts even before debounce.
        fakePinLevels[Config::Pins::BUTTON_2] = HIGH;
        assert(!c.tick(10000).powerOffRequested);
    }
    for (auto key : {ButtonId::Button1, ButtonId::Button2, ButtonId::Button3, ButtonId::Button4}) {
        Controls c;
        c.powered = false; c.waitingForKey = true; c.tick();
        auto action = c.set(key, true);
        assert(action.powerOnRequested && action.direction == 0 && !action.toggleLed && !action.modeSwitchRequested);
        c.powered = true; c.tick();
        auto release = c.set(key, false);
        assert(!release.modeSwitchRequested && release.direction == 0);
    }
    for (bool functionFirst : {false, true}) {
        Controls c;
        c.powered = false; c.tick();
        c.set(functionFirst ? ButtonId::Button4 : ButtonId::Button1, true);
        c.set(functionFirst ? ButtonId::Button1 : ButtonId::Button4, true);
        c.tick(900);
        c.set(ButtonId::Button2, true);
        assert(!c.tick(1000).powerOnRequested);
        c.set(ButtonId::Button2, false);
        assert(!c.tick(999).powerOnRequested);
        assert(c.tick(1).powerOnRequested);
    }
    {
        Controls c;
        c.set(ButtonId::Button3, true);
        c.powered = false; c.tick(); // Remote power-off consumes an in-progress click.
        assert(!c.set(ButtonId::Button3, false).modeSwitchRequested);
        assert(!c.tick(9000).powerOnRequested);
    }
}

int main()
{
    powerGestures();
    {
        Controls c;
        // Bounce shorter than debounce does not step.
        fakePinLevels[Config::Pins::BUTTON_1] = LOW;
        assert(c.tick().direction == 0);
        assert(c.tick(Config::Buttons::DEBOUNCE_MS - 1).direction == 0);
        fakePinLevels[Config::Pins::BUTTON_1] = HIGH;
        assert(c.tick().direction == 0);
        assert(c.set(ButtonId::Button1, true).direction == 1);
        assert(c.buttons.wasPressed(ButtonId::Button1));
        assert(c.tick(999).direction == 0);
        assert(c.tick(1).direction == 1);
        assert(c.buttons.wasRepeated(ButtonId::Button1));
        assert(!c.buttons.wasPressed(ButtonId::Button1));
        assert(c.tick(99).direction == 0);
        assert(c.tick(1).direction == 1);
        assert(c.tick(5000).direction == 1); // One event, no catch-up burst.
        assert(c.tick().direction == 0);
        assert(c.set(ButtonId::Button1, false).direction == 0);
        assert(c.tick(2000).direction == 0);
        assert(c.set(ButtonId::Button1, true).direction == 1);
        assert(c.tick(999).direction == 0); // New hold gets a fresh delay.
    }
    {
        Controls c;
        assert(c.set(ButtonId::Button4, true).toggleLed == false);
        assert(c.set(ButtonId::Button2, true).direction == -1);
        const auto fineRepeat = c.tick(1000);
        assert(fineRepeat.direction == -1 && fineRepeat.fine);
        c.set(ButtonId::Button4, false);
        const auto normalRepeat = c.tick(100);
        assert(normalRepeat.direction == -1 && !normalRepeat.fine); // Modifier can change mid-hold.
        c.set(ButtonId::Button1, true);
        assert(c.tick(1000).direction == 0); // Opposing held keys cancel.
        assert(c.tick(100).direction == 0);
    }
    for (bool functionFirst : {false, true}) {
        Controls c;
        if (functionFirst) {
            assert(!c.set(ButtonId::Button4, true).toggleLed);
            assert(c.set(ButtonId::Button3, true).toggleLed);
        } else {
            assert(!c.set(ButtonId::Button3, true).modeSwitchRequested);
            assert(c.set(ButtonId::Button4, true).toggleLed);
        }
        assert(!c.tick(2000).toggleLed);
        assert(!c.buttons.wasRepeated(ButtonId::Button3));
        assert(!c.buttons.wasRepeated(ButtonId::Button4));
        // Releasing/repressing the function key during the same button-3 hold
        // must not retrigger the LED or leak a standalone mode-switch event.
        c.set(ButtonId::Button4, false);
        assert(!c.set(ButtonId::Button4, true).toggleLed);
        assert(!c.set(ButtonId::Button3, false).modeSwitchRequested);
    }
    {
        Controls c;
        assert(!c.set(ButtonId::Button3, true).modeSwitchRequested);
        const auto click = c.set(ButtonId::Button3, false);
        assert(click.modeSwitchRequested && !click.toggleLed && click.direction == 0);
        assert(!c.tick().modeSwitchRequested);
        c.set(ButtonId::Button3, true);
        assert(c.set(ButtonId::Button1, true).direction == 1); // Button 3 is no modifier.
    }
    {
        Controls c(std::numeric_limits<uint32_t>::max() - 500);
        assert(c.set(ButtonId::Button1, true).direction == 1);
        assert(c.tick(999).direction == 0);
        assert(c.tick(1).direction == 1); // millis rollover.
        assert(c.tick(100).direction == 1);
        assert(!c.buttons.wasActivated(ButtonId::Count));
    }
    std::cout << "Button debounce, repeat timing, rollover and chord tests passed\n";
}
