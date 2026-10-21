#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include "StartFan.h"
#include "OledDisplay.h"

void bootAndKick()
{
    fakeMillis = 100;
    FanController fan;
    OperatingSettings saved;
    saved.dutyPercent = 43; saved.targetRpm = 930;
    fan.restoreSettings(saved);
    assert(fan.begin() && millis() == 100);
    auto state = fan.snapshot();
    assert(!state.powerEnabled && !state.powerStarting && state.waitingForPowerKey);
    assert(state.appliedDutyPercent == 0 && state.manualDutyPercent == 43 && state.targetRpm == 930);
    assert(fakePinModes[Config::Pins::FAN_POWER] == OUTPUT);
    assert(fakePinLevels[Config::Pins::FAN_POWER] == (Config::Power::RELAY_ACTIVE_HIGH ? LOW : HIGH));
    assert(!fan.setDutyPercent(80) && !fan.setTargetRpm(1200) && !fan.setMode(FanMode::Rpm));
    fan.toggleMode(); fan.adjustSetting(1, false);
    assert(fan.snapshot().manualDutyPercent == 43 && fan.snapshot().mode == FanMode::Duty);
    fan.setPowerEnabled(true);
    const auto revision = fan.snapshot().controlRevision;
    assert(fan.snapshot().powerStarting && fan.dutyPercent() == 100);
    assert(fakePinLevels[Config::Pins::FAN_POWER] == (Config::Power::RELAY_ACTIVE_HIGH ? HIGH : LOW));
    fakeMillis += 999; fan.updatePower(); assert(fan.dutyPercent() == 100);
    fan.setPowerEnabled(true); assert(fan.snapshot().controlRevision == revision);
    ++fakeMillis; fan.updatePower();
    assert(!fan.snapshot().powerStarting && fan.dutyPercent() == 43);
    fan.setPowerEnabled(false);
    assert(!fan.snapshot().waitingForPowerKey && fan.dutyPercent() == 0);
    // Repeated off commands neither restart anything nor change the revision.
    const auto offRevision = fan.snapshot().controlRevision;
    fan.setPowerEnabled(false); assert(fan.snapshot().controlRevision == offRevision);
    fakeMillis = std::numeric_limits<uint32_t>::max() - 500;
    fan.setPowerEnabled(true); fakeMillis += 1000; fan.updatePower();
    assert(fan.dutyPercent() == 43 && !fan.snapshot().powerStarting);
}

void feedbackAndThermal()
{
    FanController fan;
    assert(fan.begin()); startFan(fan);
    fan.setDutyPercent(42); fan.setTargetRpm(850);
    for (int i = 0; i < 28; ++i) fakeTachInterrupt();
    fakeMillis += 1000; float rpm; assert(fan.updateRpm(rpm));
    const auto before = fan.snapshot();
    fan.setPowerEnabled(false);
    for (int i = 0; i < 40; ++i) {
        fakeMillis += 1000; assert(fan.updateRpm(rpm));
        assert(fan.dutyPercent() == 0); // Stopped TACH must not wind up PI.
    }
    fan.setPowerEnabled(true);
    for (int i = 0; i < 40; ++i) fakeTachInterrupt();
    fakeMillis += 1000; assert(fan.updateRpm(rpm) && fan.dutyPercent() == 100);
    fan.updatePower();
    assert(fan.snapshot().mode == FanMode::Rpm && fan.snapshot().targetRpm == 850);
    assert(fan.dutyPercent() == before.appliedDutyPercent);
    assert(!fan.updateRpm(rpm)); // Fresh measurement window starts after boost.
    for (int i = 0; i < 28; ++i) fakeTachInterrupt();
    fakeMillis += 1000; assert(fan.updateRpm(rpm) && rpm == 840);
    assert(fan.dutyPercent() == before.appliedDutyPercent);
    fan.updateTemperature({true, 32.5f, 50, millis()});
    assert(fan.setThermalEnabled(true) && fan.dutyPercent() == 50);
    fan.setPowerEnabled(false);
    fan.updateTemperature({true, 40, 50, millis()});
    assert(fan.snapshot().mode == FanMode::Thermal && fan.dutyPercent() == 0);
    assert(!fan.setThermalEnabled(true));
    auto curve = ThermalCurve{}; curve.points[0].dutyPercent = 40;
    assert(fan.setThermalCurve(curve) && fan.dutyPercent() == 0);
    fan.setPowerEnabled(true);
    fakeMillis += 1000; fan.updatePower();
    assert(fan.snapshot().mode == FanMode::Thermal && fan.dutyPercent() == 65);
    fan.setPowerEnabled(false);
    fan.updateTemperature({}); // Sensor failure while off still disarms thermal.
    assert(fan.snapshot().mode == FanMode::Rpm && fan.dutyPercent() == 0);
    fan.updateTemperature({true, 40, 50, millis()});
    fan.setPowerEnabled(true); fakeMillis += 1000; fan.updatePower();
    assert(fan.snapshot().mode == FanMode::Rpm && fan.snapshot().targetRpm == 850);
    assert(fan.snapshot().manualDutyPercent == 42);
    assert(fan.setThermalEnabled(true));
    fan.setPowerEnabled(false);
    fakeMillis += Config::Thermal::SENSOR_STALE_MS + 1;
    fan.setPowerEnabled(true); fakeMillis += 1000; fan.updatePower();
    assert(fan.snapshot().mode == FanMode::Rpm); // Expiry checked even without a new read.
}

