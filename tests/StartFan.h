#pragma once
#include "FanController.h"

// Existing operating-mode suites explicitly finish the new power-on sequence.
inline void startFan(FanController &fan)
{
    fan.setPowerEnabled(true);
    fakeMillis += Config::Fan::STARTUP_TIME_MS;
    fan.updatePower();
}
