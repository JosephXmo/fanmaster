#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include "FanController.h"
#include "OperatingMemory.h"
#include "SerialConsole.h"
#include "AssetFormat.h"

void sample(FanController &fan, unsigned pulses, uint32_t elapsed = 1000)
{
    for (unsigned i = 0; i < pulses; ++i) fakeTachInterrupt();
    fakeMillis += elapsed;
    float rpm;
    assert(fan.updateRpm(rpm));
    assert(std::fabs(rpm - pulses * 60000.0f / elapsed / Config::Fan::PULSES_PER_REVOLUTION) < 0.1f);
}

void regulatorTests()
{
    RpmRegulator regulator;
    regulator.reset(35);
    float physical = 820, output = 35;
    // A delayed first-order illustrative plant plus alternating measurement noise.
    for (float target : {850.0f, 1400.0f, 650.0f}) {
        for (int i = 0; i < 180; ++i) {
            physical += 0.3f * (400 + 12 * output - physical);
            const float next = regulator.update(physical + (i % 2 ? 15 : -15), target, 1);
            assert(std::fabs(next - output) <= Config::RpmControl::MAX_DUTY_CHANGE_PER_SECOND + 0.001f);
            assert(next >= 0 && next <= 100);
            output = next;
        }
        assert(std::fabs(physical - target) < 35);
    }
    // Saturation must not store a large integral that delays subsequent reversal.
    for (int i = 0; i < 200; ++i) output = regulator.update(0, 1700, 1);
    assert(output == 100);
    for (int i = 0; i < 10; ++i) output = regulator.update(2000, 550, 1);
    assert(output < 95);
    const float previous = output;
    output = regulator.update(2000, 550, 600);
    assert(std::fabs(previous - output) <= Config::RpmControl::MAX_DUTY_CHANGE_PER_SECOND *
                                          Config::RpmControl::MAX_UPDATE_SECONDS + 0.001f);
    regulator.reset(42);
    for (int i = 0; i < 20; ++i) {
        assert(std::fabs(regulator.update(850 + (i % 2 ? 10 : -10), 850, 1) - 42) < 0.001f);
    }
}

void controlTests()
{
    fakeMillis = std::numeric_limits<uint32_t>::max() - 2000;
    FanController fan;
    assert(fan.begin());
    assert(fan.snapshot().mode == FanMode::Duty && fan.dutyPercent() == 35);
    fan.setDutyPercent(40);
    fan.toggleMode();
    assert(fan.snapshot().targetRpm == 850 && fan.dutyPercent() == 40);
    fan.adjustSetting(1, false);
    assert(fan.snapshot().targetRpm == 950);
    fan.adjustSetting(-1, true);
    assert(fan.snapshot().targetRpm == 940);
    for (int i = 0; i < 30; ++i) fan.adjustSetting(-1, false);
    assert(fan.snapshot().targetRpm == Config::RpmControl::MIN_RPM);
    for (int i = 0; i < 30; ++i) fan.adjustSetting(1, false);
    assert(fan.snapshot().targetRpm == Config::RpmControl::MAX_RPM);
    assert(!fan.setTargetRpm(0) && !fan.setTargetRpm(0xFFFFFFFF));
    assert(fan.setTargetRpm(850));
    sample(fan, 20); // 600 RPM, including millis rollover.
    assert(fan.dutyPercent() >= 40 && fan.dutyPercent() <= 42);
    float rpm;
    assert(!fan.updateRpm(rpm)); // No fresh sample: no feedback adjustment.
    for (int i = 0; i < 8; ++i) sample(fan, 20);
    assert(fan.dutyPercent() > 42 && fan.snapshot().manualDutyPercent == 40);
    fan.toggleMode();
    assert(fan.dutyPercent() == 40);
    assert(fakePwmValue == int(Config::Fan::PWM_MAX_VALUE - 40 * Config::Fan::PWM_MAX_VALUE / 100));
    fan.adjustSetting(1, false);
    fan.adjustSetting(-1, true);
    assert(fan.dutyPercent() == 44);
    fan.setTargetRpm(900);
    fan.setDutyPercent(27); // Existing HTTP/serial duty command explicitly selects manual.
    assert(fan.snapshot().mode == FanMode::Duty && fan.snapshot().targetRpm == 900);
}