void interruptedKick()
{
    FanController fan;
    assert(fan.begin()); startFan(fan);
    fan.setDutyPercent(42); fan.setTargetRpm(850);
    fan.setPowerEnabled(false); fan.setPowerEnabled(true);
    fakeMillis += 200; fan.setPowerEnabled(false);
    assert(fan.dutyPercent() == 0);
    fan.setPowerEnabled(true); fakeMillis += 1000; fan.updatePower();
    assert(fan.dutyPercent() == 42); // Never resume the interrupted 100% boost as PI output.
    fan.setPowerEnabled(false); fan.setPowerEnabled(true);
    fan.setDutyPercent(51); // Changes during the boost take effect after its deadline.
    assert(fan.dutyPercent() == 100);
    fakeMillis += 1000; fan.updatePower(); assert(fan.dutyPercent() == 51);
}

void displayTests()
{
    fakeMillis = std::numeric_limits<uint32_t>::max() - 500;
    fakeOledFrames.clear(); fakeOledCommands.clear();
    OledDisplay oled(Wire);
    assert(oled.begin());
    oled.showPowerOff(true);
    assert(fakeOledFrames.back().find("Press any key to\npower on.") != std::string::npos);
    const auto frames = fakeOledFrames.size();
    oled.showPowerOff(true); assert(fakeOledFrames.size() == frames);
    oled.showPowerOff(false); assert(fakeOledCommands.back() == SSD1306_DISPLAYOFF);
    oled.requestPowerHint(); oled.showPowerOff(false);
    assert(fakeOledFrames.back().find("Hold ^ and * for\n1 second to power on\nthe fan.") != std::string::npos);
    assert(fakeOledCommands.back() == SSD1306_DISPLAYON);
    fakeMillis += 1499; oled.showPowerOff(false);
    assert(fakeOledCommands.back() == SSD1306_DISPLAYON);
    ++fakeMillis; oled.showPowerOff(false);
    assert(fakeOledCommands.back() == SSD1306_DISPLAYOFF);
    oled.requestPowerHint(); oled.showPowerOff(false);
    fakeMillis += 1000; oled.requestPowerHint(); fakeMillis += 1000; oled.showPowerOff(false);
    assert(fakeOledCommands.back() == SSD1306_DISPLAYON); // New operation renews 1.5 s.
    oled.showStatus(840, 40, true, IPAddress{123}, "1.4", true, 25, 50);
    assert(fakeOledFrames.back().find("FANMASTER") != std::string::npos);
    oled.showPowerOff(false); assert(fakeOledCommands.back() == SSD1306_DISPLAYOFF);
    fakeOledAvailable = false;
    OledDisplay missing(Wire); assert(!missing.begin());
    const auto commands = fakeOledCommands.size();
    missing.requestPowerHint(); missing.showPowerOff(false);
    assert(fakeOledCommands.size() == commands);
    fakeOledAvailable = true;
}

int main()
{
    bootAndKick(); feedbackAndThermal(); interruptedKick(); displayTests();
    std::cout << "Relay boot-off, nonblocking kick, feedback suspension, mode recovery and OLED power tests passed\n";
}
