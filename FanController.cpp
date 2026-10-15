#include "FanController.h"

#include "Config.h"

static_assert(Config::RpmControl::MIN_RPM > 0 &&
              Config::RpmControl::MIN_RPM <= Config::RpmControl::DEFAULT_RPM &&
              Config::RpmControl::DEFAULT_RPM <= Config::RpmControl::MAX_RPM,
              "Default RPM must be inside the positive RPM range");
static_assert(Config::Fan::DEFAULT_DUTY_PERCENT <= 100, "Invalid default duty");
static_assert(Config::Fan::MAX_VALID_RPM >= Config::RpmControl::MAX_RPM,
              "TACH validity ceiling must cover the allowed RPM targets");
static_assert(Config::RpmControl::FILTER_ALPHA > 0 && Config::RpmControl::FILTER_ALPHA <= 1 &&
              Config::RpmControl::KP >= 0 && Config::RpmControl::KI > 0 &&
              Config::RpmControl::DEADBAND_RPM >= 0 &&
              Config::RpmControl::MAX_DUTY_CHANGE_PER_SECOND > 0 &&
              Config::RpmControl::MAX_UPDATE_SECONDS > 0,
              "Invalid regulator settings");
static_assert(Config::Buttons::RPM_STEP > 0 && Config::Buttons::MODIFIED_RPM_STEP > 0,
              "RPM button steps must be positive");

FanController *FanController::interruptOwner_ = nullptr;

bool FanController::begin()
{
    const bool pwmAttached = ledcAttach(
        Config::Pins::FAN_PWM,
        Config::Fan::PWM_FREQUENCY_HZ,
        Config::Fan::PWM_RESOLUTION_BITS
    );

    if (!pwmAttached) {
        return false;
    }

    applyDutyLocked(100);  // Startup only, before tasks start; never memorize the kick.
    delay(Config::Fan::STARTUP_TIME_MS);
    applyDutyLocked(manualDutyPercent_);

    // A 4.7k-10k external pull-up to 3.3V is recommended for TACH.
    pinMode(Config::Pins::FAN_TACH, INPUT_PULLUP);
    interruptOwner_ = this;
    attachInterrupt(
        digitalPinToInterrupt(Config::Pins::FAN_TACH),
        tachInterrupt,
        FALLING
    );

    lastRpmSampleMs_ = millis();
    return true;
}

bool FanController::setDutyPercent(uint8_t dutyPercent)
{
    // Serialize HTTP and local setters so the cached duty matches the PWM write.
    std::lock_guard<std::mutex> lock(dutyMutex_);
    if (mode_ == FanMode::Thermal) return false;
    setModeLocked(FanMode::Duty);
    manualDutyPercent_ = std::min<uint8_t>(dutyPercent, 100);
    applyDutyLocked(manualDutyPercent_);
    ++controlRevision_;
    return true;
}

void FanController::applyDutyLocked(uint8_t dutyPercent)
{
    if (dutyPercent > 100) {
        dutyPercent = 100;
    }

    const uint32_t fanDutyValue =
        (static_cast<uint32_t>(dutyPercent) * Config::Fan::PWM_MAX_VALUE) /
        100;

    const uint32_t gpioDutyValue = Config::Fan::PWM_INVERTED
        ? Config::Fan::PWM_MAX_VALUE - fanDutyValue
        : fanDutyValue;

    ledcWrite(Config::Pins::FAN_PWM, gpioDutyValue);
    currentDutyPercent_ = dutyPercent;
}

bool FanController::setTargetRpm(uint32_t rpm)
{
    if (rpm < Config::RpmControl::MIN_RPM || rpm > Config::RpmControl::MAX_RPM) return false;
    std::lock_guard<std::mutex> lock(dutyMutex_);
    if (mode_ == FanMode::Thermal) return false;
    setModeLocked(FanMode::Rpm);
    targetRpm_ = rpm;
    ++controlRevision_;
    return true;
}

