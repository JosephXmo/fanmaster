#pragma once

#include <cmath>
#include <cstddef>
#include "Config.h"

struct ThermalPoint {
    uint16_t temperatureDeciC;
    uint8_t dutyPercent;
};

struct ThermalCurve {
    ThermalPoint points[3] = {
        {Config::Thermal::DEFAULT_TEMPERATURES_DECI_C[0], Config::Thermal::DEFAULT_DUTIES[0]},
        {Config::Thermal::DEFAULT_TEMPERATURES_DECI_C[1], Config::Thermal::DEFAULT_DUTIES[1]},
        {Config::Thermal::DEFAULT_TEMPERATURES_DECI_C[2], Config::Thermal::DEFAULT_DUTIES[2]},
    };

    constexpr bool valid() const {
        for (size_t i = 0; i < 3; ++i) {
            const auto &p = points[i];
            if (p.temperatureDeciC < Config::Thermal::MIN_TEMPERATURE_DECI_C ||
                p.temperatureDeciC > Config::Thermal::MAX_TEMPERATURE_DECI_C || p.dutyPercent > 100)
                return false;
            if (i && (p.temperatureDeciC <= points[i-1].temperatureDeciC ||
                      p.dutyPercent < points[i-1].dutyPercent)) return false;
        }
        return true;
    }

    bool operator==(const ThermalCurve &other) const {
        for (size_t i = 0; i < 3; ++i)
            if (points[i].temperatureDeciC != other.points[i].temperatureDeciC ||
                points[i].dutyPercent != other.points[i].dutyPercent) return false;
        return true;
    }

    float dutyAt(float temperatureC) const {
        const float t = temperatureC * 10;
        if (t <= points[0].temperatureDeciC) return points[0].dutyPercent;
        for (size_t i = 1; i < 3; ++i) {
            if (t <= points[i].temperatureDeciC) {
                const auto &a = points[i-1];
                const auto &b = points[i];
                return a.dutyPercent + (b.dutyPercent - a.dutyPercent) *
                    (t - a.temperatureDeciC) / (b.temperatureDeciC - a.temperatureDeciC);
            }
        }
        return points[2].dutyPercent;
    }
};

static_assert(ThermalCurve{}.valid(), "Default thermal curve must have increasing temperatures and nondecreasing duty");
static_assert(Config::Thermal::SENSOR_STALE_MS > 0 && Config::Thermal::SENSOR_STALE_MS < 0x80000000UL,
              "Thermal sensor timeout must be positive and rollover-safe");
