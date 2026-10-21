#include "StartFan.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <limits>
#include <iostream>
#include "FanController.h"
#include "OperatingMemory.h"
#include "AssetFormat.h"

void curveTests()
{
    ThermalCurve c;
    assert(c.valid());
    for (const auto &p : c.points) assert(std::fabs(c.dutyAt(p.temperatureDeciC / 10.0f) - p.dutyPercent) < .001f);
    assert(c.dutyAt(-10) == 35 && c.dutyAt(100) == 90);
    assert(std::fabs(c.dutyAt(32.5f) - 50) < .001f);
    assert(std::fabs(c.dutyAt(60) - 77.5f) < .001f);
    c.points[1] = {251, 65}; // Smallest allowed temperature spacing.
    assert(c.valid() && std::fabs(c.dutyAt(25.05f) - 50) < .01f);
    c.points[1].temperatureDeciC = 250;
    assert(!c.valid());
    c = ThermalCurve{}; c.points[1].dutyPercent = 34; assert(!c.valid());
    c = ThermalCurve{}; c.points[0].temperatureDeciC = 149; assert(!c.valid());
    c = ThermalCurve{}; c.points[2].temperatureDeciC = 951; assert(!c.valid());
    c = ThermalCurve{}; c.points[2].dutyPercent = 101; assert(!c.valid());
    c = ThermalCurve{}; c.points[1].dutyPercent = 35;
    assert(c.valid() && c.dutyAt(30) == 35);
}

void controlTests()
{
    fakeMillis = 0;
    FanController fan;
    assert(fan.begin()); startFan(fan);
    assert(!fan.setThermalEnabled(true));
    fan.setDutyPercent(42);
    fan.updateTemperature({true, 32.5f, 55, millis()});
    assert(fan.setThermalEnabled(true));
    assert(fan.snapshot().mode == FanMode::Thermal && fan.dutyPercent() == 50);
    assert(!fan.setDutyPercent(10) && !fan.setTargetRpm(900) && !fan.setMode(FanMode::Rpm));
    fan.adjustSetting(1, false); fan.adjustSetting(-1, true);
    assert(fan.snapshot().manualDutyPercent == 42 && fan.snapshot().targetRpm == 850 && fan.dutyPercent() == 50);
    // RPM feedback must not overwrite curve output.
    fakeMillis += 1000;
    float rpm;
    assert(fan.updateRpm(rpm) && fan.dutyPercent() == 50);
    ThermalCurve c;
    c.points[0].dutyPercent = 45;
    assert(fan.setThermalCurve(c) && fan.dutyPercent() == 55);
    auto invalid = c; invalid.points[1].temperatureDeciC = 249;
    assert(!fan.setThermalCurve(invalid) && fan.snapshot().thermalCurve == c);
    fan.toggleMode(); // Button 3 exits, does not also select RPM.
    assert(fan.snapshot().mode == FanMode::Duty && fan.dutyPercent() == 42);
    assert(fan.setTargetRpm(970));
    fan.updateTemperature({true, 90, 55, millis()});
    assert(fan.setThermalEnabled(true) && fan.dutyPercent() == 90);
    assert(fan.setThermalEnabled(false));
    assert(fan.snapshot().mode == FanMode::Rpm && fan.snapshot().targetRpm == 970);
    fan.toggleMode(); assert(fan.dutyPercent() == 42);
    // Failures restore fixed immediately; a later valid sample never re-enables THERMAL.
    fan.setThermalEnabled(true);
    fan.updateTemperature({});
    assert(fan.snapshot().mode == FanMode::Duty && fan.snapshot().thermalExitReason == ThermalExitReason::Sensor);
    fan.updateTemperature({true, 40, 55, millis()});
    assert(fan.snapshot().mode == FanMode::Duty);
    assert(fan.setThermalEnabled(true));
    fan.updateTemperature({true, std::numeric_limits<float>::quiet_NaN(), 55, millis()});
    assert(fan.snapshot().mode == FanMode::Duty);
    // Stale samples, including wrapping millis(), and denied re-entry.
    fakeMillis = std::numeric_limits<uint32_t>::max() - 500;
    ClimateSample sample{true, 60, 55, millis()};
    fan.updateTemperature(sample);
    assert(fan.setThermalEnabled(true));
    fakeMillis += Config::Thermal::SENSOR_STALE_MS;
    fan.updateTemperature(sample);
    assert(fan.snapshot().mode == FanMode::Thermal);
    ++fakeMillis;
    fan.updateTemperature(sample);
    assert(fan.snapshot().mode == FanMode::Duty && fan.dutyPercent() == 42);
    assert(!fan.setThermalEnabled(true));
}

void memoryTests()
{
    // Read a real V1 layout without losing the prior presets, then save as V2.
    struct Legacy { uint32_t magic, rpm; uint8_t duty, reserved[3]; uint32_t crc; };
    Legacy legacy{0x314F4D46, 930, 43, {}, 0};
    static_assert(sizeof(legacy) == 16);
    legacy.crc = AssetFormat::crcUpdate(0xFFFFFFFF, &legacy, 12) ^ 0xFFFFFFFF;
    auto &bytes = Preferences::records["fanmaster-op/settings"];
    bytes.resize(sizeof(legacy)); std::memcpy(bytes.data(), &legacy, sizeof(legacy));
    Preferences::data = {1,2,3};
    FanController fan;
    OperatingMemory memory;
    memory.begin(fan);
    assert(fan.begin()); startFan(fan);
    assert(fan.dutyPercent() == 43 && fan.snapshot().targetRpm == 930);
    ThermalCurve c;
    c.points[0] = {253, 37};
    assert(fan.setThermalCurve(c));
    memory.update(fan);
    assert(bytes.size() == 28 && Preferences::data == std::vector<uint8_t>({1,2,3}));
    FanController boot;
    OperatingMemory reboot;
    reboot.begin(boot);
    assert(boot.snapshot().mode == FanMode::Duty && boot.snapshot().thermalCurve == c);
    assert(boot.snapshot().manualDutyPercent == 43 && boot.snapshot().targetRpm == 930);
    auto count = Preferences::writeCount;
    boot.updateTemperature({true, 50, 55, millis()});
    boot.setThermalEnabled(true);
    boot.updateTemperature({true, 60, 55, millis()});
    reboot.update(boot);
    assert(Preferences::writeCount == count); // Sensor output never writes NVS.
    OperatingMemory disabled(false);
    FanController off;
    disabled.begin(off);
    assert(off.snapshot().thermalCurve == ThermalCurve{});
    off.setThermalCurve(c);
    disabled.update(off);
    assert(Preferences::writeCount == count);
    // A valid CRC cannot make an invalid curve acceptable.
    bytes[12] = 0; bytes[13] = 0;
    uint32_t crc = AssetFormat::crcUpdate(0xFFFFFFFF, bytes.data(), 24) ^ 0xFFFFFFFF;
    std::memcpy(bytes.data() + 24, &crc, 4);
    FanController corrupt;
    OperatingMemory invalidMemory;
    invalidMemory.begin(corrupt);
    assert(corrupt.snapshot().thermalCurve == ThermalCurve{} && corrupt.snapshot().manualDutyPercent == 35);
}

int main()
{
    curveTests(); controlTests(); memoryTests();
    std::cout << "Thermal interpolation, override, sensor fallback, rollover and NVS migration tests passed\n";
}