void FanController::setModeLocked(FanMode mode)
{
    if (mode == mode_) return;
    if (mode_ == FanMode::Duty) {
        if (remembered_.dutyPercent != manualDutyPercent_) {
            remembered_.dutyPercent = manualDutyPercent_;
            ++settingsRevision_;
        }
    } else if (mode_ == FanMode::Rpm && remembered_.targetRpm != targetRpm_) {
        remembered_.targetRpm = targetRpm_;
        ++settingsRevision_;
    }
    if (mode == FanMode::Thermal) fixedMode_ = mode_;
    mode_ = mode;
    ++controlRevision_;
    if (mode_ == FanMode::Duty) applyDutyLocked(manualDutyPercent_);
    else if (mode_ == FanMode::Rpm) regulator_.reset(currentDutyPercent_);
}

bool FanController::setMode(FanMode mode)
{
    std::lock_guard<std::mutex> lock(dutyMutex_);
    if (mode_ == FanMode::Thermal || mode == FanMode::Thermal) return false;
    setModeLocked(mode);
    return true;
}

void FanController::toggleMode()
{
    std::lock_guard<std::mutex> lock(dutyMutex_);
    if (mode_ == FanMode::Thermal) {
        exitThermalLocked(ThermalExitReason::Manual);
        return; // This button-3 click only exits the override.
    }
    setModeLocked(mode_ == FanMode::Duty ? FanMode::Rpm : FanMode::Duty);
}

void FanController::adjustSetting(int direction, bool fine)
{
    std::lock_guard<std::mutex> lock(dutyMutex_);
    if (mode_ == FanMode::Thermal) return;
    const int sign = (direction > 0) - (direction < 0);
    if (mode_ == FanMode::Duty) {
        const int step = fine ? Config::Buttons::MODIFIED_DUTY_STEP_PERCENT
                              : Config::Buttons::DUTY_STEP_PERCENT;
        manualDutyPercent_ = static_cast<uint8_t>(std::clamp(int(manualDutyPercent_) + sign * step, 0, 100));
        applyDutyLocked(manualDutyPercent_);
    } else {
        const int64_t step = fine ? Config::Buttons::MODIFIED_RPM_STEP : Config::Buttons::RPM_STEP;
        targetRpm_ = static_cast<uint32_t>(std::clamp(int64_t(targetRpm_) + sign * step,
                            int64_t(Config::RpmControl::MIN_RPM), int64_t(Config::RpmControl::MAX_RPM)));
    }
    if (sign) ++controlRevision_;
}

bool FanController::temperatureReadyLocked(uint32_t now) const
{
    return thermalSample_.valid && std::isfinite(thermalSample_.temperatureC) &&
        now - thermalSample_.sampledAtMs <= Config::Thermal::SENSOR_STALE_MS;
}

void FanController::exitThermalLocked(ThermalExitReason reason)
{
    thermalExitReason_ = reason;
    setModeLocked(fixedMode_);
}

bool FanController::setThermalEnabled(bool enabled)
{
    std::lock_guard<std::mutex> lock(dutyMutex_);
    if (!enabled) {
        if (mode_ == FanMode::Thermal) exitThermalLocked(ThermalExitReason::Manual);
        return true;
    }
    if (!temperatureReadyLocked(millis())) return false;
    if (mode_ != FanMode::Thermal) setModeLocked(FanMode::Thermal);
    thermalExitReason_ = ThermalExitReason::None;
    applyDutyLocked(static_cast<uint8_t>(thermalCurve_.dutyAt(thermalSample_.temperatureC) + 0.5f));
    return true;
}

bool FanController::setThermalCurve(const ThermalCurve &curve)
{
    if (!curve.valid()) return false;
    std::lock_guard<std::mutex> lock(dutyMutex_);
    if (!(thermalCurve_ == curve)) {
        thermalCurve_ = curve;
        remembered_.thermalCurve = curve;
        ++settingsRevision_;
        ++controlRevision_;
    }
    if (mode_ == FanMode::Thermal) {
        if (!temperatureReadyLocked(millis())) exitThermalLocked(ThermalExitReason::Sensor);
        else applyDutyLocked(static_cast<uint8_t>(curve.dutyAt(thermalSample_.temperatureC) + 0.5f));
    }
    return true;
}

