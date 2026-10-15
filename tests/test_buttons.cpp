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
