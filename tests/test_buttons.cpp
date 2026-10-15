#include <cassert>
#include <iostream>
#include <limits>
#include "ButtonControls.h"

struct Controls {
    ButtonInputs buttons;
    ButtonControls controls;
    explicit Controls(uint32_t start = 0) {
        fakeMillis = start;
        fakePinLevels.fill(HIGH);
        buttons.begin();
    }
    ButtonActions tick(uint32_t elapsed = 0) {
        fakeMillis += elapsed;
        buttons.update();
        return controls.update(buttons);
    }
    ButtonActions set(ButtonId button, bool down) {
        fakePinLevels[Config::Buttons::PINS[static_cast<size_t>(button)]] = down ? LOW : HIGH;
        tick();
        return tick(Config::Buttons::DEBOUNCE_MS);
    }
};

int main()
{
    {
        Controls c;
        // Bounce shorter than debounce does not step.
        fakePinLevels[Config::Pins::BUTTON_1] = LOW;
        assert(c.tick().dutyDelta == 0);
        assert(c.tick(Config::Buttons::DEBOUNCE_MS - 1).dutyDelta == 0);
        fakePinLevels[Config::Pins::BUTTON_1] = HIGH;
        assert(c.tick().dutyDelta == 0);
        assert(c.set(ButtonId::Button1, true).dutyDelta == 5);
        assert(c.buttons.wasPressed(ButtonId::Button1));
        assert(c.tick(999).dutyDelta == 0);
        assert(c.tick(1).dutyDelta == 5);
        assert(c.buttons.wasRepeated(ButtonId::Button1));
        assert(!c.buttons.wasPressed(ButtonId::Button1));
        assert(c.tick(99).dutyDelta == 0);
        assert(c.tick(1).dutyDelta == 5);
        assert(c.tick(5000).dutyDelta == 5); // One event, no catch-up burst.
        assert(c.tick().dutyDelta == 0);
        assert(c.set(ButtonId::Button1, false).dutyDelta == 0);
        assert(c.tick(2000).dutyDelta == 0);
        assert(c.set(ButtonId::Button1, true).dutyDelta == 5);
        assert(c.tick(999).dutyDelta == 0); // New hold gets a fresh delay.
    }
    {
        Controls c;
        assert(c.set(ButtonId::Button4, true).toggleLed == false);
        assert(c.set(ButtonId::Button2, true).dutyDelta == -1);
        assert(c.tick(1000).dutyDelta == -1);
        c.set(ButtonId::Button4, false);
        assert(c.tick(100).dutyDelta == -5); // Modifier can change mid-hold.
        c.set(ButtonId::Button1, true);
        assert(c.tick(1000).dutyDelta == 0); // Opposing held keys cancel.
        assert(c.tick(100).dutyDelta == 0);
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
        assert(click.modeSwitchRequested && !click.toggleLed && click.dutyDelta == 0);
        assert(!c.tick().modeSwitchRequested);
        c.set(ButtonId::Button3, true);
        assert(c.set(ButtonId::Button1, true).dutyDelta == 5); // Button 3 is no modifier.
    }
    {
        Controls c(std::numeric_limits<uint32_t>::max() - 500);
        assert(c.set(ButtonId::Button1, true).dutyDelta == 5);
        assert(c.tick(999).dutyDelta == 0);
        assert(c.tick(1).dutyDelta == 5); // millis rollover.
        assert(c.tick(100).dutyDelta == 5);
        assert(!c.buttons.wasActivated(ButtonId::Count));
    }
    std::cout << "Button debounce, repeat timing, rollover and chord tests passed\n";
}
