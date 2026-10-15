#pragma once
#include <mutex>

struct ClimateSample {
    bool valid = false;
    float temperatureC = 0.0f;
    float humidityPercent = 0.0f;
};

// HTTP runs on another task. Publish/read the whole sample coherently, without
// holding this lock during sensor I/O, formatting, or socket writes.
class ClimateReading {
public:
    void publish(const ClimateSample &sample) {
        std::lock_guard<std::mutex> lock(mutex_);
        sample_ = sample;
    }
    ClimateSample snapshot() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return sample_;
    }
private:
    mutable std::mutex mutex_;
    ClimateSample sample_;
};
