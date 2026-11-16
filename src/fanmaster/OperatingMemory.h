#pragma once

#include <Preferences.h>
#include "FanController.h"

// Load once before fan.begin(); thereafter only the network task uses this
// handle. No flash writes while holding FanController's mutex or in its loop.
class OperatingMemory {
public:
    explicit OperatingMemory(bool enabled = Config::OperatingMemory::ENABLED) : enabled_(enabled) {}
    void begin(FanController &fan);
    void update(FanController &fan);

private:
    Preferences preferences_;
    bool enabled_, ready_ = false, attempted_ = false;
    uint32_t savedRevision_ = 0, lastAttemptMs_ = 0;
};