void FanController::updateTemperature(const ClimateSample &sample)
{
    std::lock_guard<std::mutex> lock(dutyMutex_);
    thermalSample_ = sample;
    if (mode_ != FanMode::Thermal) return;
    if (!temperatureReadyLocked(millis())) {
        exitThermalLocked(ThermalExitReason::Sensor);
    } else {
        const auto duty = static_cast<uint8_t>(thermalCurve_.dutyAt(sample.temperatureC) + 0.5f);
        if (duty != currentDutyPercent_) applyDutyLocked(duty);
    }
}

FanSnapshot FanController::snapshot() const
{
    std::lock_guard<std::mutex> lock(dutyMutex_);
    return {mode_, targetRpm_, manualDutyPercent_, currentDutyPercent_.load(), currentRpm_.load(),
            mode_ == FanMode::Thermal ? fixedMode_ : mode_, thermalCurve_,
            temperatureReadyLocked(millis()), thermalExitReason_, controlRevision_};
}

void FanController::restoreSettings(const OperatingSettings &settings)
{
    std::lock_guard<std::mutex> lock(dutyMutex_);
    manualDutyPercent_ = std::min<uint8_t>(settings.dutyPercent, 100);
    targetRpm_ = std::clamp(settings.targetRpm, Config::RpmControl::MIN_RPM, Config::RpmControl::MAX_RPM);
    thermalCurve_ = settings.thermalCurve.valid() ? settings.thermalCurve : ThermalCurve{};
    remembered_ = {targetRpm_, manualDutyPercent_, thermalCurve_};
}

uint32_t FanController::rememberedSettings(OperatingSettings &settings) const
{
    std::lock_guard<std::mutex> lock(dutyMutex_);
    settings = remembered_;
    return settingsRevision_;
}

bool FanController::updateRpm(float &rpmResult)
{
    const uint32_t now = millis();
    const uint32_t elapsedMs = now - lastRpmSampleMs_;

    if (elapsedMs < Config::Fan::RPM_SAMPLE_INTERVAL_MS) {
        return false;
    }

    uint32_t pulseSnapshot;
    portENTER_CRITICAL(&tachMux_);
    pulseSnapshot = rpmWindowPulseCount_;
    rpmWindowPulseCount_ = 0;
    portEXIT_CRITICAL(&tachMux_);

    lastRpmSampleMs_ = now;
    const float measuredRpm =
        (static_cast<float>(pulseSnapshot) * 60000.0f) /
        (static_cast<float>(elapsedMs) *
         static_cast<float>(Config::Fan::PULSES_PER_REVOLUTION));

    if (!std::isfinite(measuredRpm) || measuredRpm > Config::Fan::MAX_VALID_RPM) {
        // Advance/reset the window, but retain the last measurement and PI state.
        // A transient must not drive the integral or seed its low-pass filter.
        if (!Config::SerialConsole::PLOTTER_MODE) {
            Serial.print("[TACH] rejected implausible RPM sample: ");
            Serial.println(measuredRpm);
        }
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(dutyMutex_);
        currentRpm_ = measuredRpm;
        if (mode_ == FanMode::Rpm) {
            const float duty = regulator_.update(measuredRpm, targetRpm_, elapsedMs / 1000.0f);
            applyDutyLocked(static_cast<uint8_t>(duty + 0.5f));
        }
    }
    rpmResult = measuredRpm;
    return true;
}

uint8_t FanController::dutyPercent() const
{
    return currentDutyPercent_;
}

float FanController::currentRpm() const
{
    return currentRpm_;
}

void IRAM_ATTR FanController::tachInterrupt()
{
    if (interruptOwner_ != nullptr) {
        interruptOwner_->handleTachInterrupt();
    }
}

void IRAM_ATTR FanController::handleTachInterrupt()
{
    portENTER_CRITICAL_ISR(&tachMux_);
    rpmWindowPulseCount_++;
    portEXIT_CRITICAL_ISR(&tachMux_);
}