void memoryTests()
{
    Preferences::data = {1, 2, 3}; // Wi-Fi namespace must remain intact.
    Preferences::records.clear();
    Preferences::writeCount = 0;
    fakeMillis = 0;
    FanController fan;
    OperatingMemory memory;
    memory.begin(fan);
    assert(fan.begin());
    memory.update(fan);
    assert(Preferences::writeCount == 0); // No boot-time default write.
    fan.setDutyPercent(47);
    memory.update(fan);
    assert(Preferences::writeCount == 0); // No per-button/PI writes.
    fan.setTargetRpm(970); // Exit Duty commits 47, not the active RPM edit.
    memory.update(fan);
    assert(Preferences::writeCount == 1 && Preferences::data == std::vector<uint8_t>({1, 2, 3}));
    {
        FanController boot;
        OperatingMemory reboot;
        reboot.begin(boot);
        assert(boot.begin());
        assert(boot.snapshot().mode == FanMode::Duty && boot.dutyPercent() == 47);
        assert(boot.snapshot().targetRpm == 850);
    }
    fan.setMode(FanMode::Duty); // Now save the RPM exit as well.
    fakeMillis += 1000;
    Preferences::writable = false;
    memory.update(fan);
    const size_t failedCount = Preferences::writeCount;
    memory.update(fan);
    assert(Preferences::writeCount == failedCount); // Retry is throttled.
    Preferences::writable = true;
    fakeMillis += 1000;
    memory.update(fan);
    {
        FanController boot;
        OperatingMemory reboot;
        reboot.begin(boot);
        assert(boot.begin());
        assert(boot.dutyPercent() == 47 && boot.snapshot().targetRpm == 970);
        boot.toggleMode();
        for (int i = 0; i < 20; ++i) sample(boot, 20);
        reboot.update(boot);
        const auto writes = Preferences::writeCount;
        boot.toggleMode(); // Unchanged setpoint exit requires no new write.
        fakeMillis += 1000;
        reboot.update(boot);
        assert(Preferences::writeCount == writes && boot.dutyPercent() == 47);
    }
    {
        const auto opens = Preferences::openCount, writes = Preferences::writeCount;
        FanController disabled;
        OperatingMemory off(false);
        off.begin(disabled);
        assert(disabled.begin());
        assert(disabled.dutyPercent() == 35 && disabled.snapshot().targetRpm == 850);
        disabled.setDutyPercent(60);
        disabled.toggleMode();
        off.update(disabled);
        disabled.toggleMode();
        assert(disabled.dutyPercent() == 60); // Session memory remains available.
        assert(Preferences::openCount == opens && Preferences::writeCount == writes);
    }
    {
        auto &record = Preferences::records["fanmaster-op/settings"];
        const uint32_t oldHighTarget = Config::RpmControl::MAX_RPM + 500;
        std::memcpy(record.data() + 4, &oldHighTarget, sizeof(oldHighTarget));
        const uint32_t crc = AssetFormat::crcUpdate(0xFFFFFFFF, record.data(), record.size() - 4) ^ 0xFFFFFFFF;
        std::memcpy(record.data() + record.size() - 4, &crc, sizeof(crc));
        FanController limited;
        OperatingMemory changedLimits;
        changedLimits.begin(limited);
        assert(limited.snapshot().targetRpm == Config::RpmControl::MAX_RPM);
    }
    Preferences::records["fanmaster-op/settings"][0] ^= 1;
    FanController corrupt;
    OperatingMemory corruptMemory;
    corruptMemory.begin(corrupt);
    assert(corrupt.begin());
    assert(corrupt.dutyPercent() == 35 && corrupt.snapshot().targetRpm == 850);
    Preferences::opens = false;
    OperatingMemory unavailable;
    FanController defaults;
    unavailable.begin(defaults);
    assert(defaults.begin() && defaults.dutyPercent() == 35);
    Preferences::opens = true;
}

void sampledPlantTest()
{
    fakeMillis = 0;
    FanController fan;
    assert(fan.begin());
    float physical = 820;
    for (uint32_t target : {850, 1400, 650}) {
        assert(fan.setTargetRpm(target));
        float sum = 0;
        for (int i = 0; i < 180; ++i) {
            physical += 0.3f * (400 + 12 * fan.dutyPercent() - physical);
            const unsigned pulses = static_cast<unsigned>(std::lround((physical + (i % 2 ? 15 : -15)) / 30));
            const int previous = fan.dutyPercent();
            sample(fan, pulses);
            assert(std::abs(int(fan.dutyPercent()) - previous) <= 2);
            if (i >= 150) sum += physical;
        }
        assert(std::fabs(sum / 30 - target) < 45);
    }
}

void serialTests()
{
    FanController fan;
    SerialConsole console;
    const std::string commands = "rpm 950\nmode duty\n41\nmode rpm\nrpm -1\nrpm 9999999999999999\n";
    Serial.incoming.assign(commands.begin(), commands.end());
    Serial.position = 0;
    console.update(fan);
    assert(fan.snapshot().mode == FanMode::Rpm && fan.snapshot().targetRpm == 950);
    fan.toggleMode();
    assert(fan.dutyPercent() == 41);
}

void tachNoiseTests()
{
    fakeMillis = 0;
    FanController fan;
    assert(fan.begin());
    assert(fan.setTargetRpm(850));
    sample(fan, 28); // Valid 840 RPM establishes the feedback state.
    const auto previous = fan.snapshot();
    for (int i = 0; i < 65000; ++i) fakeTachInterrupt();
    fakeMillis += 1000;
    float rpm = -1;
    assert(!fan.updateRpm(rpm)); // A supply transient must not update output/filter.
    assert(rpm == -1 && fan.currentRpm() == previous.rpm);
    assert(fan.dutyPercent() == previous.appliedDutyPercent);
    sample(fan, 28);
    assert(fan.currentRpm() == 840 && fan.dutyPercent() == previous.appliedDutyPercent);
    // Zero remains a valid stopped-fan sample; it is not hidden by the ceiling.
    sample(fan, 0);
    assert(fan.currentRpm() == 0);
}

int main()
{
    regulatorTests();
    controlTests();
    memoryTests();
    sampledPlantTest();
    serialTests();
    tachNoiseTests();
    std::cout << "RPM feedback, mode switching, limits, serial and NVS recovery tests passed\n";
}
