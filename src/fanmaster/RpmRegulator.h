#pragma once

#include <algorithm>
#include <cmath>
#include "Config.h"

// Called only for fresh TACH samples, never for repeated HTTP/OLED reads.
class RpmRegulator {
public:
    void reset(float duty) {
        output_ = duty;
        initialized_ = false;
    }

    float update(float measuredRpm, float targetRpm, float seconds) {
        seconds = std::clamp(seconds, 0.0f, Config::RpmControl::MAX_UPDATE_SECONDS);
        if (!initialized_) {
            filtered_ = measuredRpm;
            // Seed from the existing PWM output for a bumpless mode change.
            integral_ = output_ - Config::RpmControl::KP * error(targetRpm);
            initialized_ = true;
        } else {
            filtered_ += Config::RpmControl::FILTER_ALPHA * (measuredRpm - filtered_);
        }
        const float e = error(targetRpm);
        const float proportional = Config::RpmControl::KP * e;
        const float requested = proportional + integral_ + Config::RpmControl::KI * e * seconds;
        const float maxChange = Config::RpmControl::MAX_DUTY_CHANGE_PER_SECOND * seconds;
        output_ = std::clamp(std::clamp(requested, output_ - maxChange, output_ + maxChange),
                             0.0f, 100.0f);
        // Tracking anti-windup for both PWM saturation and the slew limiter.
        integral_ = output_ - proportional;
        return output_;
    }

    float filteredRpm() const { return filtered_; }

private:
    float error(float target) const {
        const float e = target - filtered_;
        // Continuous dead zone avoids a discontinuity at the tolerance boundary.
        if (std::fabs(e) <= Config::RpmControl::DEADBAND_RPM) return 0.0f;
        return e > 0 ? e - Config::RpmControl::DEADBAND_RPM
                     : e + Config::RpmControl::DEADBAND_RPM;
    }
    float filtered_ = 0, integral_ = 0, output_ = 0;
    bool initialized_ = false;
};
