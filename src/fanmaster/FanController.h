#pragma once

#include <Arduino.h>
#include <atomic>
#include <mutex>
#include "Config.h"
#include "RpmRegulator.h"
#include "ThermalCurve.h"
#include "ClimateReading.h"

enum class FanMode : uint8_t { Duty, Rpm, Thermal };
enum class ThermalExitReason : uint8_t { None, Manual, Sensor };

struct OperatingSettings {
    uint32_t targetRpm = Config::RpmControl::DEFAULT_RPM;
    uint8_t dutyPercent = Config::Fan::DEFAULT_DUTY_PERCENT;
    ThermalCurve thermalCurve;
};

struct FanSnapshot {
    FanMode mode;
    uint32_t targetRpm;
    uint8_t manualDutyPercent;
    uint8_t appliedDutyPercent;
    float rpm;
    FanMode fixedMode;
    ThermalCurve thermalCurve;
    bool thermalSensorReady;
    ThermalExitReason thermalExitReason;
    uint32_t controlRevision;
    bool powerEnabled;
    bool powerStarting;
    bool waitingForPowerKey;
};

class FanController {
public:
    bool begin();
    void initializePowerOutput();
    void setPowerEnabled(bool enabled);
    void updatePower();
    bool setDutyPercent(uint8_t dutyPercent);
    bool setTargetRpm(uint32_t rpm);
    bool setMode(FanMode mode);
    void toggleMode();
    void adjustSetting(int direction, bool fine);
    bool setThermalEnabled(bool enabled);
    bool setThermalCurve(const ThermalCurve &curve);
    void updateTemperature(const ClimateSample &sample);
    FanSnapshot snapshot() const;
    // Restore before begin(); only exited setpoints are exposed for NVS writes.
    void restoreSettings(const OperatingSettings &settings);
    uint32_t rememberedSettings(OperatingSettings &settings) const;
    bool updateRpm(float &rpmResult);

    uint8_t dutyPercent() const;
    float currentRpm() const;

private:
    void applyDutyLocked(uint8_t dutyPercent);
    void setModeLocked(FanMode mode);
    bool temperatureReadyLocked(uint32_t now) const;
    void exitThermalLocked(ThermalExitReason reason);
    static void IRAM_ATTR tachInterrupt();
    void IRAM_ATTR handleTachInterrupt();

    static FanController *interruptOwner_;

    portMUX_TYPE tachMux_ = portMUX_INITIALIZER_UNLOCKED;
    volatile uint32_t rpmWindowPulseCount_ = 0;

    uint32_t lastRpmSampleMs_ = 0;
    std::atomic<float> currentRpm_{0.0f};
    std::atomic<uint8_t> currentDutyPercent_{0};
    mutable std::mutex dutyMutex_;
    FanMode mode_ = FanMode::Duty;
    FanMode fixedMode_ = FanMode::Duty;
    uint8_t manualDutyPercent_ = Config::Fan::DEFAULT_DUTY_PERCENT;
    uint32_t targetRpm_ = Config::RpmControl::DEFAULT_RPM;
    OperatingSettings remembered_;
    uint32_t settingsRevision_ = 0;
    RpmRegulator regulator_;
    ThermalCurve thermalCurve_;
    ClimateSample thermalSample_;
    ThermalExitReason thermalExitReason_ = ThermalExitReason::None;
    uint32_t controlRevision_ = 0;
    bool powerEnabled_ = false;
    bool powerStarting_ = false;
    bool waitingForPowerKey_ = true;
    uint32_t powerStartedAtMs_ = 0;
    uint8_t resumeDutyPercent_ = Config::Fan::DEFAULT_DUTY_PERCENT;
};
